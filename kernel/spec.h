/*
 * Copyright (C) 2010-2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __SPEC_H__
#define __SPEC_H__
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/spinlock.h>


#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
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
	GNGPIO_INT_MASK		= GNGPIO_BASE + 0x14, /* 1 == disabled */
	GNGPIO_INT_MASK_CLR	= GNGPIO_BASE + 0x18, /* irq enable */
	GNGPIO_INT_MASK_SET	= GNGPIO_BASE + 0x1C, /* irq disable */
	GNGPIO_INT_STATUS	= GNGPIO_BASE + 0x20,
	GNGPIO_INT_TYPE		= GNGPIO_BASE + 0x24, /* 1 == level */
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

/**
 * struct spec_dev - SPEC instance
 * It describes a SPEC device instance.
 * @cdev Char device descriptor
 * @dev Linux device instance descriptor
 * @flags collection of bit flags
 * @remap ioremap of PCI bar 0, 2, 4
 * @lock it protects: flags
 * @buf buffer where store FPGA bitstreams
 * @size buffer size
 */
struct spec_dev {
	struct cdev cdev;
	struct device dev;

	DECLARE_BITMAP(flags, SPEC_FLAG_BITS);
	void __iomem *remap[3];	/* ioremap of bar 0, 2, 4 */

	struct spinlock lock;

	void *buf;
	size_t size;
};


/**
 * It gets a SPEC device instance
 * @ptr pointer to a Linux device instance
 * Return: the SPEC device instance correponding to the given Linux device
 */
static inline struct spec_dev *to_spec_dev(struct device *ptr)
{
	return container_of(ptr, struct spec_dev, dev);
}

#endif /* __SPEC_H__ */
