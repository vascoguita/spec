// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/types.h>
#include <linux/platform_data/i2c-ocores.h>
#include <linux/platform_data/spi-ocores.h>
#include <linux/ioport.h>
#include <linux/gpio/consumer.h>
#include <linux/irqdomain.h>
#include <linux/dmaengine.h>
#include <linux/mfd/core.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/bitops.h>
#include <linux/fmc.h>
#include <linux/delay.h>

#include "spec.h"
#include "spec-compat.h"


enum spec_fpga_irq_lines {
	SPEC_FPGA_IRQ_FMC_I2C = 0,
	SPEC_FPGA_IRQ_SPI,
	SPEC_FPGA_IRQ_DMA_DONE,
};

enum spec_fpga_csr_offsets {
	SPEC_FPGA_CSR_APP_OFF = SPEC_BASE_REGS_CSR + 0x00,
	SPEC_FPGA_CSR_RESETS = SPEC_BASE_REGS_CSR + 0x04,
	SPEC_FPGA_CSR_FMC_PRESENT = SPEC_BASE_REGS_CSR + 0x08,
	SPEC_FPGA_CSR_GN4124_STATUS = SPEC_BASE_REGS_CSR + 0x0C,
	SPEC_FPGA_CSR_DDR_STATUS = SPEC_BASE_REGS_CSR + 0x10,
	SPEC_FPGA_CSR_PCB_REV = SPEC_BASE_REGS_CSR + 0x14,
};

enum spec_fpga_csr_fields {
	SPEC_FPGA_CSR_DDR_STATUS_DONE = 0x1,
};

enum spec_fpga_therm_offsets {
	SPEC_FPGA_THERM_SERID_MSB = SPEC_BASE_REGS_THERM_ID + 0x0,
	SPEC_FPGA_THERM_SERID_LSB = SPEC_BASE_REGS_THERM_ID + 0x4,
	SPEC_FPGA_THERM_TEMP = SPEC_BASE_REGS_THERM_ID + 0x8,
};

enum spec_fpga_meta_cap_mask {
	SPEC_META_CAP_VIC = BIT(0),
	SPEC_META_CAP_THERM = BIT(1),
	SPEC_META_CAP_SPI = BIT(2),
	SPEC_META_CAP_WR = BIT(3),
	SPEC_META_CAP_BLD = BIT(4),
	SPEC_META_CAP_DMA = BIT(5),
};


static const struct debugfs_reg32 spec_fpga_debugfs_reg32[] = {
	{
		.name = "Application offset",
		.offset = SPEC_FPGA_CSR_APP_OFF,
	},
	{
		.name = "Resets",
		.offset = SPEC_FPGA_CSR_RESETS,
	},
	{
		.name = "FMC present",
		.offset = SPEC_FPGA_CSR_FMC_PRESENT,
	},
	{
		.name = "GN4124 Status",
		.offset = SPEC_FPGA_CSR_GN4124_STATUS,
	},
	{
		.name = "DDR Status",
		.offset = SPEC_FPGA_CSR_DDR_STATUS,
	},
	{
		.name = "PCB revision",
		.offset = SPEC_FPGA_CSR_PCB_REV,
	},
};

static int spec_fpga_dbg_bld_info(struct seq_file *s, void *offset)
{
	struct spec_fpga *spec_fpga = s->private;
	int off;

	if (!(spec_fpga->meta->cap & SPEC_META_CAP_BLD)) {
		seq_puts(s, "not available\n");
		return 0;
	}

	for (off = SPEC_BASE_REGS_BUILDINFO;
	     off < SPEC_BASE_REGS_BUILDINFO + SPEC_BASE_REGS_BUILDINFO_SIZE - 1;
	     off++) {
		char tmp = ioread8(spec_fpga->fpga + off);

		if (!tmp)
			return 0;
		seq_putc(s, tmp);
	}

	return 0;
}

static int spec_fpga_dbg_bld_info_open(struct inode *inode,
					 struct file *file)
{
	struct spec_gn412x *spec = inode->i_private;

	return single_open(file, spec_fpga_dbg_bld_info, spec);
}

