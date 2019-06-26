// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/types.h>
#include <linux/platform_data/i2c-ocores.h>
#include <linux/ioport.h>
#include <linux/gpio/consumer.h>
#include <linux/irqdomain.h>
#include <linux/mfd/core.h>

#include "spec.h"
#include "spec-compat.h"

#include "platform_data/spi-ocores.h"


/* FIXME find better ID */
static int vic_id;
static int mfd_id;
static int app_id;

enum spec_fpga_mem_offsets {
	SPEC_FPGA_MEM_THERM_START = SPEC_CORE_FPGA + 0x50,
	SPEC_FPGA_MEM_THERM_END = SPEC_CORE_FPGA + 0x5F,
	SPEC_FPGA_MEM_FMC_I2C_START = SPEC_CORE_FPGA + 0x0080,
	SPEC_FPGA_MEM_FMC_I2C_END = SPEC_CORE_FPGA + 0x009F,
	SPEC_FPGA_MEM_VIC_START = SPEC_CORE_FPGA + 0x0100,
	SPEC_FPGA_MEM_VIC_END = SPEC_CORE_FPGA + 0x01FF,
	SPEC_FPGA_MEM_SPI_START = SPEC_CORE_FPGA + 0x0100,
	SPEC_FPGA_MEM_SPI_END = SPEC_CORE_FPGA + 0x01FF,
};

enum spec_fpga_irq_lines {
	SPEC_FPGA_IRQ_DMA_DONE = 0,
	SPEC_FPGA_IRQ_DMA_ERROR,
	SPEC_FPGA_IRQ_FMC_I2C,
	SPEC_FPGA_IRQ_SPI,
};

static struct resource spec_fpga_vic_res[] = {
	{
		.name = "htvic-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_FPGA_MEM_VIC_START,
		.end = SPEC_FPGA_MEM_VIC_END,
	}, {
		.name = "htvic-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = 0,
		.end = 0,
	},
};

