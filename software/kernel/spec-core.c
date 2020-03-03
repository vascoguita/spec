// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * Driver for SPEC (Simple PCI FMC carrier) board.
 */

#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/mfd/core.h>
#include <linux/gpio/consumer.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#if KERNEL_VERSION(3, 17, 0) <= LINUX_VERSION_CODE
#include <linux/gpio/machine.h>
#endif

#include "platform_data/gn412x-gpio.h"
#include "spec.h"
#include "spec-compat.h"

static char *spec_fw_name_45t = "spec-golden-45T.bin";
static char *spec_fw_name_100t = "spec-golden-100T.bin";
static char *spec_fw_name_150t = "spec-golden-150T.bin";

char *spec_fw_name = "";
module_param_named(fw_name, spec_fw_name, charp, 0444);

static int spec_fw_load(struct spec_gn412x *spec_gn412x, const char *name);

/* Debugging */
static int spec_irq_dbg_info(struct seq_file *s, void *offset)
{
	struct spec_gn412x *spec_gn412x = s->private;

	seq_printf(s, "'%s':\n", dev_name(&spec_gn412x->pdev->dev));

	seq_printf(s, "  redirect: %d\n",
		   to_pci_dev(&spec_gn412x->pdev->dev)->irq);
	seq_puts(s, "  irq-mapping:\n");
	seq_puts(s, "    - hardware: 8\n");
	seq_printf(s, "      linux: %d\n",
		   gpiod_to_irq(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]));
	seq_puts(s, "    - hardware: 9\n");
	seq_printf(s, "      linux: %d\n",
		   gpiod_to_irq(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]));

	return 0;
}

static int spec_irq_dbg_info_open(struct inode *inode, struct file *file)
{
	struct spec_gn412x *spec = inode->i_private;

	return single_open(file, spec_irq_dbg_info, spec);
}

static const struct file_operations spec_irq_dbg_info_ops = {
	.owner = THIS_MODULE,
	.open  = spec_irq_dbg_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define SPEC_DBG_FW_BUF_LEN 128
static ssize_t spec_dbg_fw_write(struct file *file,
				 const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct spec_gn412x *spec_gn412x = file->private_data;
	char buf_l[SPEC_DBG_FW_BUF_LEN];
	int err;

	if (SPEC_DBG_FW_BUF_LEN < count) {
		dev_err(&spec_gn412x->pdev->dev,
			 "Firmware name too long max %u\n",
			SPEC_DBG_FW_BUF_LEN);

		return -EINVAL;
	}
	memset(buf_l, 0, SPEC_DBG_FW_BUF_LEN);
	err = copy_from_user(buf_l, buf, count);
	if (err)
		return -EFAULT;

	err = spec_fw_load(spec_gn412x, buf);
	if (err)
		return err;
	return count;
}

static const struct file_operations spec_dbg_fw_ops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.write = spec_dbg_fw_write,
};

static void seq_printf_meta(struct seq_file *s, const char *indent,
			    struct spec_meta_id *meta)
{
	seq_printf(s, "%sMetadata:\n", indent);
	seq_printf(s, "%s  - Vendor: 0x%08x\n", indent, meta->vendor);
	seq_printf(s, "%s  - Device: 0x%08x\n", indent, meta->device);
	seq_printf(s, "%s  - Version: 0x%08x\n", indent, meta->version);
	seq_printf(s, "%s  - BOM: 0x%08x\n", indent, meta->bom);
	seq_printf(s, "%s  - SourceID: 0x%08x%08x%08x%08x\n",
		   indent,
		   meta->src[0],
		   meta->src[1],
		   meta->src[2],
		   meta->src[3]);
	seq_printf(s, "%s  - CapabilityMask: 0x%08x\n", indent, meta->cap);
	seq_printf(s, "%s  - VendorUUID: 0x%08x%08x%08x%08x\n",
		   indent,
		   meta->uuid[0],
		   meta->uuid[1],
		   meta->uuid[2],
		   meta->uuid[3]);
}

