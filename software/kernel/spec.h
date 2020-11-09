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
#include <uapi/linux/spec.h>

#include "gn412x.h"
#include "spec-core-fpga.h"

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
	FPGA_META_VENDOR = 0x00,
	FPGA_META_DEVICE = 0x04,
	FPGA_META_VERSION = 0x08,
	FPGA_META_BOM = 0x0C,
	FPGA_META_SRC = 0x10,
	FPGA_META_CAP = 0x20,
	FPGA_META_UUID = 0x30,

};

enum {
	/* Metadata */
	SPEC_META_BASE = SPEC_BASE_REGS_METADATA,
	SPEC_META_VENDOR = SPEC_META_BASE + FPGA_META_VENDOR,
	SPEC_META_DEVICE = SPEC_META_BASE + FPGA_META_DEVICE,
	SPEC_META_VERSION = SPEC_META_BASE + FPGA_META_VERSION,
	SPEC_META_BOM = SPEC_META_BASE + FPGA_META_BOM,
	SPEC_META_SRC = SPEC_META_BASE + FPGA_META_SRC,
	SPEC_META_CAP = SPEC_META_BASE + FPGA_META_CAP,
	SPEC_META_UUID = SPEC_META_BASE + FPGA_META_UUID,
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
	struct platform_device *dma_pdev;
	struct platform_device *app_pdev;
	struct fmc_slot_info slot_info;
	struct dentry *dbg_dir_fpga;
#define SPEC_DBG_CSR_NAME "csr_regs"
	struct dentry *dbg_csr;
	struct debugfs_regset32 dbg_csr_reg;
#define SPEC_DBG_BLD_INFO_NAME "build_info"
	struct dentry *dbg_bld;
#define SPEC_DBG_DMA_NAME "dma"
	struct dentry *dbg_dma;
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
