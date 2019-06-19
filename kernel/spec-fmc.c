// SPDX-License-Identifier: GPLv2
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <linux/bitops.h>
#include <linux/fmc.h>
#include <linux/platform_data/i2c-ocores.h>

#include "spec.h"
#include "spec-compat.h"

#define SPEC_FMC_SLOTS 1

#define SPEC_I2C_SIZE		32
#define SPEC_I2C_ADDR_START	0x14000
#define SPEC_I2C_ADDR_END	((SPEC_I2C_ADDR_START + SPEC_I2C_SIZE) - 1)

static inline u8 spec_fmc_slot_nr(struct spec_dev *spec)
{
	return (ioread32be(spec->remap[0] + 0x0) & 0x1);
}

static inline u8 spec_fmc_presence(struct spec_dev *spec)
{
	return (ioread32(spec->remap[0] + 0x0) & 0x1);
}

static int spec_fmc_is_present(struct fmc_carrier *carrier,
			       struct fmc_slot *slot)
{
	struct spec_dev *spec = carrier->priv;

	return (spec_fmc_presence(spec) & BIT(slot->ga));
}

static const struct fmc_carrier_operations spec_fmc_ops = {
	.owner = THIS_MODULE,
	.is_present = spec_fmc_is_present,
};


static const struct ocores_i2c_platform_data pdata = {
	.reg_shift = 2, /* 32bit aligned */
	.reg_io_width = 4,
	.clock_khz = 62500,
	.big_endian = 1,
	.num_devices = 0,
	.devices = NULL,
};

static int id;

/**
 * It builds the platform_device_info necessary to register the
 * I2C master device.
 * @spec the SPEC instance
 *
 * Return: an array of I2C master devices
 */
static int spec_i2c_add(struct spec_dev *spec)
{
	struct resource res = {
		.name = "i2c-ocores-mem",
		.flags = IORESOURCE_MEM,
		.start = pci_resource_start(to_pci_dev(spec->dev.parent), 0) + SPEC_I2C_ADDR_START,
		.end = pci_resource_start(to_pci_dev(spec->dev.parent), 0) + SPEC_I2C_ADDR_END,
	};

	/* FIXME find better ID */
	spec->i2c_pdev = platform_device_register_resndata(spec->dev.parent,
							   "i2c-ohwr", id++,
							   &res, 1,
							   &pdata,
							   sizeof(pdata));
	if (!spec->i2c_pdev)
		return -ENODEV;

	return 0;
}

static void spec_i2c_del(struct spec_dev *spec)
{
	if (spec->i2c_pdev)
		platform_device_unregister(spec->i2c_pdev);
}

static int spec_i2c_find_adapter(struct device *dev, void *data)
{
	struct spec_dev *spec = data;
	struct i2c_adapter *adap, *adap_parent;

	if (dev->type != &i2c_adapter_type)
		return 0;

	adap = to_i2c_adapter(dev);
	adap_parent = i2c_parent_is_i2c_adapter(adap);
	if (!adap_parent)
		return 0;

	/* We have a muxed I2C master */
	if (&spec->i2c_pdev->dev != adap_parent->dev.parent)
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
static int spec_i2c_get_bus(struct spec_dev *spec)
{
	return i2c_for_each_dev(spec, spec_i2c_find_adapter);
}

/**
 * Create an FMC interface
 */
int spec_fmc_init(struct spec_dev *spec)
{
	int err;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (spec_fmc_slot_nr(spec) != SPEC_FMC_SLOTS) {
		dev_err(spec->dev.parent,
			"Invalid SPEC FPGA (slot count: %d)\n",
			spec_fmc_slot_nr(spec));
		return -EINVAL;
	}

	pr_info("%s:%d\n", __func__, __LINE__);
	err = spec_i2c_add(spec);
	if (err)
		goto err_i2c;
	pr_info("%s:%d\n", __func__, __LINE__);
	spec->slot_info.i2c_bus_nr = spec_i2c_get_bus(spec);
	if (spec->slot_info.i2c_bus_nr <= 0)
		goto err_i2c_bus;
	spec->slot_info.ga = 0;
	spec->slot_info.lun = 1;
	pr_info("%s:%d\n", __func__, __LINE__);
	err = fmc_carrier_register(&spec->dev, &spec_fmc_ops,
				   spec_fmc_slot_nr(spec), &spec->slot_info,
				   spec);
	if (err) {
		dev_err(spec->dev.parent,
			"Failed to register as FMC carrier\n");
		goto err_fmc;
	}


	return 0;

err_fmc:
err_i2c_bus:
	spec_i2c_del(spec);
err_i2c:
	return -1;
}

int spec_fmc_exit(struct spec_dev *spec)
{
	int err;

	err = fmc_carrier_unregister(&spec->dev);
	if (err)
		dev_err(spec->dev.parent,
			"Failed to unregister from FMC\n");
	spec_i2c_del(spec);
	return err;
}
