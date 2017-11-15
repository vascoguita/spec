/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * Released according to the GNU GPL, version 3
 *
 * Driver for SPEC (Simple PCI FMC carrier) board.
 */

#include <linux/bitmap.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include "spec.h"
#include "loader-ll.h"


static DECLARE_BITMAP(spec_minors, SPEC_MINOR_MAX);
static dev_t basedev;
static struct class *spec_class;


/**
 * It reads a 32bit register from the gennum chip
 * @spec spec device instance
 * @reg gennum register offset
 * Return: a 32bit value
 */
static inline uint32_t gennum_readl(struct spec_dev *spec, int reg)
{
	return readl(spec->remap[2] + reg);
}


/**
 * It writes a 32bit register to the gennum chip
 * @spec spec device instance
 * @val a 32bit valure
 * @reg gennum register offset
 */
static inline void gennum_writel(struct spec_dev *spec, uint32_t val, int reg)
{
	writel(val, spec->remap[2] + reg);
}


/**
 * It writes a 32bit register to the gennum chip according to the given mask
 * @spec spec device instance
 * @mask bit mask of the bits to actually write
 * @val a 32bit valure
 * @reg gennum register offset
 */
static inline void gennum_mask_val(struct spec_dev *spec,
				   uint32_t mask, uint32_t val, int reg)
{
	uint32_t v = gennum_readl(spec, reg);
	v &= ~mask;
	v |= val;
	gennum_writel(spec, v, reg);
}


/**
 * It configures the gennum chip
 * @spec spec device instance
 * Return: 0 on success, otherwise a negative errno number
 */
static int gennum_config(struct spec_dev *spec)
{
	/* Put our 6 pins to a sane state (4 test points, 2 from FPGA) */
	gennum_mask_val(spec, 0xfc0, 0x000, GNGPIO_BYPASS_MODE); /* no AF */
	gennum_mask_val(spec, 0xfc0, 0xfc0, GNGPIO_DIRECTION_MODE); /* input */
	gennum_writel(spec, 0xffff, GNGPIO_INT_MASK_SET); /* disable */

	return 0;
}



/**
 * It gets a minor number
 * Return: the first minor number available
 */
static inline int spec_minor_get(void)
{
	int minor;

	minor = find_first_zero_bit(spec_minors, SPEC_MINOR_MAX);
	set_bit(minor, spec_minors);

	return minor;
}


/**
 * It releases a minor number
 * @minor minor number to release
 */
static inline void spec_minor_put(unsigned int minor)
{
	clear_bit(minor, spec_minors);
}


/* Load the FPGA. This bases on loader-ll.c, a kernel/user space thing */
static int spec_load_fpga(struct spec_dev *spec, const void *data, int size)
{
	int i, wrote;
	unsigned long j;

	/* loader_low_level is designed to run from user space too */
	wrote = loader_low_level(0 /* unused fd */,
				 spec->remap[2], data, size);
	j = jiffies + 2 * HZ;
	/* Wait for DONE interrupt  */
	while (1) {
		udelay(100);
		i = readl(spec->remap[2] + FCL_IRQ);
		if (i & 0x8)
			break;

		if (i & 0x4) {
			dev_err(&spec->dev,
				"FPGA program error after %i writes\n",
				wrote);
			return -ETIMEDOUT;
		}

		if (time_after(jiffies, j)) {
			dev_err(&spec->dev,
				"FPGA timeout after %i writes\n", wrote);
			return -ETIMEDOUT;
		}
	}
	gpiofix_low_level(0 /* unused fd */, spec->remap[2]);
	loader_reset_fpga(0 /* unused fd */, spec->remap[2]);

	return 0;
}


/**
 * It prepares the FPGA to receive a new bitstream.
 * @inode file system node
 * @file char device file open instance
 *
 * By just opening this device you may reset the FPGA
 * (unless other errors prevent the user from programming).
 * Only one user at time can access the programming procedure.
 * Return: 0 on success, otherwise a negative errno number
 */