static const struct file_operations spec_fpga_dbg_bld_ops = {
	.owner = THIS_MODULE,
	.open  = spec_fpga_dbg_bld_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int spec_fpga_dbg_init(struct spec_fpga *spec_fpga)
{
	struct pci_dev *pdev = to_pci_dev(spec_fpga->dev.parent);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	int err;

	spec_fpga->dbg_dir_fpga = debugfs_create_dir(dev_name(&spec_fpga->dev),
						     spec_gn412x->dbg_dir);
	if (IS_ERR_OR_NULL(spec_fpga->dbg_dir_fpga)) {
		err = PTR_ERR(spec_fpga->dbg_dir_fpga);
		dev_err(&spec_fpga->dev,
			"Cannot create debugfs directory \"%s\" (%d)\n",
			dev_name(&spec_fpga->dev), err);
		return err;
	}

	spec_fpga->dbg_csr_reg.regs = spec_fpga_debugfs_reg32;
	spec_fpga->dbg_csr_reg.nregs = ARRAY_SIZE(spec_fpga_debugfs_reg32);
	spec_fpga->dbg_csr_reg.base = spec_fpga->fpga;
	spec_fpga->dbg_csr = debugfs_create_regset32(SPEC_DBG_CSR_NAME, 0200,
						spec_fpga->dbg_dir_fpga,
						&spec_fpga->dbg_csr_reg);
	if (IS_ERR_OR_NULL(spec_fpga->dbg_csr)) {
		err = PTR_ERR(spec_fpga->dbg_csr);
		dev_warn(&spec_fpga->dev,
			"Cannot create debugfs file \"%s\" (%d)\n",
			SPEC_DBG_CSR_NAME, err);
		goto err;
	}

	spec_fpga->dbg_bld = debugfs_create_file(SPEC_DBG_BLD_INFO_NAME,
						 0444,
						 spec_fpga->dbg_dir_fpga,
						 spec_fpga,
						 &spec_fpga_dbg_bld_ops);
	if (IS_ERR_OR_NULL(spec_fpga->dbg_bld)) {
		err = PTR_ERR(spec_fpga->dbg_bld);
		dev_err(&spec_fpga->dev,
			"Cannot create debugfs file \"%s\" (%d)\n",
			SPEC_DBG_BLD_INFO_NAME, err);
		goto err;
	}

	return 0;
err:
	debugfs_remove_recursive(spec_fpga->dbg_dir_fpga);
	return err;
}

static void spec_fpga_dbg_exit(struct spec_fpga *spec_fpga)
{
	debugfs_remove_recursive(spec_fpga->dbg_dir_fpga);
}

static inline uint32_t spec_fpga_csr_app_offset(struct spec_fpga *spec_fpga)
{
	return ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_APP_OFF);
}

static inline uint32_t spec_fpga_csr_pcb_rev(struct spec_fpga *spec_fpga)
{
	return ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_PCB_REV);
}


static struct resource spec_fpga_vic_res[] = {
	{
		.name = "htvic-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_BASE_REGS_VIC,
		.end = SPEC_BASE_REGS_VIC + SPEC_BASE_REGS_VIC_SIZE - 1,
	}, {
		.name = "htvic-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = 0,
		.end = 0,
	},
};

struct irq_domain *spec_fpga_irq_find_host(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	struct irq_fwspec fwspec = {
		.fwnode = dev->fwnode,
		.param_count = 2,
		.param[0] = ((unsigned long)dev >> 32) & 0xffffffff,
		.param[1] = ((unsigned long)dev) & 0xffffffff,
	};
	return irq_find_matching_fwspec(&fwspec, DOMAIN_BUS_ANY);
#else
	return (irq_find_host((void *)dev));
#endif
}

/* Vector Interrupt Controller */
static int spec_fpga_vic_init(struct spec_fpga *spec_fpga)
{
	struct pci_dev *pcidev = to_pci_dev(spec_fpga->dev.parent);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pcidev);
	unsigned long pci_start = pci_resource_start(pcidev, 0);
	const unsigned int res_n = ARRAY_SIZE(spec_fpga_vic_res);
	struct resource res[ARRAY_SIZE(spec_fpga_vic_res)];
	struct platform_device *pdev;

	if (!(spec_fpga->meta->cap & SPEC_META_CAP_VIC))
		return 0;

	memcpy(&res, spec_fpga_vic_res, sizeof(res));
	res[0].start += pci_start;
	res[0].end += pci_start;
	res[1].start = gpiod_to_irq(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]);
	res[1].end = res[1].start;
	pdev = platform_device_register_resndata(&spec_fpga->dev,
						 "htvic-spec",
						 PLATFORM_DEVID_AUTO,
						 res, res_n,
						 NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	spec_fpga->vic_pdev = pdev;

	return 0;
}

