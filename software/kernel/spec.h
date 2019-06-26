/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2010-2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 */
#ifndef __SPEC_H__
#define __SPEC_H__
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/irqdomain.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/fmc.h>

#include "gn412x.h"

#define SPEC_FMC_SLOTS 1

/* On FPGA components */
#define SPEC_I2C_MASTER_ADDR	0x0
#define SPEC_I2C_MASTER_SIZE	8

#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
#define PCI_DEVICE_ID_SPEC_150T (0x01A3)
#define PCI_VENDOR_ID_GENNUM    (0x1A39)
#define PCI_DEVICE_ID_GN4124    (0x0004)

#define SPEC_MINOR_MAX (64)
#define SPEC_FLAG_BITS (8)
#define SPEC_FLAG_THERM_BIT 0

#define GN4124_GPIO_MAX 16
#define GN4124_GPIO_BOOTSEL0 15
#define GN4124_GPIO_BOOTSEL1 14
#define GN4124_GPIO_SPRI_DIN 13
#define GN4124_GPIO_SPRI_FLASH_CS 12
#define GN4124_GPIO_IRQ0 9
#define GN4124_GPIO_IRQ1 8
#define GN4124_GPIO_SCL 5
#define GN4124_GPIO_SDA 4

/**
 * @SPEC_FPGA_SELECT_FLASH: (default) the FPGA takes its configuration from
 *                          flash
 * @SPEC_FPGA_SELECT_GN4124: the FPGA takes its configuration from GN4124
 * @SPEC_FPGA_SELECT_SPI: the SPI flash is accessible from GN4124
 */
enum spec_fpga_select {
	SPEC_FPGA_SELECT_FLASH = 0,
	SPEC_FPGA_SELECT_GN4124,
	SPEC_FPGA_SELECT_SPI,
};


enum {
	/* Metadata */
	SPEC_CORE_FPGA = 0x0,
	SPEC_META_BASE = SPEC_CORE_FPGA + 0x00,
	SPEC_META_VENDOR = SPEC_META_BASE + 0x00,
	SPEC_META_DEVICE = SPEC_META_BASE + 0x04,
	SPEC_META_VERSION = SPEC_META_BASE + 0x08,
	SPEC_META_BOM = SPEC_META_BASE + 0x0C,
	SPEC_META_SRC = SPEC_META_BASE + 0x10,
	SPEC_META_CAP = SPEC_META_BASE + 0x20,
	SPEC_META_UUID = SPEC_META_BASE + 0x30,
};

#define SPEC_META_VENDOR_ID PCI_VENDOR_ID_CERN
#define SPEC_META_DEVICE_ID 0x53504543
#define SPEC_META_BOM_LE 0xFFFE0000
#define SPEC_META_BOM_END_MASK 0xFFFF0000
#define SPEC_META_BOM_VER_MASK 0x0000FFFF
#define SPEC_META_VERSION_1_4 0x01040000
#define SPEC_META_CAP_VIC BIT(0)
#define SPEC_META_CAP_THERM BIT(0)
#define SPEC_META_CAP_SPI BIT(0)
#define SPEC_META_CAP_DMA BIT(0)
#define SPEC_META_CAP_WR BIT(0)

/**
 * struct spec_meta_id Metadata
 */
struct spec_meta_id {
	uint32_t vendor;
	uint32_t device;
	uint32_t version;
	uint32_t bom;
	uint32_t src[4];
	uint32_t cap;
	uint32_t uuid[4];
};

/**
 * struct spec_dev - SPEC instance
 * It describes a SPEC device instance.
 * @dev Linux device instance descriptor
 * @flags collection of bit flags
 * @remap ioremap of PCI bar 0, 2, 4
 * @slot_info: information about FMC slot
 * @i2c_pdev: platform device for I2C master
 * @i2c_adapter: the I2C master device to be used
 */
struct spec_dev {
	struct device dev;

	DECLARE_BITMAP(flags, SPEC_FLAG_BITS);
	void __iomem *remap[3];	/* ioremap of bar 0, 2, 4 */
	void __iomem *fpga;
	struct spec_meta_id __iomem *meta;

	struct platform_device *vic_pdev;
	struct platform_device *app_pdev;

	struct fmc_slot_info slot_info;

	struct dentry *dbg_dir;
#define SPEC_DBG_INFO_NAME "info"
	struct dentry *dbg_info;
#define SPEC_DBG_FW_NAME "fpga_firmware"
	struct dentry *dbg_fw;
#define SPEC_DBG_META_NAME "fpga_device_metadata"
	struct dentry *dbg_meta;

	struct gpiod_lookup_table *gpiod_table;
	struct gpio_desc *gpiod[GN4124_GPIO_MAX];
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

extern int spec_fw_load(struct spec_dev *spec, const char *name);

extern int spec_irq_init(struct spec_dev *spec);
extern void spec_irq_exit(struct spec_dev *spec);

extern int spec_dbg_init(struct spec_dev *spec);
extern void spec_dbg_exit(struct spec_dev *spec);

extern int spec_fmc_init(struct spec_dev *spec);
extern int spec_fmc_exit(struct spec_dev *spec);

extern void spec_gpio_fpga_select(struct spec_dev *spec,
				  enum spec_fpga_select sel);
extern int spec_gpio_init(struct spec_dev *spec);
extern void spec_gpio_exit(struct spec_dev *spec);

extern int spec_core_fpga_init(struct spec_dev *spec);
extern int spec_core_fpga_exit(struct spec_dev *spec);

#endif /* __SPEC_H__ */
