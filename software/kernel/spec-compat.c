// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/mfd/core.h>
#include <linux/version.h>
#include <linux/gpio/driver.h>
#include "spec-compat.h"



#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
struct fpga_manager *__fpga_mgr_get(struct device *dev)
{
	struct fpga_manager *mgr;
	int ret = -ENODEV;

	mgr = to_fpga_manager(dev);
	if (!mgr)
		goto err_dev;

	/* Get exclusive use of fpga manager */
	if (!mutex_trylock(&mgr->ref_mutex)) {
		ret = -EBUSY;
		goto err_dev;
	}

	if (!try_module_get(dev->parent->driver->owner))
		goto err_ll_mod;

	return mgr;

err_ll_mod:
	mutex_unlock(&mgr->ref_mutex);
err_dev:
	put_device(dev);
	return ERR_PTR(ret);
}

static int fpga_mgr_dev_match(struct device *dev, const void *data)
{
	return dev->parent == data;
}

/**
 * fpga_mgr_get - get an exclusive reference to a fpga mgr
 * @dev:parent device that fpga mgr was registered with
 *
 * Given a device, get an exclusive reference to a fpga mgr.
 *
 * Return: fpga manager struct or IS_ERR() condition containing error code.
 */
struct fpga_manager *fpga_mgr_get(struct device *dev)
{
	struct class *fpga_mgr_class = (struct class *) kallsyms_lookup_name("fpga_mgr_class");
	struct device *mgr_dev;

	mgr_dev = class_find_device(fpga_mgr_class, NULL, dev, fpga_mgr_dev_match);
	if (!mgr_dev)
		return ERR_PTR(-ENODEV);

	return __fpga_mgr_get(mgr_dev);
}
#endif


static int __compat_spec_fw_load(struct fpga_manager *mgr, const char *name)
{
#if KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
	return fpga_mgr_firmware_load(mgr, 0, name);
#else
	struct fpga_image_info image;

	memset(&image, 0, sizeof(image));
	return fpga_mgr_firmware_load(mgr, &image, name);
#endif
#else
	struct fpga_image_info image;

	memset(&image, 0, sizeof(image));
	image.firmware_name = (char *)name;
	image.dev = mgr->dev.parent;

	return fpga_mgr_load(mgr, &image);
#endif
}

struct mfd_find_data {
	const char *name;
	int id;
};

static int mfd_find_device_match(struct device *dev, void *data)
{
	struct mfd_find_data *d = data;
	struct platform_device *pdev = to_platform_device(dev);

	if (strncmp(d->name, mfd_get_cell(pdev)->name,
		    strnlen(d->name, 32)) != 0)
		return 0;

	if (d->id >= 0 && pdev->id != d->id)
		return 0;

	return 1;
}

static struct platform_device *mfd_find_device(struct device *parent,
					       const char *name,
					       int id)
{
	struct mfd_find_data d = {name, id};
	struct device *dev;

	dev = device_find_child(parent, (void *)&d, mfd_find_device_match);
	if (!dev)
		return NULL;

	return to_platform_device(dev);
}

int compat_spec_fw_load(struct spec_dev *spec, const char *name)
{
	struct fpga_manager *mgr;
	struct platform_device *fpga_pdev;
	int err;

	fpga_pdev = mfd_find_device(&spec->dev, "gn412x-fcl", -1);
	if (!fpga_pdev)
		return -ENODEV;
	mgr = fpga_mgr_get(&fpga_pdev->dev);
	if (IS_ERR(mgr))
		return -ENODEV;

	err = fpga_mgr_lock(mgr);
	if (err)
		goto out;
	err = __compat_spec_fw_load(mgr, name);
	fpga_mgr_unlock(mgr);
out:
	fpga_mgr_put(mgr);

	return err;
}


int compat_gpiod_add_lookup_table(struct gpiod_lookup_table *table)
{
	void (*gpiod_add_lookup_table_p)(struct gpiod_lookup_table *table);

	gpiod_add_lookup_table_p = (void *) kallsyms_lookup_name("gpiod_add_lookup_table");

	if (gpiod_add_lookup_table_p)
		gpiod_add_lookup_table_p(table);
	else
		return WARN(1, "Cannot find 'gpiod_add_lookup_table'");
	return 0;
}

#if KERNEL_VERSION(4, 3, 0) > LINUX_VERSION_CODE
void gpiod_remove_lookup_table(struct gpiod_lookup_table *table)
{
	struct mutex *gpio_lookup_lock_p = (void *) kallsyms_lookup_name("gpio_lookup_lock");

	mutex_lock(gpio_lookup_lock_p);

	list_del(&table->list);

	mutex_unlock(gpio_lookup_lock_p);
}
#endif