static int spec_dbg_meta(struct seq_file *s, void *offset)
{
	struct spec_gn412x *spec_gn412x = s->private;
	struct resource *r0 = &spec_gn412x->pdev->resource[0];
	void *iomem;
	uint32_t app_offset;

	iomem =  ioremap(r0->start, resource_size(r0));
	if (!iomem) {
		dev_warn(&spec_gn412x->pdev->dev, "%s: Mapping failed\n",
			 __func__);
		return -ENOMEM;
	}
	app_offset = ioread32(iomem + 0x40);
	seq_printf_meta(s, "", iomem + SPEC_META_BASE);
	seq_puts(s, "Application:\n");
	seq_printf_meta(s, "  ", iomem + app_offset);
	iounmap(iomem);

	return 0;
}

static int spec_dbg_meta_open(struct inode *inode, struct file *file)
{
	struct spec_gn412x *spec = inode->i_private;

	return single_open(file, spec_dbg_meta, spec);
}

static const struct file_operations spec_dbg_meta_ops = {
	.owner = THIS_MODULE,
	.open  = spec_dbg_meta_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * It initializes the debugfs interface
 * @spec: SPEC device instance
 *
 * Return: 0 on success, otherwise a negative error number
 */
static int spec_dbg_init(struct spec_gn412x *spec_gn412x)
{
	struct device *dev = &spec_gn412x->pdev->dev;

	spec_gn412x->dbg_dir = debugfs_create_dir(dev_name(dev),
						  NULL);
	if (IS_ERR_OR_NULL(spec_gn412x->dbg_dir)) {
		dev_err(dev, "Cannot create debugfs directory (%ld)\n",
			PTR_ERR(spec_gn412x->dbg_dir));
		return PTR_ERR(spec_gn412x->dbg_dir);
	}

	spec_gn412x->dbg_info = debugfs_create_file(SPEC_DBG_INFO_NAME, 0444,
						    spec_gn412x->dbg_dir,
						    spec_gn412x,
						    &spec_irq_dbg_info_ops);
	if (IS_ERR_OR_NULL(spec_gn412x->dbg_info)) {
		dev_err(dev, "Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_INFO_NAME, PTR_ERR(spec_gn412x->dbg_info));
		return PTR_ERR(spec_gn412x->dbg_info);
	}

	spec_gn412x->dbg_fw = debugfs_create_file(SPEC_DBG_FW_NAME, 0200,
						  spec_gn412x->dbg_dir,
						  spec_gn412x,
						  &spec_dbg_fw_ops);
	if (IS_ERR_OR_NULL(spec_gn412x->dbg_fw)) {
		dev_err(dev, "Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_FW_NAME, PTR_ERR(spec_gn412x->dbg_fw));
		return PTR_ERR(spec_gn412x->dbg_fw);
	}

	spec_gn412x->dbg_meta = debugfs_create_file(SPEC_DBG_META_NAME, 0200,
						    spec_gn412x->dbg_dir,
						    spec_gn412x,
						    &spec_dbg_meta_ops);
	if (IS_ERR_OR_NULL(spec_gn412x->dbg_meta)) {
		dev_err(dev, "Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_META_NAME, PTR_ERR(spec_gn412x->dbg_meta));
		return PTR_ERR(spec_gn412x->dbg_meta);
	}

	return 0;
}

/**
 * It removes the debugfs interface
 * @spec: SPEC device instance
 */
static void spec_dbg_exit(struct spec_gn412x *spec_gn412x)
{
	debugfs_remove_recursive(spec_gn412x->dbg_dir);
}


/* SPEC GPIO configuration */

static void spec_bootsel_set(struct spec_gn412x *spec_gn412x,
			     enum spec_fpga_select sel)
{
	switch (sel) {
	case SPEC_FPGA_SELECT_FPGA_FLASH:
	case SPEC_FPGA_SELECT_GN4124_FPGA:
	case SPEC_FPGA_SELECT_GN4124_FLASH:
		gpiod_set_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0],
				!!(sel & 0x1));
		gpiod_set_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1],
				!!(sel & 0x2));
		break;
	default:
		break;
	}
}

static enum spec_fpga_select spec_bootsel_get(struct spec_gn412x *spec_gn412x)
{
	enum spec_fpga_select sel = 0;

	sel |= !!gpiod_get_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1]) << 1;
	sel |= !!gpiod_get_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]) << 0;

	return sel;
}


static const struct gpiod_lookup_table spec_gpiod_table = {
	.table = {
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_BOOTSEL0,
				"bootsel", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_BOOTSEL1,
				"bootsel", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SPRI_DIN,
				"spi", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SPRI_FLASH_CS,
				"spi", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_IRQ0,
				"irq", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_IRQ1,
				"irq", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SCL,
				"i2c", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SDA,
				"i2c", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		{},
	}
};

