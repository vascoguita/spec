// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/fpga/fpga-mgr.h>
#include <linux/types.h>
#include <linux/version.h>

#if KERNEL_VERSION(4,10,0) <= LINUX_VERSION_CODE
#if KERNEL_VERSION(4,16,0) > LINUX_VERSION_CODE
/* So that we select the buffer size because smaller */
#define compat_fpga_ops_initial_header_size .initial_header_size = 0xFFFFFFFF,
#else
#define compat_fpga_ops_initial_header_size .initial_header_size = 0,
#endif
#else
#define compat_fpga_ops_initial_header_size
#endif

#if KERNEL_VERSION(4,16,0) <= LINUX_VERSION_CODE
#define compat_fpga_ops_groups .groups = NULL,
#else
#define compat_fpga_ops_groups
#endif

#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE
struct fpga_image_info;
#endif

int spec_fpga_write_init(struct fpga_manager *mgr,
			 struct fpga_image_info *info,
			 const char *buf, size_t count);
int spec_fpga_write_complete(struct fpga_manager *mgr,
			     struct fpga_image_info *info);

#if KERNEL_VERSION(4,10,0) > LINUX_VERSION_CODE
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
struct fpga_manager *compat_fpga_mgr_create(struct device *dev,
					    const char *name,
					    const struct fpga_manager_ops *mops,
					    void *priv);
void compat_fpga_mgr_free(struct fpga_manager *mgr);
int compat_fpga_mgr_register(struct fpga_manager *mgr);
void compat_fpga_mgr_unregister(struct fpga_manager *mgr);
