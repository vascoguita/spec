// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * Driver for SPEC (Simple PCI FMC carrier) board.
 */

#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/mfd/core.h>

#include "platform_data/gn412x-gpio.h"
#include "spec.h"
#include "spec-compat.h"

static int mfd_id; /* FIXME look for something better */

static char *spec_fw_name_45t = "spec-golden-45T.bin";
static char *spec_fw_name_100t = "spec-golden-100T.bin";
static char *spec_fw_name_150t = "spec-golden-150T.bin";

char *spec_fw_name = "";
module_param_named(fw_name, spec_fw_name, charp, 0444);


static struct gn412x_platform_data gn412x_gpio_pdata = {
	.int_cfg = 0,
};

static struct resource gn412x_gpio_res[] = {
	{
		.name = "gn412x-gpio-mem",
		.flags = IORESOURCE_MEM,
		.start = 0,
		.end = 0x1000 - 1,
	}, {
		.name = "gn412x-gpio-irq",
		.flags = IORESOURCE_IRQ,
		.start = 0,
		.end = 0,
	}
};

static struct resource gn412x_fcl_res[] = {
	{
		.name = "gn412x-fcl-mem",
		.flags = IORESOURCE_MEM,
		.start = 0,
		.end = 0x1000 - 1,
	}, {
		.name = "gn412x-fcl-irq",
		.flags = IORESOURCE_IRQ,
		.start = 0,
		.end = 0,
	}
};

enum spec_mfd_enum {
	SPEC_MFD_GN412X_GPIO = 0,
	SPEC_MFD_GN412X_FCL,
};

static const struct mfd_cell spec_mfd_devs[] = {
	[SPEC_MFD_GN412X_GPIO] = {
		.name = "gn412x-gpio",
		.platform_data = &gn412x_gpio_pdata,
		.pdata_size = sizeof(gn412x_gpio_pdata),
		.num_resources = ARRAY_SIZE(gn412x_gpio_res),
		.resources = gn412x_gpio_res,
	},
	[SPEC_MFD_GN412X_FCL] = {
		.name = "gn412x-fcl",
		.platform_data = NULL,
		.pdata_size = 0,
		.num_resources = ARRAY_SIZE(gn412x_fcl_res),
		.resources = gn412x_fcl_res,
	},
};


/**
 * Return the SPEC defult FPGA firmware name based on PCI ID
 * @spec: SPEC device
 *
 * Return: FPGA firmware name
 */
static const char *spec_fw_name_init_get(struct spec_gn412x *spec_gn412x)
{
	if (strlen(spec_fw_name) > 0)
		return spec_fw_name;

	switch (spec_gn412x->pdev->device) {
	case PCI_DEVICE_ID_SPEC_45T:
		return spec_fw_name_45t;
	case PCI_DEVICE_ID_SPEC_100T:
		return spec_fw_name_100t;
	case PCI_DEVICE_ID_SPEC_150T:
		return spec_fw_name_150t;
	default:
		return NULL;
	}
}

/**
 * Load FPGA code
 * @spec: SPEC device
 * @name: FPGA bitstream file name
 *
 * Return: 0 on success, otherwise a negative error number
 */
int spec_fw_load(struct spec_gn412x *spec_gn412x, const char *name)
{
	enum spec_fpga_select sel;
	int err;

	err = spec_fpga_exit(spec_gn412x);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Cannot remove FPGA device instances. Try to remove them manually and to reload this device instance\n");
		return err;
	}


	mutex_lock(&spec_gn412x->mtx);
	sel = spec_gpio_fpga_select_get(spec_gn412x);

	spec_gpio_fpga_select_set(spec_gn412x, SPEC_FPGA_SELECT_GN4124_FPGA);

	err = compat_spec_fw_load(spec_gn412x, name);
	if (err)
		goto out;

	err = spec_fpga_init(spec_gn412x);
	if (err)
		dev_warn(&spec_gn412x->pdev->dev,
			 "FPGA incorrectly programmed\n");

out:
	spec_gpio_fpga_select_set(spec_gn412x, sel);
	mutex_unlock(&spec_gn412x->mtx);

	return err;
}

