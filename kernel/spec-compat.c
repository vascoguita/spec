// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/fpga/fpga-mgr.h>
#include <linux/version.h>
#include "spec-compat.h"

int compat_get_fpga_last_word_size(struct fpga_image_info *info, size_t count)
{
	int last_word_size = count;

#if KERNEL_VERSION(4,16,0) <= LINUX_VERSION_CODE
	if (info)
		last_word_size = info->count;
#endif

	return last_word_size;
}

#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE
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


#if KERNEL_VERSION(4,18,0) >= LINUX_VERSION_CODE
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
	return !!mgr;
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