static void spec_fpga_vic_exit(struct spec_fpga *spec_fpga)
{
	if (spec_fpga->vic_pdev) {
		platform_device_unregister(spec_fpga->vic_pdev);
		spec_fpga->vic_pdev = NULL;
	}
}

/* DMA engine */
static struct resource spec_fpga_dma_res[] = {
	{
		.name = "spec-gn412x-dma-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_BASE_REGS_DMA,
		.end = SPEC_BASE_REGS_DMA + SPEC_BASE_REGS_DMA_SIZE - 1,
	}, {
		.name = "spec-gn412x-dma-irq-done",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = 0,
		.end = 0,
	},
};

static int spec_fpga_dma_init(struct spec_fpga *spec_fpga)
{
	struct pci_dev *pcidev = to_pci_dev(spec_fpga->dev.parent);
	unsigned long pci_start = pci_resource_start(pcidev, 0);
	const unsigned int res_n = ARRAY_SIZE(spec_fpga_dma_res);
	struct resource res[ARRAY_SIZE(spec_fpga_dma_res)];
	struct platform_device *pdev;
	struct irq_domain *vic_domain;
	uint32_t ddr_status;

	if (!(spec_fpga->meta->cap & SPEC_META_CAP_DMA))
		return 0;

	mdelay(1);
	ddr_status = ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_DDR_STATUS);
	if (!(ddr_status & SPEC_FPGA_CSR_DDR_STATUS_DONE)) {
		dev_err(&spec_fpga->dev,
			"Failed to load DMA engine: DDR controller not calibrated - 0x%x.\n",
			ddr_status);
		return -ENODEV;
	}
	vic_domain = spec_fpga_irq_find_host(&spec_fpga->vic_pdev->dev);
	if (!vic_domain) {
		dev_err(&spec_fpga->dev,
			"Failed to load DMA engine: can't find VIC\n");
		return -ENODEV;
	}

	memcpy(&res, spec_fpga_dma_res, sizeof(res));
	res[0].start += pci_start;
	res[0].end += pci_start;
	res[1].start = irq_find_mapping(vic_domain, SPEC_FPGA_IRQ_DMA_DONE);
	pdev = platform_device_register_resndata(&spec_fpga->dev,
						 "spec-gn412x-dma",
						 PLATFORM_DEVID_AUTO,
						 res, res_n,
						 NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	spec_fpga->dma_pdev = pdev;

	return 0;
}
static void spec_fpga_dma_exit(struct spec_fpga *spec_fpga)
{
	if (spec_fpga->dma_pdev) {
		platform_device_unregister(spec_fpga->dma_pdev);
		spec_fpga->dma_pdev = NULL;
	}
}

/* MFD devices */
enum spec_fpga_mfd_devs_enum {
	SPEC_FPGA_MFD_FMC_I2C = 0,
	SPEC_FPGA_MFD_SPI,
};

static struct resource spec_fpga_fmc_i2c_res[] = {
	{
		.name = "i2c-ocores-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_BASE_REGS_FMC_I2C,
		.end = SPEC_BASE_REGS_FMC_I2C +
		       SPEC_BASE_REGS_FMC_I2C_SIZE - 1,
	}, {
		.name = "i2c-ocores-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = SPEC_FPGA_IRQ_FMC_I2C,
		.end = SPEC_FPGA_IRQ_FMC_I2C,
	},
};

#define SPEC_FPGA_WB_CLK_HZ 62500000
#define SPEC_FPGA_WB_CLK_KHZ (SPEC_FPGA_WB_CLK_HZ / 1000)
static struct ocores_i2c_platform_data spec_fpga_fmc_i2c_pdata = {
	.reg_shift = 2, /* 32bit aligned */
	.reg_io_width = 4,
	.clock_khz = SPEC_FPGA_WB_CLK_KHZ,
	.big_endian = 0,
	.num_devices = 0,
	.devices = NULL,
};

