// SPDX-License-Identifier: GPL-2.0-or-later
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
#define SPEC_FMC_PRESENCE (SPEC_CORE_FPGA + 0x64)

static inline u8 spec_fmc_presence(struct spec_dev *spec)
{
	return (ioread32(spec->fpga + SPEC_FMC_PRESENCE) & 0x1);
}

static int spec_fmc_is_present(struct fmc_carrier *carrier,
			       struct fmc_slot *slot)
{
	struct spec_dev *spec = carrier->priv;

	return spec_fmc_presence(spec);
}

static const struct fmc_carrier_operations spec_fmc_ops = {
	.owner = THIS_MODULE,
	.is_present = spec_fmc_is_present,
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

	/* We have a muxed I2C master */
	if (&spec->dev != adap_parent->dev.parent->parent)
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

	spec->slot_info.i2c_bus_nr = spec_i2c_get_bus(spec);
	if (spec->slot_info.i2c_bus_nr <= 0)
		return -ENODEV;
	spec->slot_info.ga = 0;
	spec->slot_info.lun = 1;

	err = fmc_carrier_register(&spec->dev, &spec_fmc_ops,
				   SPEC_FMC_SLOTS, &spec->slot_info,
				   spec);
	if (err) {
		dev_err(spec->dev.parent,
			"Failed to register as FMC carrier\n");
		goto err_fmc;
	}


	return 0;

err_fmc:
	return err;
}

int spec_fmc_exit(struct spec_dev *spec)
{
	return fmc_carrier_unregister(&spec->dev);
}