static inline size_t spec_gpiod_table_size(void)
{
	return sizeof(struct gpiod_lookup_table) +
	       (sizeof(struct gpiod_lookup) * 9);
}

static int spec_gpio_init_table(struct spec_gn412x *spec_gn412x)
{
	struct gpiod_lookup_table *lookup;
	int err = 0;

	lookup = kmemdup(&spec_gpiod_table, spec_gpiod_table_size(),
			 GFP_KERNEL);
	if (!lookup)
		return -ENOMEM;

	lookup->dev_id = kstrdup(dev_name(&spec_gn412x->pdev->dev),
				 GFP_KERNEL);
	if (!lookup->dev_id)
		goto err_dup;

	spec_gn412x->gpiod_table = lookup;
	err = compat_gpiod_add_lookup_table(spec_gn412x->gpiod_table);
	if (err)
		goto err_lookup;

	return 0;

err_lookup:
	kfree(lookup->dev_id);
err_dup:
	kfree(lookup);

	return err;
}

static void spec_gpio_exit_table(struct spec_gn412x *spec_gn412x)
{
	struct gpiod_lookup_table *lookup = spec_gn412x->gpiod_table;

	gpiod_remove_lookup_table(lookup);
	kfree(lookup->dev_id);
	kfree(lookup);

	spec_gn412x->gpiod_table = NULL;
}

/**
 * Configure bootsel GPIOs
 *
 * Note: Because of a BUG in RedHat kernel 3.10 we re-set direction
 */
static int spec_gpio_init_bootsel(struct spec_gn412x *spec_gn412x)
{
	struct gpio_desc *gpiod;
	int err;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "bootsel", 0,
				GPIOD_OUT_HIGH);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel0;
	}
	err = gpiod_direction_output(gpiod, 1);
	if (err) {
		gpiod_put(gpiod);
		goto err_out0;
	}
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0] = gpiod;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "bootsel", 1,
				GPIOD_OUT_HIGH);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel1;
	}
	err = gpiod_direction_output(gpiod, 1);
	if (err) {
		gpiod_put(gpiod);
		goto err_out1;
	}
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1] = gpiod;

	return 0;

err_out1:
err_sel1:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]);
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0] = NULL;
err_out0:
err_sel0:
	return err;
}

static void spec_gpio_exit_bootsel(struct spec_gn412x *spec_gn412x)
{
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1]);
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1] = NULL;
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]);
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0] = NULL;
}


/**
 * Configure IRQ GPIOs
 *
 * Note: Because of a BUG in RedHat kernel 3.10 we re-set direction
 */
static int spec_gpio_init_irq(struct spec_gn412x *spec_gn412x)
{
	struct gpio_desc *gpiod;
	int err;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "irq", 0, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel0;
	}
	err = gpiod_direction_input(gpiod);
	if (err) {
		gpiod_put(gpiod);
		goto err_out0;
	}
	spec_gn412x->gpiod[GN4124_GPIO_IRQ0] = gpiod;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "irq", 1, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel1;
	}
	err = gpiod_direction_input(gpiod);
	if (err) {
		gpiod_put(gpiod);
		goto err_out1;
	}
	spec_gn412x->gpiod[GN4124_GPIO_IRQ1] = gpiod;

	return 0;

err_out1:
err_sel1:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]);
	spec_gn412x->gpiod[GN4124_GPIO_IRQ0] = NULL;
err_out0:
err_sel0:
	return err;
}

static void spec_gpio_exit_irq(struct spec_gn412x *spec_gn412x)
{
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]);
	spec_gn412x->gpiod[GN4124_GPIO_IRQ1] = NULL;
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]);
	spec_gn412x->gpiod[GN4124_GPIO_IRQ0] = NULL;
}

static int spec_gpio_init(struct spec_gn412x *spec_gn412x)
{
	int err;

	err = spec_gpio_init_table(spec_gn412x);
	if (err)
		return err;
	err = spec_gpio_init_bootsel(spec_gn412x);
	if (err)
		goto err_bootsel;
	err = spec_gpio_init_irq(spec_gn412x);
	if (err)
		goto err_irq;

	return 0;

err_irq:
	spec_gpio_exit_bootsel(spec_gn412x);
err_bootsel:
	spec_gpio_exit_table(spec_gn412x);

	return err;
}