static struct resource spec_fpga_spi_res[] = {
	{
		.name = "spi-ocores-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_BASE_REGS_FLASH_SPI,
		.end = SPEC_BASE_REGS_FLASH_SPI +
		       SPEC_BASE_REGS_FLASH_SPI_SIZE - 1,
	}, {
		.name = "spi-ocores-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = SPEC_FPGA_IRQ_SPI,
		.end = SPEC_FPGA_IRQ_SPI,
	},
};

struct flash_platform_data spec_flash_pdata = {
	.name = "spec-flash",
	.parts = NULL,
	.nr_parts = 0,
	.type = "m25p32",
};

static struct spi_board_info spec_fpga_spi_devices_info[] = {
	{
		.modalias = "m25p32", /*
				       * just informative: sometimes we have
				       * other chips, but the m25p80 driver
				       * takes care of identifying the correct
				       * memory
				       */
		.max_speed_hz = SPEC_FPGA_WB_CLK_HZ / 128,
		.chip_select = 0,
		.platform_data = &spec_flash_pdata,
	}
};

static struct spi_ocores_platform_data spec_fpga_spi_pdata = {
	.big_endian = 0,
	.clock_hz = SPEC_FPGA_WB_CLK_HZ,
	.num_devices = ARRAY_SIZE(spec_fpga_spi_devices_info),
	.devices = spec_fpga_spi_devices_info,
};


static const struct mfd_cell spec_fpga_mfd_devs[] = {
	[SPEC_FPGA_MFD_FMC_I2C] = {
		.name = "i2c-ohwr",
		.platform_data = &spec_fpga_fmc_i2c_pdata,
		.pdata_size = sizeof(spec_fpga_fmc_i2c_pdata),
		.num_resources = ARRAY_SIZE(spec_fpga_fmc_i2c_res),
		.resources = spec_fpga_fmc_i2c_res,
	},
	[SPEC_FPGA_MFD_SPI] = {
		.name = "spi-ocores",
		.platform_data = &spec_fpga_spi_pdata,
		.pdata_size = sizeof(spec_fpga_spi_pdata),
		.num_resources = ARRAY_SIZE(spec_fpga_spi_res),
		.resources = spec_fpga_spi_res,
	},
};

static inline size_t __fpga_mfd_devs_size(void)
{
#define SPEC_FPGA_MFD_DEVS_MAX 4
	return (sizeof(struct mfd_cell) * SPEC_FPGA_MFD_DEVS_MAX);
}

static int spec_fpga_devices_init(struct spec_fpga *spec_fpga)
{
	struct pci_dev *pcidev = to_pci_dev(spec_fpga->dev.parent);
	struct mfd_cell *fpga_mfd_devs;
	struct irq_domain *vic_domain;
	unsigned int n_mfd = 0;
	int err;

	fpga_mfd_devs = devm_kzalloc(&spec_fpga->dev,
				     __fpga_mfd_devs_size(),
				     GFP_KERNEL);
	if (!fpga_mfd_devs)
		return -ENOMEM;

	memcpy(&fpga_mfd_devs[n_mfd],
	       &spec_fpga_mfd_devs[SPEC_FPGA_MFD_FMC_I2C],
	       sizeof(fpga_mfd_devs[n_mfd]));
	n_mfd++;

	if (spec_fpga->meta->cap & SPEC_META_CAP_SPI) {
		memcpy(&fpga_mfd_devs[n_mfd],
		       &spec_fpga_mfd_devs[SPEC_FPGA_MFD_SPI],
		       sizeof(fpga_mfd_devs[n_mfd]));
		n_mfd++;
	}

	vic_domain = spec_fpga_irq_find_host(&spec_fpga->vic_pdev->dev);
	if (!vic_domain) {
		/* Remove IRQ resource from all devices */
		fpga_mfd_devs[0].num_resources = 1;  /* FMC I2C */
		fpga_mfd_devs[1].num_resources = 1;  /* SPI */
	}
	err = mfd_add_devices(&spec_fpga->dev,
			      PLATFORM_DEVID_AUTO,
			      fpga_mfd_devs, n_mfd,
			      &pcidev->resource[0],
			      0, vic_domain);
	if (err)
		goto err_mfd;

	return 0;

err_mfd:
	devm_kfree(&spec_fpga->dev, fpga_mfd_devs);

	return err;
}

