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


/**
 * It configures the gennum chip
 * @spec spec device instance
 * Return: 0 on success, otherwise a negative errno number
 */
static int gennum_config(struct spec_dev *spec)
{
	/* Put our 6 pins to a sane state (4 test points, 2 from FPGA) */
	gennum_mask_val(spec, 0xfc0, 0x000, GNGPIO_BYPASS_MODE); /* no AF */
	gennum_mask_val(spec, 0xfc0, 0xfc0, GNGPIO_DIRECTION_MODE); /* input */
	gennum_writel(spec, 0xffff, GNGPIO_INT_MASK_SET); /* disable */

	return 0;
}


static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_dev *spec;
	int err, i;

	spec = kzalloc(sizeof(struct spec_dev), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;
	spec->pdev = pdev;

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

	err = spec_fpga_init(spec);
	if (err)
		goto err_fpga;

	err = spec_fmc_init(spec);
	if (err)
		goto err_fmc;

	/* Enable the PCI device */
	pci_set_drvdata(pdev, spec);
	err = pci_enable_device(pdev);
	if (err)
		goto err_enable;
	pci_set_master(pdev);

	err = gennum_config(spec);
	if (err)
		goto err_gennum;

	return 0;

err_gennum:
	pci_disable_device(pdev);
err_enable:
	spec_fmc_exit(spec);
err_fmc:
	spec_fpga_exit(spec);
err_fpga:
	pci_set_drvdata(pdev, NULL);
	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
err_remap:
	kfree(spec);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_dev *spec = pci_get_drvdata(pdev);
	int i;

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);

	spec_fpga_exit(spec);
	spec_fmc_exit(spec);

	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
	kfree(spec);
}


static DEFINE_PCI_DEVICE_TABLE(spec_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_45T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_100T)},
	{0,},
};


static struct pci_driver spec_driver = {
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
