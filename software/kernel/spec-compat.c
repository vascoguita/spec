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


static int mfd_find_device_match(struct device *dev, void *data)
{
	struct mfd_cell *cell = data;
	struct platform_device *pdev = to_platform_device(dev);

	return (strncmp(cell->name, mfd_get_cell(pdev)->name,
			strnlen(cell->name, 32)) == 0);
}

static struct device *mfd_find_device(struct device *parent,
				      const struct mfd_cell *cell)
{
	return device_find_child(parent, (void *)cell, mfd_find_device_match);
}

int compat_spec_fw_load(struct spec_dev *spec, const struct mfd_cell *cell,
			const char *name)
{
	struct fpga_manager *mgr;
	struct device *fpga_dev;
	int err;

	fpga_dev = mfd_find_device(&spec->dev, cell);
	if (!fpga_dev)
		return -ENODEV;
	mgr = fpga_mgr_get(fpga_dev);
	if (IS_ERR(mgr))
		return -ENODEV;

	err = __compat_spec_fw_load(mgr, name);
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