static void spec_fpga_devices_exit(struct spec_fpga *spec_fpga)
{
	mfd_remove_devices(&spec_fpga->dev);
}

/* Thermometer */
static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct spec_fpga *spec_fpga = to_spec_fpga(dev);

	if (spec_fpga->meta->cap & SPEC_META_CAP_THERM) {
		uint32_t temp;

		temp = ioread32(spec_fpga->fpga + SPEC_FPGA_THERM_TEMP);

		return snprintf(buf, PAGE_SIZE, "%d.%d C\n",
			temp / 16, (temp & 0xF) * 1000 / 16);
	}
	return snprintf(buf, PAGE_SIZE, "-.- C\n");
}
static DEVICE_ATTR_RO(temperature);

static ssize_t serial_number_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct spec_fpga *spec_fpga = to_spec_fpga(dev);

	if (spec_fpga->meta->cap & SPEC_META_CAP_THERM) {
		uint32_t msb, lsb;

		msb = ioread32(spec_fpga->fpga + SPEC_FPGA_THERM_SERID_MSB);
		lsb = ioread32(spec_fpga->fpga + SPEC_FPGA_THERM_SERID_LSB);

		return snprintf(buf, PAGE_SIZE, "0x%08x%08x\n", msb, lsb);
	}
	return snprintf(buf, PAGE_SIZE, "0x----------------\n");
}
static DEVICE_ATTR_RO(serial_number);

static struct attribute *spec_fpga_therm_attrs[] = {
	&dev_attr_serial_number.attr,
	&dev_attr_temperature.attr,
	NULL,
};

static const struct attribute_group spec_fpga_therm_group = {
	.name = "thermometer",
	.attrs = spec_fpga_therm_attrs,
};

/* CSR attributes */
static ssize_t pcb_rev_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct spec_fpga *spec_fpga = to_spec_fpga(dev);

	return snprintf(buf, PAGE_SIZE, "0x%x\n",
			spec_fpga_csr_pcb_rev(spec_fpga));
}
static DEVICE_ATTR_RO(pcb_rev);

static ssize_t application_offset_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct spec_fpga *spec_fpga = to_spec_fpga(dev);

	return snprintf(buf, PAGE_SIZE, "0x%x\n",
			spec_fpga_csr_app_offset(spec_fpga));
}
static DEVICE_ATTR_RO(application_offset);

enum spec_fpga_csr_resets {
	SPEC_FPGA_CSR_RESETS_ALL = BIT(0),
	SPEC_FPGA_CSR_RESETS_APP = BIT(1),
};

static void spec_fpga_app_reset(struct spec_fpga *spec_fpga, bool val)
{
	uint32_t resets;

	resets = ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_RESETS);
	if (val)
		resets |= SPEC_FPGA_CSR_RESETS_APP;
	else
		resets &= ~SPEC_FPGA_CSR_RESETS_APP;
	iowrite32(resets, spec_fpga->fpga + SPEC_FPGA_CSR_RESETS);
}

static void spec_fpga_app_restart(struct spec_fpga *spec_fpga)
{
	spec_fpga_app_reset(spec_fpga, true);
	udelay(1);
	spec_fpga_app_reset(spec_fpga, false);
	udelay(1);
}

static ssize_t reset_app_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct spec_fpga *spec_fpga = to_spec_fpga(dev);
	uint32_t resets;

	resets = ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_RESETS);
	return snprintf(buf, PAGE_SIZE, "%d\n",
			!!(resets & SPEC_FPGA_CSR_RESETS_APP));
}
static ssize_t reset_app_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	spec_fpga_app_reset(to_spec_fpga(dev), val);

	return count;
}
static DEVICE_ATTR_RW(reset_app);

static struct attribute *spec_fpga_csr_attrs[] = {
	&dev_attr_pcb_rev.attr,
	&dev_attr_application_offset.attr,
	&dev_attr_reset_app.attr,
	NULL,
};

static const struct attribute_group spec_fpga_csr_group = {
	.attrs = spec_fpga_csr_attrs,
};

/* FMC */
#define SPEC_FMC_SLOTS 1

static inline u8 spec_fmc_presence(struct spec_fpga *spec_fpga)
{
	return (ioread32(spec_fpga->fpga + SPEC_FPGA_CSR_FMC_PRESENT) & 0x1);
}

