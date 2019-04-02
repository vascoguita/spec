/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Driver for SPEC (Simple PCI FMC carrier) board.
 */

#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>

#include "spec.h"
#include "spec-compat.h"

static char *spec_fw_name_45t = "spec-init-45T.bin";
static char *spec_fw_name_100t = "spec-init-100T.bin";
static char *spec_fw_name_150t = "spec-init-150T.bin";

char *spec_fw_name = "";
module_param_named(fw_name, spec_fw_name, charp, 0444);

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
static int spec_fw_load(struct spec_dev *spec, const char *name)
{
	return compat_spec_fw_load(spec, name);
}

/**
 * Return: True if the FPGA is programmed, otherwise false
 */
static bool spec_fw_is_pre_programmed(struct spec_dev *spec)
{

	return false;
}

/**
 * Load default FPGA code
 * @spec: SPEC device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static int spec_fw_load_init(struct spec_dev *spec)
{
	if (spec_fw_is_pre_programmed(spec))
		return 0;

	return spec_fw_load(spec, spec_fw_name_init_get(spec));
}

static void spec_release(struct device *dev)
{

}

static int spec_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return 0;
}

static const struct device_type spec_dev_type = {
	.name = "spec",
	.release = spec_release,
	.uevent = spec_uevent,
};

static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_dev *spec;
	int err, i;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

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

	spec->gn412x.mem = spec->remap[2];
	err = gn412x_gpio_init(&spec->gn412x);
	if (err)
		goto err_ggpio;

	err = spec_gpio_init(spec);
	if (err)
		goto err_sgpio;

	err = spec_fpga_init(spec);
	if (err)
		goto err_fpga;

	err = spec_irq_init(spec);
	if (err)
		goto err_irq;

	err = spec_fw_load_init(spec);
	if (err)
		goto err_fw;

	err = spec_fmc_init(spec);
	if (err)
		goto err_fmc;

	pci_set_drvdata(pdev, spec);
	dev_info(spec->dev.parent, "Spec registered devptr=0x%p\n", spec->dev.parent);

	spec_dbg_init(spec);

	return 0;

err_fmc:
err_fw:
	spec_irq_exit(spec);
err_irq:
	spec_fpga_exit(spec);
err_fpga:
	spec_gpio_exit(spec);
err_sgpio:
	gn412x_gpio_exit(&spec->gn412x);
err_ggpio:
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
	spec_fmc_exit(spec);
	spec_irq_exit(spec);
	spec_fpga_exit(spec);
	spec_gpio_exit(spec);
	gn412x_gpio_exit(&spec->gn412x);

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
	.name = "spec",
	.probe = spec_probe,
	.remove = spec_remove,
	.id_table = spec_pci_tbl,
};

module_pci_driver(spec_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("spec driver");
MODULE_DEVICE_TABLE(pci, spec_pci_tbl);

ADDITIONAL_VERSIONS;