static int spec_open(struct inode *inode, struct file *file)
{
	struct spec_dev *spec = container_of(inode->i_cdev,
					     struct spec_dev,
					     cdev);
	int err;

	if (!test_bit(SPEC_FLAG_UNLOCK, spec->flags)) {
		dev_info(&spec->dev, "Application FPGA programming blocked\n");
		return -EPERM;
	}

	err = try_module_get(file->f_op->owner);
	if (err == 0)
		return -EBUSY;

	spec->size = 1024 * 1024 * 4; /* 4MiB to begin with */
	spec->buf = vmalloc(spec->size);
	if (!spec->buf) {
		err = -ENOMEM;
		goto err_vmalloc;
	}


	file->private_data = spec;

	return 0;

err_vmalloc:
	module_put(file->f_op->owner);
	return err;
}


/**
 * @inode file system node
 * @file char device file open instance
 * Return 0 on success, otherwise a negative errno number.
 */
static int spec_close(struct inode *inode, struct file *file)
{
	struct spec_dev *spec = file->private_data;

	file->private_data = NULL;

	spec_load_fpga(spec, spec->buf, spec->size);

	vfree(spec->buf);
	spec->size = 0;

	spin_lock(&spec->lock);
	clear_bit(SPEC_FLAG_UNLOCK, spec->flags);
	spin_unlock(&spec->lock);

	module_put(file->f_op->owner);

	return 0;
}


/**
 * It creates a local copy of the user buffer and it start
 * to program the FPGA with it
 * @file char device file open instance
 * @buf user space buffer
 * @count user space buffer size
 * @offp offset where to copy the buffer (ignored here)
 * Return: number of byte actually copied, otherwise a negative errno
 */
static ssize_t spec_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *offp)
{
	struct spec_dev *spec = file->private_data;
	int err;

	if (unlikely(!spec->buf)) {
		dev_err(&spec->dev, "No memory left to copy the bitstream\n");
		return -ENOMEM;
	}

	if (unlikely(count + *offp > 1024 * 1024 * 20)) {
		dev_err(&spec->dev, "An FPGA bitstream of 20MiB is too big\n");
		return -EINVAL;
	}

	if (count + *offp > spec->size) {
		vfree(spec->buf);
		spec->size *= 2;
		spec->buf = vmalloc(spec->size);
		return 0;
	}

	err = copy_from_user(spec->buf + *offp, buf, count);
	if (err)
		return err;

	*offp += count;

	return count;
}


/**
 * Char device operation to provide bitstream
 */
static const struct file_operations spec_fops = {
	.owner = THIS_MODULE,
	.open = spec_open,
	.release = spec_close,
	.write  = spec_write,
};



/**
 * It shows the current AFPGA programming locking status
 */
static ssize_t spec_afpga_lock_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct spec_dev *spec = to_spec_dev(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			test_bit(SPEC_FLAG_UNLOCK, spec->flags) ?
			"unlocked" : "locked");
}


/**
 * It unlocks the AFPGA programming when the user write "unlock" or "lock"
 */
static ssize_t spec_afpga_lock_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct spec_dev *spec = to_spec_dev(dev);
	unsigned int lock;

	if (strncmp(buf, "unlock" , min(6, (int)count)) != 0 &&
	    strncmp(buf, "lock" , min(4, (int)count)) != 0)
		return -EINVAL;

	lock = (strncmp(buf, "lock" , min(4, (int)count)) == 0);

	spin_lock(&spec->lock);
	if (lock)
		clear_bit(SPEC_FLAG_UNLOCK, spec->flags);
	else
		set_bit(SPEC_FLAG_UNLOCK, spec->flags);
	spin_unlock(&spec->lock);

	return count;
}
static DEVICE_ATTR(lock, 0644, spec_afpga_lock_show, spec_afpga_lock_store);


static struct attribute *spec_dev_attrs[] = {
	&dev_attr_lock.attr,
	NULL,
};
static const struct attribute_group spec_dev_group = {
	.name = "AFPGA",
	.attrs = spec_dev_attrs,
};

static const struct attribute_group *spec_dev_groups[] = {
	&spec_dev_group,
	NULL,
};


/**
 * It releases device resources (`device->release()`)
 * @dev Linux device instance
 */