static int spec_fmc_is_present(struct fmc_carrier *carrier,
			       struct fmc_slot *slot)
{
	struct spec_fpga *spec_fpga = carrier->priv;

	return spec_fmc_presence(spec_fpga);
}

static const struct fmc_carrier_operations spec_fmc_ops = {
	.owner = THIS_MODULE,
	.is_present = spec_fmc_is_present,
};

static int spec_i2c_find_adapter(struct device *dev, void *data)
{
	struct spec_fpga *spec_fpga = data;
	struct i2c_adapter *adap, *adap_parent;

	if (dev->type != &i2c_adapter_type)
		return 0;

	adap = to_i2c_adapter(dev);
	adap_parent = i2c_parent_is_i2c_adapter(adap);
	if (!adap_parent)
		return 0;

	/* We have a muxed I2C master */
	if (&spec_fpga->dev != adap_parent->dev.parent->parent)
		return 0;

	/* Found! Return the bus ID */
	return i2c_adapter_id(adap);
}

/**
 * Get the I2C adapter associated with an FMC slot
 * @data: data used to find the correct I2C bus
 * @slot_nr: FMC slot number
 *
 * Return: the I2C bus to be used
 */
static int spec_i2c_get_bus(struct spec_fpga *spec_fpga)
{
	return i2c_for_each_dev(spec_fpga, spec_i2c_find_adapter);
}

/**
 * Create an FMC interface
 */
static int spec_fmc_init(struct spec_fpga *spec_fpga)
{
	int err;

	spec_fpga->slot_info.i2c_bus_nr = spec_i2c_get_bus(spec_fpga);
	if (spec_fpga->slot_info.i2c_bus_nr <= 0) {
		dev_err(spec_fpga->dev.parent,
			"Invalid I2C bus number %d\n",
			spec_fpga->slot_info.i2c_bus_nr);
		return -ENODEV;
	}
	spec_fpga->slot_info.ga = 0;
	spec_fpga->slot_info.lun = 1;

	err = fmc_carrier_register(&spec_fpga->dev, &spec_fmc_ops,
				   SPEC_FMC_SLOTS, &spec_fpga->slot_info,
				   spec_fpga);
	if (err) {
		dev_err(spec_fpga->dev.parent,
			"Failed to register as FMC carrier\n");
		goto err_fmc;
	}


	return 0;

err_fmc:
	return err;
}

static int spec_fmc_exit(struct spec_fpga *spec_fpga)
{
	return fmc_carrier_unregister(&spec_fpga->dev);
}

/* FPGA Application */

/**
 * Build the platform_device_id->name from metadata
 *
 * The byte order on SPEC is little endian, but we want to convert it
 * in string. Use big-endian read to swap word and get the string order
 * from MSB to LSB
 */
static int spec_fpga_app_id_build(struct spec_fpga *spec_fpga,
				  unsigned long app_off,
				  char *id, unsigned int size)
{
	uint32_t vendor, device;

	vendor = ioread32be(spec_fpga->fpga + app_off + FPGA_META_VENDOR);
	device = ioread32be(spec_fpga->fpga + app_off + FPGA_META_DEVICE);

	memset(id, 0, size);
	if (vendor == 0xFF000000) {
		dev_warn(&spec_fpga->dev, "Vendor UUID not supported yet\n");
		return -ENODEV;
	}
	snprintf(id, size, "id:%4phN%4phN", &vendor, &device);

	return 0;
}

static int spec_fpga_app_init_res_mem(struct spec_fpga *spec_fpga,
				       struct resource *res)
{
	struct pci_dev *pcidev = to_pci_dev(spec_fpga->dev.parent);
	unsigned long app_offset;

	app_offset = spec_fpga_csr_app_offset(spec_fpga);
	if (!app_offset)
		return -ENODEV;

	res->name  = "app-mem";
	res->flags = IORESOURCE_MEM;
	res->start = pci_resource_start(pcidev, 0) + app_offset;
	res->end = pci_resource_end(pcidev, 0);

	return 0;
}

