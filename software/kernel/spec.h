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
#include <linux/mutex.h>
#include <linux/fmc.h>

#include "gn412x.h"

#define SPEC_FMC_SLOTS 1

/* On FPGA components */
#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
#define PCI_DEVICE_ID_SPEC_150T (0x01A3)
#define PCI_VENDOR_ID_GENNUM    (0x1A39)
#define PCI_DEVICE_ID_GN4124    (0x0004)

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
 * @SPEC_FPGA_SELECT_FPGA_FLASH: (default) the FPGA is an SPI master that can
 *                               access the flash (at boot it takes its
 *                               configuration from flash)
 * @SPEC_FPGA_SELECT_GN4124_FPGA: the GN4124 can configure the FPGA
 * @SPEC_FPGA_SELECT_GN4124_FLASH: the GN4124 is an SPI master that can access
 *                                 the flash
 */
enum spec_fpga_select {
	SPEC_FPGA_SELECT_FPGA_FLASH = 0x3,
	SPEC_FPGA_SELECT_GN4124_FPGA = 0x1,
	SPEC_FPGA_SELECT_GN4124_FLASH = 0x0,
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
#define SPEC_META_VERSION_MASK 0xFFFF0000
#define SPEC_META_VERSION_1_4 0x01040000

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
 * struct spec_fpga - it contains data to handle the FPGA
 *
 * @pdev: pointer to the PCI device
 * @fpga:
 * @meta:
 * @vic_pdev:
 * @app_pdev:
 * @slot_info:
 * @dbg_dir_fpga:
 * @dbg_csr:
 * @dbg_csr_reg:
 */
struct spec_fpga {
	struct device dev;
	void __iomem *fpga;
	struct spec_meta_id __iomem *meta;
	struct platform_device *vic_pdev;
	struct platform_device *app_pdev;
	struct fmc_slot_info slot_info;
	struct dentry *dbg_dir_fpga;
#define SPEC_DBG_CSR_NAME "csr_regs"
	struct dentry *dbg_csr;
	struct debugfs_regset32 dbg_csr_reg;
#define SPEC_DBG_BLD_INFO_NAME "build_info"
	struct dentry *dbg_bld_info;
};

/**
 * struct spec_gn412x - it contains data to handle the PCB
 *
 * @pdev: pointer to the PCI device
 * @mtx: it protects FPGA device/configuration loading
 */
struct spec_gn412x {
	struct pci_dev *pdev;
	struct mutex mtx;
	struct gpiod_lookup_table *gpiod_table;
	struct gpio_desc *gpiod[GN4124_GPIO_MAX];
	struct dentry *dbg_dir;
#define SPEC_DBG_INFO_NAME "info"
	struct dentry *dbg_info;
#define SPEC_DBG_FW_NAME "fpga_firmware"
	struct dentry *dbg_fw;
#define SPEC_DBG_META_NAME "fpga_device_metadata"
	struct dentry *dbg_meta;
	struct spec_fpga *spec_fpga;
};

static inline struct spec_fpga *to_spec_fpga(struct device *_dev)
{
	return container_of(_dev, struct spec_fpga, dev);
}

extern int spec_fpga_init(struct spec_gn412x *spec_gn412x);
extern int spec_fpga_exit(struct spec_gn412x *spec_gn412x);

#endif /* __SPEC_H__ */