static ssize_t bootselect_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	enum spec_fpga_select sel;

	if (strncmp("fpga-flash", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_FPGA_FLASH;
	} else if (strncmp("gn4124-fpga", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_GN4124_FPGA;
	} else if (strncmp("gn4124-flash", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_GN4124_FLASH;
	} else {
		dev_err(dev, "Unknown bootselect option '%s'\n",
			buf);
		return -EINVAL;
	}

	mutex_lock(&spec_gn412x->mtx);
	spec_gpio_fpga_select_set(spec_gn412x, sel);
	mutex_unlock(&spec_gn412x->mtx);

	return count;

}
static ssize_t bootselect_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	enum spec_fpga_select sel;

	sel = spec_gpio_fpga_select_get(spec_gn412x);
	switch (sel) {
	case SPEC_FPGA_SELECT_FPGA_FLASH:
		return snprintf(buf, PAGE_SIZE, "fpga-flash\n");
	case SPEC_FPGA_SELECT_GN4124_FPGA:
		return snprintf(buf, PAGE_SIZE, "gn4124-fpga\n");
	case SPEC_FPGA_SELECT_GN4124_FLASH:
		return snprintf(buf, PAGE_SIZE, "gn4124-flash\n");
	default:
		dev_err(dev, "Unknown bootselect option '%x'\n",
			sel);
		return -EINVAL;
	}
}
static DEVICE_ATTR(bootselect, 0744, bootselect_show, bootselect_store);

/**
 * Load golden bitstream on FGPA
 */
static ssize_t load_golden_fpga_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	int err;

	err = spec_fw_load(spec_gn412x, spec_fw_name_init_get(spec_gn412x));

	return err < 0 ? err : count;
}
static DEVICE_ATTR_WO(load_golden_fpga);

static struct attribute *gn412x_fpga_attrs[] = {
	&dev_attr_load_golden_fpga.attr,
	&dev_attr_bootselect.attr,
	NULL,
};

static const struct attribute_group gn412x_fpga_group = {
	.name = "fpga-options",
	.attrs = gn412x_fpga_attrs,
};

static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_gn412x *spec_gn412x;
	int err;

	spec_gn412x = kzalloc(sizeof(*spec_gn412x), GFP_KERNEL);
	if (!spec_gn412x)
		return -ENOMEM;

	mutex_init(&spec_gn412x->mtx);
	pci_set_drvdata(pdev, spec_gn412x);
	spec_gn412x->pdev = pdev;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable PCI device (%d)\n",
		        err);
		goto err_enable;
	}

	pci_set_master(pdev);
	err = mfd_add_devices(&pdev->dev, mfd_id++,
			      spec_mfd_devs,
			      ARRAY_SIZE(spec_mfd_devs),
			      &pdev->resource[4], pdev->irq, NULL);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to add MFD devices (%d)\n",
			err);
		goto err_mfd;
	}

	err = spec_gpio_init(spec_gn412x);
	if (err) {
		dev_err(&pdev->dev, "Failed to get GPIOs (%d)\n", err);
		goto err_sgpio;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &gn412x_fpga_group);
	if (err)
		goto err_sysfs;

	spec_dbg_init(spec_gn412x);

	mutex_lock(&spec_gn412x->mtx);
	err = spec_fpga_init(spec_gn412x);
	if (err)
		dev_warn(&pdev->dev,
			 "FPGA incorrectly programmed or empty (%d)\n", err);
	mutex_unlock(&spec_gn412x->mtx);

	return 0;

err_sysfs:
	spec_gpio_exit(spec_gn412x);
err_sgpio:
	mfd_remove_devices(&pdev->dev);
err_mfd:
	pci_disable_device(pdev);
err_enable:
	kfree(spec_gn412x);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);

	spec_dbg_exit(spec_gn412x);
	spec_fpga_exit(spec_gn412x);
	sysfs_remove_group(&pdev->dev.kobj, &gn412x_fpga_group);
	spec_gpio_exit(spec_gn412x);

	mfd_remove_devices(&pdev->dev);
	pci_disable_device(pdev);
	kfree(spec_gn412x);
}


static const struct pci_device_id spec_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_45T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_100T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_150T)},
	{0,},
};


static struct pci_driver spec_driver = {
	.driver = {
		.owner = THIS_MODULE,
	},
	.name = "spec-fmc-carrier",
	.probe = spec_probe,
	.remove = spec_remove,
	.id_table = spec_pci_tbl,
};

module_pci_driver(spec_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Driver for the 'Simple PCIe FMC Carrier' a.k.a. SPEC");
MODULE_DEVICE_TABLE(pci, spec_pci_tbl);

ADDITIONAL_VERSIONS;