static void spec_fpga_app_init_res_irq(struct spec_fpga *spec_fpga,
				       unsigned int first_hwirq,
				       struct resource *res,
				       unsigned int res_n)
{
	struct irq_domain *vic_domain;
	int i, hwirq;

	if (!spec_fpga->vic_pdev)
		return;

	vic_domain = spec_fpga_irq_find_host(&spec_fpga->vic_pdev->dev);
	for (i = 0, hwirq = first_hwirq; i < res_n; ++i, ++hwirq) {
		res[i].name = "app-irq";
		res[i].flags = IORESOURCE_IRQ;
		res[i].start = irq_find_mapping(vic_domain, hwirq);
	}
}

static void spec_fpga_app_init_res_dma(struct spec_fpga *spec_fpga,
				       struct resource *res)
{
	struct dma_device *dma;

	if (!spec_fpga->dma_pdev) {
		dev_warn(&spec_fpga->dev, "Not able to find DMA engine: platform_device missing\n");
		return ;
	}
	dma = platform_get_drvdata(spec_fpga->dma_pdev);
	if (dma) {
		res->name  = "app-dma";
		res->flags = IORESOURCE_DMA;
		res->start = 0;
		res->start |= dma->dev_id << 16;
	} else {
		dev_warn(&spec_fpga->dev, "Not able to find DMA engine: drvdata missing\n");
	}
}

#define SPEC_FPGA_APP_NAME_MAX 47
#define SPEC_FPGA_APP_IRQ_BASE 6
#define SPEC_FPGA_APP_RES_IRQ_START 2
#define SPEC_FPGA_APP_RES_IRQ_N (32 - SPEC_FPGA_APP_IRQ_BASE)
#define SPEC_FPGA_APP_RES_N (SPEC_FPGA_APP_RES_IRQ_N + 1 + 1) /* IRQs MEM DMA */
#define SPEC_FPGA_APP_RES_MEM 0
#define SPEC_FPGA_APP_RES_DMA 1
static int spec_fpga_app_init(struct spec_fpga *spec_fpga)
{
	unsigned int res_n = SPEC_FPGA_APP_RES_N;
	struct resource *res;
	struct platform_device *pdev;
	char app_name[SPEC_FPGA_APP_NAME_MAX];

	int err = 0;

	res = kcalloc(SPEC_FPGA_APP_RES_N, sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	err = spec_fpga_app_init_res_mem(spec_fpga,
					 &res[SPEC_FPGA_APP_RES_MEM]);
	if (err) {
		dev_warn(&spec_fpga->dev, "Application not found\n");
		err = 0;
		goto err_free;
	}
	spec_fpga_app_init_res_dma(spec_fpga, &res[SPEC_FPGA_APP_RES_DMA]);
	spec_fpga_app_init_res_irq(spec_fpga,
				   SPEC_FPGA_APP_IRQ_BASE,
				   &res[SPEC_FPGA_APP_RES_IRQ_START],
				   SPEC_FPGA_APP_RES_IRQ_N);

	err = spec_fpga_app_id_build(spec_fpga,
				     spec_fpga_csr_app_offset(spec_fpga),
				     app_name, SPEC_FPGA_APP_NAME_MAX);
	if (err)
		goto err_free;
	spec_fpga_app_restart(spec_fpga);
	pdev = platform_device_register_resndata(&spec_fpga->dev,
						 app_name, PLATFORM_DEVID_AUTO,
						 res, res_n,
						 NULL, 0);
	err = IS_ERR(pdev);
	if (err)
		goto err_free;

	spec_fpga->app_pdev = pdev;

err_free:
	kfree(res);
	return err;
}

static void spec_fpga_app_exit(struct spec_fpga *spec_fpga)
{
	if (spec_fpga->app_pdev) {
		platform_device_unregister(spec_fpga->app_pdev);
		spec_fpga->app_pdev = NULL;
	}
}


static bool spec_fpga_is_valid(struct spec_gn412x *spec_gn412x,
			       struct spec_meta_id *meta)
{
	if ((meta->bom & SPEC_META_BOM_END_MASK) != SPEC_META_BOM_LE) {
		dev_err(&spec_gn412x->pdev->dev,
			"Expected Little Endian devices BOM: 0x%x\n",
			meta->bom);
		return false;
	}

	if ((meta->bom & SPEC_META_BOM_VER_MASK) != 0) {
		dev_err(&spec_gn412x->pdev->dev,
			"Unknow Metadata specification version BOM: 0x%x\n",
			meta->bom);
		return false;
	}

	if (meta->vendor != SPEC_META_VENDOR_ID ||
	    meta->device != SPEC_META_DEVICE_ID) {
		dev_err(&spec_gn412x->pdev->dev,
			"Unknow vendor/device ID: %08x:%08x\n",
			meta->vendor, meta->device);
		return false;
	}

	if ((meta->version & SPEC_META_VERSION_MASK) != SPEC_META_VERSION_1_4) {
		dev_err(&spec_gn412x->pdev->dev,
			"Unknow version: %08x\n", meta->version);
		return false;
	}

	return true;
}


static void spec_release(struct device *dev)
{

}

static int spec_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return 0;
}

