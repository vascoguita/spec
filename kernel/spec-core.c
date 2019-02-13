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

#include "spec.h"

static void spec_release(struct device *dev)
{

}

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
	spec->dev.release = spec_release;
	err = dev_set_name(&spec->dev, "spec-%s", dev_name(&pdev->dev));
	if (err)
		goto err_name;

	err = device_register(&spec->dev);
	if (err)
		goto err_dev;

	err = spec_fpga_init(spec);
	if (err)
		goto err_fpga;

	err = spec_irq_init(spec);
	if (err)
		goto err_irq;

	pci_set_drvdata(pdev, spec);
	dev_info(spec->dev.parent, "Spec registered devptr=0x%p\n", spec->dev.parent);

	return 0;

err_irq:
	spec_fpga_exit(spec);
err_fpga:
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


	spec_irq_exit(spec);
	spec_fpga_exit(spec);

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
MODULE_VERSION(GIT_VERSION);
MODULE_DESCRIPTION("spec driver");

ADDITIONAL_VERSIONS;
