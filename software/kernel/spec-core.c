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
static const char *spec_fw_name_init_get(struct spec_dev *spec)
{
	struct pci_dev *pdev = to_pci_dev(spec->dev.parent);

	if (strlen(spec_fw_name) > 0)
		return spec_fw_name;

	switch (pdev->device) {
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
int spec_fw_load(struct spec_dev *spec, const char *name)
{
	int err;

	err = spec_core_fpga_exit(spec);
	if (err) {
		dev_err(&spec->dev,
			"Cannot remove FPGA device instances. Try to remove them manually and to reload this device instance\n");
		return err;
	}


	spec_gpio_fpga_select(spec, SPEC_FPGA_SELECT_GN4124);

	err = compat_spec_fw_load(spec, &spec_mfd_devs[SPEC_MFD_GN412X_FCL], name);
	if (err)
		goto out;

	err = spec_core_fpga_init(spec);
	if (err)
		dev_warn(&spec->dev, "FPGA incorrectly programmed\n");

out:
	spec_gpio_fpga_select(spec, SPEC_FPGA_SELECT_FLASH);

	return err;
}

static void spec_release(struct device *dev)
{

}

static int spec_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return 0;
}

/**
 * Load golden bitstream on FGPA
 */
static ssize_t load_golden_fpga_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct spec_dev *spec = to_spec_dev(dev);
	int err;

	err = spec_fw_load(spec, spec_fw_name_init_get(spec));

	return err < 0 ? err : count;
}
static DEVICE_ATTR_WO(load_golden_fpga);

static struct attribute *spec_type_attrs[] = {
	&dev_attr_load_golden_fpga.attr,
	NULL,
};

static const struct attribute_group spec_type_group = {
	.attrs = spec_type_attrs,
};

static const struct attribute_group *spec_type_groups[] = {
	&spec_type_group,
	NULL,
};

static const struct device_type spec_dev_type = {
	.name = "spec",
	.release = spec_release,
	.uevent = spec_uevent,
	.groups = spec_type_groups,
};


static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_dev *spec;
	int err, i;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	pci_set_drvdata(pdev, spec);

	err = pci_enable_device(pdev);
	if (err)
		goto err_enable;

	pci_set_master(pdev);

	/* Remap our 3 bars */
	for (i = err = 0; i < 3; i++) {
		struct resource *r = pdev->resource + (2 * i);

		if (!r->start)
			continue;
		if (r->flags & IORESOURCE_MEM) {
			spec->remap[i] = ioremap(r->start,
						r->end + 1 - r->start);
			if (!spec->remap[i])
				err = -ENOMEM;
		}
	}
	if (err)
		goto err_remap;

	spec->dev.parent = &pdev->dev;
	spec->dev.type = &spec_dev_type;
	err = dev_set_name(&spec->dev, "spec-%s", dev_name(&pdev->dev));
	if (err)
		goto err_name;

	err = device_register(&spec->dev);
	if (err)
		goto err_dev;
	/* This virtual device is assciated with this driver */
	spec->dev.driver = pdev->dev.driver;

	err = mfd_add_devices(&spec->dev, mfd_id++,
			      spec_mfd_devs,
			      ARRAY_SIZE(spec_mfd_devs),
			      &pdev->resource[4], pdev->irq, NULL);
	if (err)
		goto err_mfd;

	err = spec_gpio_init(spec);
	if (err)
		goto err_sgpio;

	err = spec_core_fpga_init(spec);
	if (err)
		dev_warn(&spec->dev, "FPGA incorrectly programmed or empty\n");

	spec_dbg_init(spec);

	return 0;

err_sgpio:
	mfd_remove_devices(&spec->dev);
err_mfd:
	device_unregister(&spec->dev);
err_dev:
err_name:
	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
	}
err_remap:
	pci_disable_device(pdev);
err_enable:
	kfree(spec);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_dev *spec = pci_get_drvdata(pdev);
	int i;

	spec_dbg_exit(spec);
	spec_core_fpga_exit(spec);
	spec_gpio_exit(spec);

	mfd_remove_devices(&spec->dev);

	for (i = 0; i < 3; i++)
		if (spec->remap[i])
			iounmap(spec->remap[i]);
	device_unregister(&spec->dev);
	pci_disable_device(pdev);
	kfree(spec);
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
