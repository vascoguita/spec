/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <linux/kernel.h>
#include <linux/platform_data/i2c-ocores.h>

#include "spec.h"


static int spec_fmc_is_present(struct fmc_carrier *carrier,
			       struct fmc_slot *slot)
{
	/* TODO implement me */
	return 0;
}

static const struct fmc_carrier_operations spec_fmc_ops = {
	.is_present = spec_fmc_is_present,
};

static const struct ocores_i2c_platform_data pdata = {
	.reg_shift = 0,
	.reg_io_width = 4,
	.clock_khz = 62500,
	.big_endian = 0,
	.num_devices = 0,
	.devices = NULL,
};


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

	if (&spec->i2c_pdev->dev != adap_parent->dev.parent)
		return 0;

	/* Found! */
	return i2c_adapter_id(adap);
}


/**
 * Get the I2C adapter associated with an FMC slot
 * @spec: SPEC instance
 *
 * Return: the I2C bus to be used
 */
static int spec_i2c_get_bus(struct spec_dev *spec)
{
	return i2c_for_each_dev(spec, spec_i2c_find_adapter);
}


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
	};
	int err;

	/* VME function 1 */
	res.start = pci_resource_start(to_pci_dev(spec->pdev->dev.parent), 0);
	res.start += SPEC_I2C_MASTER_ADDR;
	res.end = res.start + SPEC_I2C_MASTER_SIZE;

	/* FIXME find better ID */
	spec->i2c_pdev = platform_device_register_resndata(&spec->pdev->dev,
							   "ocores-i2c", id++,
							   &res, 1,
							   &pdata,
							   sizeof(pdata));
	if (!spec->i2c_pdev)
		return -ENODEV;

	err = i2c_for_each_dev(spec, spec_i2c_find_adapter);
	if (err <= 0)
		goto err;
	return 0;

err:
	platform_device_unregister(spec->i2c_pdev);
	return -ENODEV;
}

static void spec_i2c_del(struct spec_dev *spec)
{
	platform_device_unregister(spec->i2c_pdev);
	if (spec->i2c_adapter)
		i2c_put_adapter(spec->i2c_adapter);
}


int spec_fmc_init(struct spec_dev *spec)
{
	int err;

	return 0; /* UNTIL WE TEST IT */

	if (!spec)
		return -EINVAL;

	err = spec_i2c_add(spec);
	if (err)
		goto err_i2c;

	spec->slot_info.i2c_bus_nr = spec_i2c_get_bus(spec);
	if (spec->slot_info.i2c_bus_nr <= 0)
		goto err_i2c_bus;
	spec->slot_info.ga = 0;
	spec->slot_info.lun = 0;
	err = fmc_carrier_register(&spec->pdev->dev, &spec_fmc_ops,
				   SPEC_FMC_SLOTS, &spec->slot_info, spec);
	if (err)
		goto err_fmc;

	return 0;

err_fmc:
err_i2c_bus:
	spec_i2c_del(spec);
err_i2c:
	return err;
}

void spec_fmc_exit(struct spec_dev *spec)
{
	return; /* UNTIL WE TEST IT */

	if (!spec)
		return;
	fmc_carrier_unregister(&spec->pdev->dev);
	spec_i2c_del(spec);
}
