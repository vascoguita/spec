// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 CERN
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <linux/module.h>
#include <linux/fs.h>

#include "spec.h"
#include "spec-compat.h"


static int spec_irq_dbg_info(struct seq_file *s, void *offset)
{
	struct spec_dev *spec = s->private;
	int i;

	seq_printf(s, "'%s':\n",dev_name(spec->dev.parent));

	seq_printf(s, "  redirect: %d\n", to_pci_dev(spec->dev.parent)->irq);
	seq_printf(s, "  irq-mapping:\n");
	for (i = 0; i < GN4124_GPIO_IRQ_MAX; ++i) {
		seq_printf(s, "    - hardware: %d\n", i);
		seq_printf(s, "      linux: %d\n",
			   irq_find_mapping(spec->gpio_domain, i));
	}

	return 0;
}

static int spec_irq_dbg_info_open(struct inode *inode, struct file *file)
{
	struct spec_dev *spec = inode->i_private;

	return single_open(file, spec_irq_dbg_info, spec);
}

static const struct file_operations spec_irq_dbg_info_ops = {
	.owner = THIS_MODULE,
	.open  = spec_irq_dbg_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t spec_dbg_fw_write(struct file *file,
				 const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct spec_dev *spec = file->private_data;
	int err;

	err = compat_spec_fw_load(spec, buf);
	if (err)
		return err;
	return count;
}

static int spec_dbg_fw_open(struct inode *inode, struct file *file)
{
	struct htvic_device *htvic = inode->i_private;

	file->private_data = htvic;

	return 0;
}

static const struct file_operations spec_dbg_fw_ops = {
	.owner = THIS_MODULE,
	.open  = spec_dbg_fw_open,
	.write = spec_dbg_fw_write,
};

/**
 * It initializes the debugfs interface
 * @spec: SPEC device instance
 *
 * Return: 0 on success, otherwise a negative error number
 */
int spec_dbg_init(struct spec_dev *spec)
{
	spec->dbg_dir = debugfs_create_dir(dev_name(&spec->dev), NULL);
	if (IS_ERR_OR_NULL(spec->dbg_dir)) {
		dev_err(&spec->dev,
			"Cannot create debugfs directory (%ld)\n",
			PTR_ERR(spec->dbg_dir));
		return PTR_ERR(spec->dbg_dir);
	}

	spec->dbg_info = debugfs_create_file(SPEC_DBG_INFO_NAME, 0444,
					     spec->dbg_dir, spec,
					     &spec_irq_dbg_info_ops);
	if (IS_ERR_OR_NULL(spec->dbg_info)) {
		dev_err(&spec->dev,
			"Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_INFO_NAME, PTR_ERR(spec->dbg_info));
		return PTR_ERR(spec->dbg_info);
	}

	spec->dbg_fw = debugfs_create_file(SPEC_DBG_FW_NAME, 0200,
					    spec->dbg_dir, spec,
					    &spec_dbg_fw_ops);
	if (IS_ERR_OR_NULL(spec->dbg_fw)) {
		dev_err(&spec->dev,
			"Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_FW_NAME, PTR_ERR(spec->dbg_fw));
		return PTR_ERR(spec->dbg_fw);
	}

	return 0;
}

/**
 * It removes the debugfs interface
 * @spec: SPEC device instance
 */
void spec_dbg_exit(struct spec_dev *spec)
{
	if (spec->dbg_dir)
		debugfs_remove_recursive(spec->dbg_dir);
}