static void spec_gpio_exit(struct spec_gn412x *spec_gn412x)
{
	spec_gpio_exit_irq(spec_gn412x);
	spec_gpio_exit_bootsel(spec_gn412x);
	spec_gpio_exit_table(spec_gn412x);
}

/* SPEC sub-devices */
static struct gn412x_platform_data gn412x_gpio_pdata = {
	.int_cfg = 0,
};

static struct resource gn412x_gpio_res[] = {
	{
		.name = "gn412x-gpio-mem",
		.flags = IORESOURCE_MEM,
		.start = 0,
		.end = 0x1000 - 1,
	}, {
		.name = "gn412x-gpio-irq",
		.flags = IORESOURCE_IRQ,
		.start = 0,
		.end = 0,
	}
};

static struct resource gn412x_fcl_res[] = {
	{
		.name = "gn412x-fcl-mem",
		.flags = IORESOURCE_MEM,
		.start = 0,
		.end = 0x1000 - 1,
	},
};

enum spec_mfd_enum {
	SPEC_MFD_GN412X_GPIO = 0,
	SPEC_MFD_GN412X_FCL,
};

static const struct mfd_cell spec_mfd_devs[] = {
	[SPEC_MFD_GN412X_GPIO] = {
		.name = "gn412x-gpio",
		.platform_data = &gn412x_gpio_pdata,
		.pdata_size = sizeof(gn412x_gpio_pdata),
		.num_resources = ARRAY_SIZE(gn412x_gpio_res),
		.resources = gn412x_gpio_res,
	},
	[SPEC_MFD_GN412X_FCL] = {
		.name = "gn412x-fcl",
		.platform_data = NULL,
		.pdata_size = 0,
		.num_resources = ARRAY_SIZE(gn412x_fcl_res),
		.resources = gn412x_fcl_res,
	},
};


/**
 * Return the SPEC defult FPGA firmware name based on PCI ID
 * @spec: SPEC device
 *
 * Return: FPGA firmware name
 */
static const char *spec_fw_name_init_get(struct spec_gn412x *spec_gn412x)
{
	if (strlen(spec_fw_name) > 0)
		return spec_fw_name;

	switch (spec_gn412x->pdev->device) {
	case PCI_DEVICE_ID_SPEC_45T:
		return spec_fw_name_45t;
	case PCI_DEVICE_ID_SPEC_100T:
		return spec_fw_name_100t;
	case PCI_DEVICE_ID_SPEC_150T:
		return spec_fw_name_150t;
	default:
		return NULL;
	}
}

/**
 * Load FPGA code
 * @spec: SPEC device
 * @name: FPGA bitstream file name
 *
 * Return: 0 on success, otherwise a negative error number
 */
static int spec_fw_load(struct spec_gn412x *spec_gn412x, const char *name)
{
	enum spec_fpga_select sel;
	int err;

	dev_dbg(&spec_gn412x->pdev->dev, "Writing firmware '%s'\n", name);
	err = spec_fpga_exit(spec_gn412x);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Cannot remove FPGA device instances. Try to remove them manually and to reload this device instance\n");
		return err;
	}


	mutex_lock(&spec_gn412x->mtx);
	sel = spec_bootsel_get(spec_gn412x);

	spec_bootsel_set(spec_gn412x, SPEC_FPGA_SELECT_GN4124_FPGA);

	err = compat_spec_fw_load(spec_gn412x, name);
	if (err)
		goto out;

	err = spec_fpga_init(spec_gn412x);
	if (err)
		dev_warn(&spec_gn412x->pdev->dev,
			 "FPGA incorrectly programmed %d\n", err);

out:
	spec_bootsel_set(spec_gn412x, sel);
	mutex_unlock(&spec_gn412x->mtx);

	return err;
}