static const struct attribute_group *spec_groups[] = {
	&spec_fpga_therm_group,
	&spec_fpga_csr_group,
	NULL
};

static const struct device_type spec_fpga_type = {
	.name = "spec",
	.release = spec_release,
	.uevent = spec_uevent,
	.groups = spec_groups,
};

/**
 * Initialize carrier devices on FPGA
 */
int spec_fpga_init(struct spec_gn412x *spec_gn412x)
{
	struct spec_fpga *spec_fpga;
	struct resource *r0 = &spec_gn412x->pdev->resource[0];
	int err;

	spec_fpga = kzalloc(sizeof(*spec_fpga), GFP_KERNEL);
	if (!spec_fpga)
		return -ENOMEM;

	spec_gn412x->spec_fpga = spec_fpga;
	spec_fpga->fpga = ioremap(r0->start, resource_size(r0));
	if (!spec_fpga->fpga) {
		err = -ENOMEM;
		goto err_map;
	}
	spec_fpga->meta = spec_fpga->fpga + SPEC_META_BASE;
	if (!spec_fpga_is_valid(spec_gn412x, spec_fpga->meta)) {
		err =  -EINVAL;
		goto err_valid;
	}

	spec_fpga->dev.parent = &spec_gn412x->pdev->dev;
	spec_fpga->dev.driver = spec_gn412x->pdev->dev.driver;
	spec_fpga->dev.type = &spec_fpga_type;
	err = dev_set_name(&spec_fpga->dev, "spec-%s",
			   dev_name(&spec_gn412x->pdev->dev));
	if (err)
		goto err_name;

	err = device_register(&spec_fpga->dev);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev, "Failed to register '%s'\n",
			dev_name(&spec_gn412x->pdev->dev));
		goto err_dev;
	}

	spec_fpga_dbg_init(spec_fpga);

	err = spec_fpga_vic_init(spec_fpga);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to initialize VIC %d\n", err);
		goto err_vic;
	}
	err = spec_fpga_dma_init(spec_fpga);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to initialize DMA %d\n", err);
			goto err_dma;
	}
	err = spec_fpga_devices_init(spec_fpga);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to initialize Devices %d\n", err);
		goto err_devs;
	}
	err = spec_fmc_init(spec_fpga);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to initialize FMC %d\n", err);
			goto err_fmc;
	}
	err = spec_fpga_app_init(spec_fpga);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to initialize APP %d\n", err);
		goto err_app;
	}

	return 0;

err_app:
	spec_fmc_exit(spec_fpga);
err_fmc:
	spec_fpga_devices_exit(spec_fpga);
err_devs:
	spec_fpga_dma_exit(spec_fpga);
err_dma:
	spec_fpga_vic_exit(spec_fpga);
err_vic:
	return err;

err_dev:
err_name:
err_valid:
	iounmap(spec_fpga->fpga);
err_map:
	kfree(spec_fpga);
	spec_gn412x->spec_fpga = NULL;
	return err;
}

int spec_fpga_exit(struct spec_gn412x *spec_gn412x)
{
	struct spec_fpga *spec_fpga = spec_gn412x->spec_fpga;

	if (!spec_fpga)
		return 0;

	spec_fpga_app_exit(spec_fpga);
	spec_fmc_exit(spec_fpga);
	spec_fpga_devices_exit(spec_fpga);
	spec_fpga_dma_exit(spec_fpga);
	spec_fpga_vic_exit(spec_fpga);

	spec_fpga_dbg_exit(spec_fpga);
	device_unregister(&spec_fpga->dev);
	iounmap(spec_fpga->fpga);
	kfree(spec_fpga);
	spec_gn412x->spec_fpga = NULL;

	return 0;
}