static void spec_release(struct device *dev)
{
	struct spec_dev *spec = dev_get_drvdata(dev);
	struct pci_dev *pdev = to_pci_dev(dev->parent);
	int minor = MINOR(dev->devt), i;

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);

	cdev_del(&spec->cdev);
	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
	kfree(spec);
	spec_minor_put(minor);
}


static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_dev *spec;
	int err, minor, i;

	minor = spec_minor_get();
	if (minor >= SPEC_MINOR_MAX)
		return -EINVAL;


	spec = kzalloc(sizeof(struct spec_dev), GFP_KERNEL);
	if (!spec) {
		err = -ENOMEM;
		goto err_alloc;
	}
	dev_set_name(&spec->dev, "spec.%04x",
		     pdev->bus->number << 8 | PCI_SLOT(pdev->devfn));
	spec->dev.class = spec_class;
	spec->dev.devt = basedev + minor;
	spec->dev.parent = &pdev->dev;
	spec->dev.release = spec_release;
	spec->dev.groups = spec_dev_groups;
	dev_set_drvdata(&spec->dev, spec);
	spin_lock_init(&spec->lock);

	/* Remap our 3 bars */
	for (i = err = 0; i < 3; i++) {
		struct resource *r = pdev->resource + (2 * i);

		if (!r->start)
			continue;
		if (r->flags & IORESOURCE_MEM) {
			spec->remap[i] = ioremap(r->start,
						r->end + 1 - r->start);
			if (!spec->remap[i])
				err = -ENOMEM;
		}
	}
	if (err)
		goto err_remap;


	cdev_init(&spec->cdev, &spec_fops);
	spec->cdev.owner = THIS_MODULE;
	err = cdev_add(&spec->cdev, spec->dev.devt, 1);
	if (err)
		goto err_cdev;

	err = device_register(&spec->dev);
	if (err)
		goto err_dev_reg;

	/* Enable the PCI device */
	pci_set_drvdata(pdev, spec);
	err = pci_enable_device(pdev);
	if (err)
		goto err_enable;
	pci_set_master(pdev);

	err = gennum_config(spec);
	if (err)
		goto err_gennum;

	return 0;

err_gennum:
	pci_disable_device(pdev);
err_enable:
	pci_set_drvdata(pdev, NULL);
	device_unregister(&spec->dev);
err_dev_reg:
	cdev_del(&spec->cdev);
err_cdev:
	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
err_remap:
	kfree(spec);
err_alloc:
	spec_minor_put(minor);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_dev *spec = pci_get_drvdata(pdev);

	device_unregister(&spec->dev);
}


static DEFINE_PCI_DEVICE_TABLE(spec_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_45T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_100T)},
	{PCI_DEVICE(PCI_VENDOR_ID_GENNUM, PCI_DEVICE_ID_GN4124)},
	{0,},
};


static struct pci_driver spec_driver = {
	.name = "spec",
	.probe = spec_probe,
	.remove = spec_remove,
	.id_table = spec_pci_tbl,
};


static int __init spec_init(void)
{
	int err = 0;

	spec_class = class_create(THIS_MODULE, "spec");
	if (IS_ERR_OR_NULL(spec_class)) {
		err = PTR_ERR(spec_class);
		goto err_cls;
	}
	/* Allocate a char device region for devices, CPUs and slots */
	err = alloc_chrdev_region(&basedev, 0, SPEC_MINOR_MAX, "spec");
	if (err)
		goto err_chrdev_alloc;

	err = pci_register_driver(&spec_driver);
	if (err)
		goto err_reg;

	return 0;

err_reg:
	unregister_chrdev_region(basedev, SPEC_MINOR_MAX);
err_chrdev_alloc:
	class_destroy(spec_class);
err_cls:
	return err;
}


static void __exit spec_exit(void)
{
	pci_unregister_driver(&spec_driver);
	unregister_chrdev_region(basedev, SPEC_MINOR_MAX);
	class_destroy(spec_class);
}


module_init(spec_init);
module_exit(spec_exit);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(GIT_VERSION);
MODULE_DESCRIPTION("spec driver");

ADDITIONAL_VERSIONS;
