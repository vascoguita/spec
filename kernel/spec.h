/*
 * Copyright (C) 2010-2018 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __SPEC_H__
#define __SPEC_H__
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/i2c.h>
#include <linux/irqdomain.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define SPEC_FMC_SLOTS 1

/* On FPGA components */
#define SPEC_I2C_MASTER_ADDR	0x0
#define SPEC_I2C_MASTER_SIZE	8

/* These must be set to choose the FPGA configuration mode */
#define GPIO_BOOTSEL0 15
#define GPIO_BOOTSEL1 14

#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
#define PCI_DEVICE_ID_SPEC_150T (0x01A3)
#define PCI_VENDOR_ID_GENNUM    (0x1A39)
#define PCI_DEVICE_ID_GN4124    (0x0004)

#define SPEC_MINOR_MAX (64)
#define SPEC_FLAG_BITS (8)
#define SPEC_FLAG_UNLOCK BIT(0)


/* Registers for GN4124 access */
enum {
	/* page 106 */
	GNPPCI_MSI_CONTROL	= 0x48,		/* actually, 3 smaller regs */
	GNPPCI_MSI_ADDRESS_LOW	= 0x4c,
	GNPPCI_MSI_ADDRESS_HIGH	= 0x50,
	GNPPCI_MSI_DATA		= 0x54,

	GNPCI_SYS_CFG_SYSTEM	= 0x800,

	/* page 130 ff */
	GNINT_CTRL		= 0x810,
	GNINT_STAT		= 0x814,
	GNINT_CFG_0		= 0x820,
	GNINT_CFG_1		= 0x824,
	GNINT_CFG_2		= 0x828,
	GNINT_CFG_3		= 0x82c,
	GNINT_CFG_4		= 0x830,
	GNINT_CFG_5		= 0x834,
	GNINT_CFG_6		= 0x838,
	GNINT_CFG_7		= 0x83c,
#define GNINT_CFG(x) (GNINT_CFG_0 + 4 * (x))

	/* page 146 ff */
	GNGPIO_BASE = 0xA00,
	GNGPIO_BYPASS_MODE	= GNGPIO_BASE,
	GNGPIO_DIRECTION_MODE	= GNGPIO_BASE + 0x04, /* 0 == output */
	GNGPIO_OUTPUT_ENABLE	= GNGPIO_BASE + 0x08,
	GNGPIO_OUTPUT_VALUE	= GNGPIO_BASE + 0x0C,
	GNGPIO_INPUT_VALUE	= GNGPIO_BASE + 0x10,
	GNGPIO_INT_MASK	= GNGPIO_BASE + 0x14, /* 1 == disabled */
	GNGPIO_INT_MASK_CLR	= GNGPIO_BASE + 0x18, /* irq enable */
	GNGPIO_INT_MASK_SET	= GNGPIO_BASE + 0x1C, /* irq disable */
	GNGPIO_INT_STATUS	= GNGPIO_BASE + 0x20,
	GNGPIO_INT_TYPE	= GNGPIO_BASE + 0x24, /* 1 == level */
	GNGPIO_INT_VALUE	= GNGPIO_BASE + 0x28, /* 1 == high/rise */
	GNGPIO_INT_ON_ANY	= GNGPIO_BASE + 0x2C, /* both edges */

	/* page 158 ff */
	FCL_BASE		= 0xB00,
	FCL_CTRL		= FCL_BASE,
	FCL_STATUS		= FCL_BASE + 0x04,
	FCL_IODATA_IN		= FCL_BASE + 0x08,
	FCL_IODATA_OUT		= FCL_BASE + 0x0C,
	FCL_EN			= FCL_BASE + 0x10,
	FCL_TIMER_0		= FCL_BASE + 0x14,
	FCL_TIMER_1		= FCL_BASE + 0x18,
	FCL_CLK_DIV		= FCL_BASE + 0x1C,
	FCL_IRQ			= FCL_BASE + 0x20,
	FCL_TIMER_CTRL		= FCL_BASE + 0x24,
	FCL_IM			= FCL_BASE + 0x28,
	FCL_TIMER2_0		= FCL_BASE + 0x2C,
	FCL_TIMER2_1		= FCL_BASE + 0x30,
	FCL_DBG_STS		= FCL_BASE + 0x34,

	FCL_FIFO		= 0xE00,

	PCI_SYS_CFG_SYSTEM	= 0x800
};


#define GNINT_STAT_GPIO BIT(15)
#define GNINT_STAT_SW0 BIT(2)
#define GNINT_STAT_SW1 BIT(3)
#define GNINT_STAT_SW_ALL (GNINT_STAT_SW0 | GNINT_STAT_SW1)

/**
 * struct spec_dev - SPEC instance
 * It describes a SPEC device instance.
 * @dev Linux device instance descriptor
 * @gpio_domain: IRQ domain for GN4124 chip
 * @flags collection of bit flags
 * @remap ioremap of PCI bar 0, 2, 4
 * @slot_info: information about FMC slot
 * @i2c_pdev: platform device for I2C master
 * @i2c_adapter: the I2C master device to be used
 * @compl: for IRQ testing
 */
struct spec_dev {
	struct device dev;

	struct irq_domain *gpio_domain;

	struct fpga_manager *mgr;

	DECLARE_BITMAP(flags, SPEC_FLAG_BITS);
	void __iomem *remap[3];	/* ioremap of bar 0, 2, 4 */

	struct dentry *dbg_dir;
#define SPEC_DBG_INFO_NAME "info"
	struct dentry *dbg_info;

	struct completion	compl;
};


static inline struct spec_dev *to_spec_dev(struct device *_dev)
{
	return container_of(_dev, struct spec_dev, dev);
}

/**
 * It reads a 32bit register from the gennum chip
 * @spec spec device instance
 * @reg gennum register offset
 * Return: a 32bit value
 */
static inline uint32_t gennum_readl(struct spec_dev *spec, int reg)
{
	return readl(spec->remap[2] + reg);
}


/**
 * It writes a 32bit register to the gennum chip
 * @spec spec device instance
 * @val a 32bit valure
 * @reg gennum register offset
 */
static inline void gennum_writel(struct spec_dev *spec, uint32_t val, int reg)
{
	writel(val, spec->remap[2] + reg);
}


/**
 * It writes a 32bit register to the gennum chip according to the given mask
 * @spec spec device instance
 * @mask bit mask of the bits to actually write
 * @val a 32bit valure
 * @reg gennum register offset
 */
static inline void gennum_mask_val(struct spec_dev *spec,
				   uint32_t mask, uint32_t val, int reg)
{
	uint32_t v = gennum_readl(spec, reg);
	v &= ~mask;
	v |= val;
	gennum_writel(spec, v, reg);
}


extern int spec_fpga_init(struct spec_dev *spec);
extern void spec_fpga_exit(struct spec_dev *spec);

extern int spec_irq_init(struct spec_dev *spec);
extern void spec_irq_exit(struct spec_dev *spec);

#endif /* __SPEC_H__ */
