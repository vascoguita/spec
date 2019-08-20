/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#ifndef __SPEC_COMPAT_H__
#define __SPEC_COMPAT_H__
#include <linux/fpga/fpga-mgr.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/gpio/driver.h>
#include "spec.h"

#if KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE
#if KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE
/* So that we select the buffer size because smaller */
#define compat_fpga_ops_initial_header_size .initial_header_size = 0xFFFFFFFF,
#else
#define compat_fpga_ops_initial_header_size .initial_header_size = 0,
#endif
#else
#define compat_fpga_ops_initial_header_size
#endif

#if KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
#define compat_fpga_ops_groups
#else
#define compat_fpga_ops_groups .groups = NULL,
#endif

#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
struct fpga_image_info;
#endif

int spec_fpga_write_init(struct fpga_manager *mgr,
			 struct fpga_image_info *info,
			 const char *buf, size_t count);
int spec_fpga_write_complete(struct fpga_manager *mgr,
			     struct fpga_image_info *info);

#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
int compat_spec_fpga_write_init(struct fpga_manager *mgr, u32 flags,
				const char *buf, size_t count);
int compat_spec_fpga_write_complete(struct fpga_manager *mgr, u32 flags);
#else
int compat_spec_fpga_write_init(struct fpga_manager *mgr,
				struct fpga_image_info *info,
				const char *buf, size_t count);
int compat_spec_fpga_write_complete(struct fpga_manager *mgr,
				    struct fpga_image_info *info);
#endif
int compat_get_fpga_last_word_size(struct fpga_image_info *info,
				   size_t count);
int compat_spec_fw_load(struct spec_gn412x *spec_gn412x, const char *name);

#if KERNEL_VERSION(3, 11, 0) > LINUX_VERSION_CODE
#define __ATTR_RW(_name) __ATTR(_name, (S_IWUSR | S_IRUGO),	\
			 _name##_show, _name##_store)
#define __ATTR_WO(_name) {						\
	.attr	= { .name = __stringify(_name), .mode = S_IWUSR },	\
	.store	= _name##_store,					\
}
#define DEVICE_ATTR_RW(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RW(_name)
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define DEVICE_ATTR_WO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_WO(_name)
#endif


#if KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE
#define GPIO_PERSISTENT (0 << 3)
#endif

extern int compat_gpiod_add_lookup_table(struct gpiod_lookup_table *table);

#if KERNEL_VERSION(4, 3, 0) > LINUX_VERSION_CODE
extern void gpiod_remove_lookup_table(struct gpiod_lookup_table *table);
#endif

#endif /* __SPEC_COMPAT_H__ */