static ssize_t bootselect_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	enum spec_fpga_select sel;

	if (strncmp("fpga-flash", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_FPGA_FLASH;
	} else if (strncmp("gn4124-fpga", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_GN4124_FPGA;
	} else if (strncmp("gn4124-flash", buf, 8) == 0) {
		sel = SPEC_FPGA_SELECT_GN4124_FLASH;
	} else {
		dev_err(dev, "Unknown bootselect option '%s'\n",
			buf);
		return -EINVAL;
	}

	mutex_lock(&spec_gn412x->mtx);
	spec_bootsel_set(spec_gn412x, sel);
	mutex_unlock(&spec_gn412x->mtx);

	return count;

}
static ssize_t bootselect_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	enum spec_fpga_select sel;

	sel = spec_bootsel_get(spec_gn412x);
	switch (sel) {
	case SPEC_FPGA_SELECT_FPGA_FLASH:
		return snprintf(buf, PAGE_SIZE, "fpga-flash\n");
	case SPEC_FPGA_SELECT_GN4124_FPGA:
		return snprintf(buf, PAGE_SIZE, "gn4124-fpga\n");
	case SPEC_FPGA_SELECT_GN4124_FLASH:
		return snprintf(buf, PAGE_SIZE, "gn4124-flash\n");
	default:
		dev_err(dev, "Unknown bootselect option '%x'\n",
			sel);
		return -EINVAL;
	}
}
static DEVICE_ATTR_RW(bootselect);

/**
 * Load golden bitstream on FGPA
 */
static ssize_t load_golden_fpga_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);
	int err;

	err = spec_fw_load(spec_gn412x, spec_fw_name_init_get(spec_gn412x));

	return err < 0 ? err : count;
}
static DEVICE_ATTR_WO(load_golden_fpga);

static struct attribute *gn412x_fpga_attrs[] = {
	&dev_attr_load_golden_fpga.attr,
	&dev_attr_bootselect.attr,
	NULL,
};

static const struct attribute_group gn412x_fpga_group = {
	.name = "fpga-options",
	.attrs = gn412x_fpga_attrs,
};

static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_gn412x *spec_gn412x;
	int err = 0;

	spec_gn412x = kzalloc(sizeof(*spec_gn412x), GFP_KERNEL);
	if (!spec_gn412x)
		return -ENOMEM;

	mutex_init(&spec_gn412x->mtx);
	pci_set_drvdata(pdev, spec_gn412x);
	spec_gn412x->pdev = pdev;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable PCI device (%d)\n",
			err);
		goto err_enable;
	}

	pci_set_master(pdev);
	err = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_AUTO,
			      spec_mfd_devs,
			      ARRAY_SIZE(spec_mfd_devs),
			      &pdev->resource[4], pdev->irq, NULL);
	if (err) {
		dev_err(&spec_gn412x->pdev->dev,
			"Failed to add MFD devices (%d)\n",
			err);
		goto err_mfd;
	}

	err = spec_gpio_init(spec_gn412x);
	if (err) {
		dev_err(&pdev->dev, "Failed to get GPIOs (%d)\n", err);
		goto err_sgpio;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &gn412x_fpga_group);
	if (err)
		goto err_sysfs;

	spec_dbg_init(spec_gn412x);

	mutex_lock(&spec_gn412x->mtx);
	err = spec_fpga_init(spec_gn412x);
	if (err)
		dev_warn(&pdev->dev,
			 "FPGA incorrectly programmed or empty (%d)\n", err);
	mutex_unlock(&spec_gn412x->mtx);

	return 0;

err_sysfs:
	spec_gpio_exit(spec_gn412x);
err_sgpio:
	mfd_remove_devices(&pdev->dev);
err_mfd:
	pci_disable_device(pdev);
err_enable:
	kfree(spec_gn412x);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_gn412x *spec_gn412x = pci_get_drvdata(pdev);

	spec_fpga_exit(spec_gn412x);
	spec_dbg_exit(spec_gn412x);
	sysfs_remove_group(&pdev->dev.kobj, &gn412x_fpga_group);
	spec_gpio_exit(spec_gn412x);

	mfd_remove_devices(&pdev->dev);
	pci_disable_device(pdev);
	kfree(spec_gn412x);
}


static const struct pci_device_id spec_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_45T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_100T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_150T)},
	{0,},
};


static struct pci_driver spec_driver = {
	.driver = {
		.owner = THIS_MODULE,
	},
	.name = "spec-fmc-carrier",
	.probe = spec_probe,
	.remove = spec_remove,
	.id_table = spec_pci_tbl,
};

module_pci_driver(spec_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Driver for the 'Simple PCIe FMC Carrier' a.k.a. SPEC");
MODULE_DEVICE_TABLE(pci, spec_pci_tbl);

MODULE_SOFTDEP("pre: gn412x_gpio gn412x_fcl htvic spec_gn412x_dma i2c_mux i2c-ocores spi-ocores");

ADDITIONAL_VERSIONS;
