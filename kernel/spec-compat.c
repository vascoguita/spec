// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/module.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/version.h>
#include "spec-compat.h"

int compat_get_fpga_last_word_size(struct fpga_image_info *info, size_t count)
{
#if KERNEL_VERSION(4,16,0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
	return count;
#else
	return info ? info->count : count;
#endif
}

#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
int compat_spec_fpga_write_init(struct fpga_manager *mgr,
				u32 flags,
				const char *buf, size_t count)
{
	return spec_fpga_write_init(mgr, NULL, buf, count);
}
int compat_spec_fpga_write_complete(struct fpga_manager *mgr,
				    u32 flags)
{
	return spec_fpga_write_complete(mgr, NULL);
}
#else
int compat_spec_fpga_write_init(struct fpga_manager *mgr,
				struct fpga_image_info *info,
				const char *buf, size_t count)
{
	return spec_fpga_write_init(mgr, info, buf, count);
}
int compat_spec_fpga_write_complete(struct fpga_manager *mgr,
				    struct fpga_image_info *info)
{
	return spec_fpga_write_complete(mgr, info);
}
#endif

#if KERNEL_VERSION(4,18,0) > LINUX_VERSION_CODE  && !defined(CONFIG_FPGA_MGR_BACKPORT)
struct fpga_manager *compat_fpga_mgr_create(struct device *dev, const char *name,
					    const struct fpga_manager_ops *mops,
					    void *priv)
{
	int err;

	err = fpga_mgr_register(dev, name, mops, priv);
	if (err)
		return NULL;
	return (struct fpga_manager *)dev;
}

void compat_fpga_mgr_free(struct fpga_manager *mgr)
{
	fpga_mgr_unregister((struct device *)mgr);
}

int compat_fpga_mgr_register(struct fpga_manager *mgr)
{
	return mgr ? 0 : 1;
}

void compat_fpga_mgr_unregister(struct fpga_manager *mgr)
{
	fpga_mgr_unregister((struct device *)mgr);
}
#else
struct fpga_manager *compat_fpga_mgr_create(struct device *dev,
					    const char *name,
					    const struct fpga_manager_ops *mops,
					    void *priv)
{
	return fpga_mgr_create(dev, name, mops, priv);
}

void compat_fpga_mgr_free(struct fpga_manager *mgr)
{
	fpga_mgr_free(mgr);
}

int compat_fpga_mgr_register(struct fpga_manager *mgr)
{
	return fpga_mgr_register(mgr);
}

void compat_fpga_mgr_unregister(struct fpga_manager *mgr)
{
	fpga_mgr_unregister(mgr);
}
#endif



#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE
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
static struct fpga_manager *fpga_mgr_get(struct device *dev)
{
	struct device *mgr_dev = class_find_device(fpga_mgr_class, NULL, dev,
						   fpga_mgr_dev_match);
	if (!mgr_dev)
		return ERR_PTR(-ENODEV);

	return __fpga_mgr_get(mgr_dev);
}
#endif

int compat_spec_fw_load(struct spec_dev *spec, const char *name)
{
	struct fpga_manager *mgr;
	struct fpga_image_info image;
	int err;

	mgr = fpga_mgr_get(&spec->dev);
	if (IS_ERR(mgr))
		return -ENODEV;

#if KERNEL_VERSION(4,16,0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE
	err = fpga_mgr_firmware_load(mgr, 0, name);
#else
	memset(&image, 0, sizeof(image));
	err = fpga_mgr_firmware_load(mgr, &image, name);
#endif
#else
	memset(&image, 0, sizeof(image));
	image.firmware_name = (char *)name;
	image.dev = &spec->dev;
	mgr = spec->mgr;

	err = fpga_mgr_load(mgr, &image);
#endif

	fpga_mgr_put(mgr);

	return err;
}
