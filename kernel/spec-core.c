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
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>


#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
#define PCI_VENDOR_ID_GENNUM    (0x1A39)
#define PCI_DEVICE_ID_GN4124    (0x0004)

#define SPEC_MINOR_MAX (64)
#define SPEC_FLAG_BITS (8)


static DECLARE_BITMAP(spec_minors, SPEC_MINOR_MAX);
static dev_t basedev;
static struct class *spec_class;


/**
 * struct spec_dev - SPEC instance
 * It describes a SPEC device instance.
 * @cdev Char device descriptor
 * @dev Linux device instance descriptor
 * @flags collection of bit flags
 * @remap ioremap of PCI bar 0, 2, 4
 */
struct spec_dev {
	struct cdev cdev;
	struct device dev;

	DECLARE_BITMAP(flags, SPEC_FLAG_BITS);
	void __iomem *remap[3];	/* ioremap of bar 0, 2, 4 */
};


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

	err = try_module_get(file->f_op->owner);
	if (err == 0)
		return -EBUSY;

	file->private_data = spec;

	return 0;
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
	return -EINVAL;
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
	dev_set_name(&spec->dev, "spec.%04x:%02x:%02x.%d",
		     pci_domain_nr(pdev->bus),
		     pdev->bus->number, PCI_SLOT(pdev->devfn),
		     PCI_FUNC(pdev->devfn));
	spec->dev.class = spec_class;
	spec->dev.devt = basedev + minor;
	spec->dev.parent = &pdev->dev;
	spec->dev.release = spec_release;
	dev_set_drvdata(&spec->dev, spec);

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

	return 0;

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