/* Vector Interrupt Controller */
static int spec_fpga_vic_init(struct spec_dev *spec)
{
	struct pci_dev *pcidev = to_pci_dev(spec->dev.parent);
	unsigned long pci_start = pci_resource_start(pcidev, 0);
	const unsigned int res_n = ARRAY_SIZE(spec_fpga_vic_res);
	struct resource res[ARRAY_SIZE(spec_fpga_vic_res)];
	struct platform_device *pdev;

	if (!(spec->meta->cap & SPEC_META_CAP_VIC))
		return 0;

	memcpy(&res, spec_fpga_vic_res, sizeof(spec_fpga_vic_res));
	res[0].start += pci_start;
	res[0].end += pci_start;
	res[1].start = gpiod_to_irq(spec->gpiod[GN4124_GPIO_IRQ1]);
	res[1].end = res[1].start;
	pdev = platform_device_register_resndata(&spec->dev,
						 "htvic-spec", vic_id++,
						 res, res_n,
						 NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	spec->vic_pdev = pdev;

	return 0;
}

static void spec_fpga_vic_exit(struct spec_dev *spec)
{
	if (spec->vic_pdev) {
		platform_device_unregister(spec->vic_pdev);
		spec->vic_pdev = NULL;
	}
}

/* MFD devices */
enum spce_fpga_mfd_devs_enum {
	SPEC_FPGA_MFD_FMC_I2C = 0,
	SPEC_FPGA_MFD_SPI,
};

static struct resource spec_fpga_fmc_i2c_res[] = {
	{
		.name = "i2c-ocores-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_FPGA_MEM_FMC_I2C_START,
		.end = SPEC_FPGA_MEM_FMC_I2C_END,
	}, {
		.name = "i2c-ocores-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = SPEC_FPGA_IRQ_FMC_I2C,
		.end = SPEC_FPGA_IRQ_FMC_I2C,
	},
};

static struct ocores_i2c_platform_data spec_fpga_fmc_i2c_pdata = {
	.reg_shift = 2, /* 32bit aligned */
	.reg_io_width = 4,
	.clock_khz = 62500,
	.big_endian = 0,
	.num_devices = 0,
	.devices = NULL,
};

static struct resource spec_fpga_spi_res[] = {
	{
		.name = "spi-ocores-mem",
		.flags = IORESOURCE_MEM,
		.start = SPEC_FPGA_MEM_SPI_START,
		.end = SPEC_FPGA_MEM_SPI_END,
	}, {
		.name = "spi-ocores-irq",
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
		.start = SPEC_FPGA_IRQ_SPI,
		.end = SPEC_FPGA_IRQ_SPI,
	},
};

static struct spi_ocores_platform_data spec_fpga_spi_pdata = {
	.big_endian = 0,
	.clock_hz = 65200000,
	.num_devices = 0,
	.devices = NULL,
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

static int spec_fpga_devices_init(struct spec_dev *spec)
{
	struct pci_dev *pcidev = to_pci_dev(spec->dev.parent);
	struct mfd_cell *fpga_mfd_devs;
	struct irq_domain *vic_domain;
	unsigned int n_mfd = 0;
	int err;

	fpga_mfd_devs = devm_kzalloc(&spec->dev,
				     __fpga_mfd_devs_size(),
				     GFP_KERNEL);
	if (!fpga_mfd_devs)
		return -ENOMEM;

	memcpy(&fpga_mfd_devs[n_mfd],
	       &spec_fpga_mfd_devs[SPEC_FPGA_MFD_FMC_I2C],
	       sizeof(fpga_mfd_devs[n_mfd]));
	n_mfd++;

	memcpy(&fpga_mfd_devs[n_mfd],
	       &spec_fpga_mfd_devs[SPEC_FPGA_MFD_SPI],
	       sizeof(fpga_mfd_devs[n_mfd]));
	n_mfd++;

	vic_domain = irq_find_host((void *)&spec->vic_pdev->dev);
	if (!vic_domain) {
		/* Remove IRQ resource from all devices */
		fpga_mfd_devs[0].num_resources = 1;  /* FMC I2C */
		fpga_mfd_devs[1].num_resources = 1;  /* SPI */
	}
	err = mfd_add_devices(&spec->dev, mfd_id++,
			      fpga_mfd_devs, n_mfd,
			      &pcidev->resource[0],
			      0, vic_domain);
	if (err)
		goto err_mfd;

	return 0;

err_mfd:
	devm_kfree(&spec->dev, fpga_mfd_devs);

	return err;
}

static void spec_fpga_devices_exit(struct spec_dev *spec)
{
	mfd_remove_devices(&spec->dev);
}

/* Thermometer */
#define SPEC_FPGA_THERM_SERID_MSB (SPEC_FPGA_MEM_THERM_START + 0x0)
#define SPEC_FPGA_THERM_SERID_LSB (SPEC_FPGA_MEM_THERM_START + 0x4)
#define SPEC_FPGA_THERM_TEMP (SPEC_FPGA_MEM_THERM_START + 0x8)
static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct spec_dev *spec = to_spec_dev(dev);
	uint32_t temp = ioread32(spec->fpga + SPEC_FPGA_THERM_TEMP);

	return snprintf(buf, PAGE_SIZE, "%d.%d C\n",
			temp / 16, (temp & 0xF) * 1000 / 16);
}
static DEVICE_ATTR_RO(temperature);

static ssize_t serial_number_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct spec_dev *spec = to_spec_dev(dev);

	return snprintf(buf, PAGE_SIZE, "0x%08x%08x\n",
			ioread32(spec->fpga + SPEC_FPGA_THERM_SERID_MSB),
			ioread32(spec->fpga + SPEC_FPGA_THERM_SERID_LSB));
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

static int spec_fpga_therm_init(struct spec_dev *spec)
{
	int err;

	clear_bit(SPEC_FLAG_THERM_BIT, spec->flags);
	if (!(spec->meta->cap & SPEC_META_CAP_THERM))
		return 0;

	err = sysfs_create_group(&spec->dev.kobj,
				 &spec_fpga_therm_group);
	if (err)
		return err;

	set_bit(SPEC_FLAG_THERM_BIT, spec->flags);
	return 0;
}

static void spec_fpga_therm_exit(struct spec_dev *spec)
{
	if (!(test_bit(SPEC_FLAG_THERM_BIT, spec->flags)))
		return;
	sysfs_remove_group(&spec->dev.kobj, &spec_fpga_therm_group);
}


/* FPGA Application */
#define SPEC_FPGA_APP_OFF (SPEC_CORE_FPGA + 0x40)
static void spec_fpga_app_id_build(struct spec_dev *spec,
					unsigned long app_off,
					char *id, unsigned int size)
{
	uint32_t vendor = ioread32(spec->fpga + app_off);
	uint32_t device = ioread32(spec->fpga + app_off);

	memset(id, 0, size);
	if (vendor == 0xFF000000) {
		uint32_t vendor_uuid[4];

		vendor_uuid[0] = ioread32(spec->fpga + app_off + 0x30);
		vendor_uuid[1] = ioread32(spec->fpga + app_off + 0x34);
		vendor_uuid[2] = ioread32(spec->fpga + app_off + 0x38);
		vendor_uuid[3] = ioread32(spec->fpga + app_off + 0x3C);
		snprintf(id, size, "cern:%16phN:%4phN", &vendor_uuid, &device);
	} else {
		snprintf(id, size, "cern:%4phN:%4phN", &vendor, &device);
	}
}

static int spec_fpga_app_init(struct spec_dev *spec)
{
#define SPEC_FPGA_APP_NAME_MAX 47
#define SPEC_FPGA_APP_IRQ_BASE 5
#define SPEC_FPGA_APP_RES_N 28
	struct pci_dev *pcidev = to_pci_dev(spec->dev.parent);
	unsigned int res_n = SPEC_FPGA_APP_RES_N;
	struct resource res[SPEC_FPGA_APP_RES_N] = {
		[0] = {
			.name = "app-mem",
			.flags = IORESOURCE_MEM,
		},
	};
	struct platform_device *pdev;
	struct irq_domain *vic_domain;
	char app_name[SPEC_FPGA_APP_NAME_MAX];
	unsigned long app_offset;

	app_offset = ioread32(spec->fpga + SPEC_FPGA_APP_OFF);
	if (!app_offset) {
		dev_warn(spec->dev.parent, "Application not found\n");
		return 0;
	}

	res[0].start = pci_resource_start(pcidev, 0) + app_offset;
	res[0].end = pci_resource_end(pcidev, 0);

	if (spec->vic_pdev)
		vic_domain = irq_find_host((void *)&spec->vic_pdev->dev);
	else
		vic_domain = NULL;

	if (vic_domain) {
		int i, hwirq;

		for (i = 1, hwirq = SPEC_FPGA_APP_IRQ_BASE;
		     i < SPEC_FPGA_APP_RES_N;
		     ++i, ++hwirq) {
			res[i].name = "app-irq",
			res[i].flags = IORESOURCE_IRQ,
			res[i].start = irq_find_mapping(vic_domain, hwirq);
			res[i].end = res[1].start;
		}
	} else {
		res_n = 1;
	}

	spec_fpga_app_id_build(spec, SPEC_FPGA_APP_OFF,
				    app_name, SPEC_FPGA_APP_NAME_MAX);
	pdev = platform_device_register_resndata(&spec->dev,
						 app_name, app_id++,
						 res, res_n,
						 NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	spec->app_pdev = pdev;

	return 0;
}

static void spec_fpga_app_exit(struct spec_dev *spec)
{
	if (spec->app_pdev) {
		platform_device_unregister(spec->app_pdev);
		spec->app_pdev = NULL;
	}
}


static bool spec_fpga_is_valid(struct spec_dev *spec)
{
	if ((spec->meta->bom & SPEC_META_BOM_END_MASK) != SPEC_META_BOM_LE) {
		dev_err(spec->dev.parent,
			"Expected Little Endian devices BOM: 0x%x\n",
		       spec->meta->bom);
		return false;
	}

	if ((spec->meta->bom & SPEC_META_BOM_VER_MASK) != 0) {
		dev_err(spec->dev.parent,
			"Unknow Metadata specification version BOM: 0x%x\n",
			spec->meta->bom);
		return false;
	}

	if (spec->meta->vendor != SPEC_META_VENDOR_ID ||
	    spec->meta->device != SPEC_META_DEVICE_ID) {
		dev_err(spec->dev.parent,
			"Unknow vendor/device ID: %08x:%08x\n",
			spec->meta->vendor, spec->meta->device);
		return false;
	}

	if (spec->meta->version != SPEC_META_VERSION_1_4) {
		dev_err(spec->dev.parent,
			"Unknow version: %08x\n", spec->meta->version);
		return false;
	}

	return true;
}


/**
 * Initialize carrier devices on FPGA
 */
int spec_fpga_init(struct spec_dev *spec)
{
	int err;

	spec->fpga = spec->remap[0];
	spec->meta = spec->fpga + SPEC_META_BASE;

	if (!spec_fpga_is_valid(spec))
		return -EINVAL;

	err = spec_fpga_therm_init(spec);
	if (err)
		goto err_therm;
	err = spec_fpga_vic_init(spec);
	if (err)
		goto err_vic;
	err = spec_fpga_devices_init(spec);
	if (err)
		goto err_dev;
	err = spec_fmc_init(spec);
	if (err)
		goto err_fmc;
	err = spec_fpga_app_init(spec);
	if (err)
		goto err_app;

	return 0;

err_app:
	spec_fmc_exit(spec);
err_fmc:
	spec_fpga_devices_exit(spec);
err_dev:
	spec_fpga_vic_exit(spec);
err_vic:
	spec_fpga_therm_exit(spec);
err_therm:
	return err;
}

int spec_fpga_exit(struct spec_dev *spec)
{
	spec_fpga_app_exit(spec);
	spec_fmc_exit(spec);
	spec_fpga_devices_exit(spec);
	spec_fpga_vic_exit(spec);
	spec_fpga_therm_exit(spec);

	return 0;
}
