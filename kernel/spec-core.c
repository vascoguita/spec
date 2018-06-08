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
#include <linux/fpga/fpga-mgr.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include "spec.h"

/* These must be set to choose the FPGA configuration mode */
#define GPIO_BOOTSEL0 15
#define GPIO_BOOTSEL1 14

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

static inline uint8_t reverse_bits8(uint8_t x)
{
	x = ((x >> 1) & 0x55) | ((x & 0x55) << 1);
	x = ((x >> 2) & 0x33) | ((x & 0x33) << 2);
	x = ((x >> 4) & 0x0f) | ((x & 0x0f) << 4);

	return x;
}

static uint32_t unaligned_bitswap_le32(const uint32_t *ptr32)
{
	static uint32_t tmp32;
	static uint8_t *tmp8 = (uint8_t *) &tmp32;
	static uint8_t *ptr8;

	ptr8 = (uint8_t *) ptr32;

	*(tmp8 + 0) = reverse_bits8(*(ptr8 + 0));
	*(tmp8 + 1) = reverse_bits8(*(ptr8 + 1));
	*(tmp8 + 2) = reverse_bits8(*(ptr8 + 2));
	*(tmp8 + 3) = reverse_bits8(*(ptr8 + 3));

	return tmp32;
}

static inline void gpio_out(struct spec_dev *spec, const uint32_t addr,
			    const int bit, const int value)
{
	uint32_t reg;

	reg = gennum_readl(spec, addr);

	if(value)
		reg |= (1<<bit);
	else
		reg &= ~(1<<bit);

	gennum_writel(spec, reg, addr);
}


/**
 * configure Gennum GPIO to select GN4124->FPGA configuration mode
 * @spec: spec device instance
 */
static void gn4124_gpio_config(struct spec_dev *spec)
{
	gpio_out(spec, GNGPIO_DIRECTION_MODE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_DIRECTION_MODE, GPIO_BOOTSEL1, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL0, 1);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL1, 1);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL0, 1);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL1, 0);
}


/**
 * Initialize the gennum
 * @spec: spec device instance
 * @last_word_size: last word size in the FPGA bitstream
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_fcl_init(struct spec_dev *spec, int last_word_size)
{
	uint32_t ctrl;
	int i;

	gennum_writel(spec, 0x00, FCL_CLK_DIV);
	gennum_writel(spec, 0x40, FCL_CTRL); /* Reset */
	i = gennum_readl(spec, FCL_CTRL);
	if (i != 0x40) {
		printk(KERN_ERR "%s: %i: error\n", __func__, __LINE__);
		return -EIO;
	}
	gennum_writel(spec, 0x00, FCL_CTRL);
	gennum_writel(spec, 0x00, FCL_IRQ); /* clear pending irq */

	switch(last_word_size) {
	case 3: ctrl = 0x116; break;
	case 2: ctrl = 0x126; break;
	case 1: ctrl = 0x136; break;
	case 0: ctrl = 0x106; break;
	default: return -EINVAL;
	}
	gennum_writel(spec, ctrl, FCL_CTRL);
	gennum_writel(spec, 0x00, FCL_CLK_DIV); /* again? maybe 1 or 2? */
	gennum_writel(spec, 0x00, FCL_TIMER_CTRL); /* "disable FCL timr fun" */
	gennum_writel(spec, 0x10, FCL_TIMER_0); /* "pulse width" */
	gennum_writel(spec, 0x00, FCL_TIMER_1);

	/*
	 * Set delay before data and clock is applied by FCL
	 * after SPRI_STATUS is	detected being assert.
	 */
	gennum_writel(spec, 0x08, FCL_TIMER2_0); /* "delay before data/clk" */
	gennum_writel(spec, 0x00, FCL_TIMER2_1);
	gennum_writel(spec, 0x17, FCL_EN); /* "output enable" */

	ctrl |= 0x01; /* "start FSM configuration" */
	gennum_writel(spec, ctrl, FCL_CTRL);

	return 0;
}


/**
 * It notifies the gennum that the configuration is over
 * @spec: spec device instance
 */
static void gn4124_fcl_complete(struct spec_dev *spec)
{
	gennum_writel(spec, 0x186, FCL_CTRL); /* "last data written" */
}


/**
 * It configures the FPGA with the given image
 * @spec: spec instance
 * @data: FPGA configuration code
 * @len: image length in bytes
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_load(struct spec_dev *spec, const void *data, int len)
{
	int size32 = (len + 3) >> 2;
	int done = 0, wrote = 0, i;
	const uint32_t *data32 = data;

	while(size32 > 0)
	{
		/* Check to see if FPGA configuation has error */
		i = gennum_readl(spec, FCL_IRQ);
		if ( (i & 8) && wrote) {
			done = 1;
			printk("%s: %i: done after %i\n", __func__, __LINE__,
				wrote);
		} else if ( (i & 0x4) && !done) {
			printk("%s: %i: error after %i\n", __func__, __LINE__,
				wrote);
			return -EIO;
		}

		/* Wait until at least 1/2 of the fifo is empty */
		while (gennum_readl(spec, FCL_IRQ)  & (1<<5))
			;

		/* Write a few dwords into FIFO at a time. */
		for (i = 0; size32 && i < 32; i++) {
			gennum_writel(spec, unaligned_bitswap_le32(data32),
				  FCL_FIFO);
			data32++; size32--; wrote++;
		}
	}

	return 0;
}

/**
 * After programming, we fix gpio lines so pci can access the flash
 */
static void gn4124_gpio_restore(struct spec_dev *spec)
{
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL1, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL1, 0);
}


/**
 * it resets the FPGA
 */
static void gn4124_reset_fpga(struct spec_dev *spec)
{
	uint32_t reg;

	/* After reprogramming, reset the FPGA using the gennum register */
	reg = gennum_readl(spec, GNPCI_SYS_CFG_SYSTEM);
	/*
	 * This _fucking_ register must be written with extreme care,
	 * becase some fields are "protected" and some are not. *hate*
	 */
	gennum_writel(spec, (reg & ~0xffff) | 0x3fff, GNPCI_SYS_CFG_SYSTEM);
	gennum_writel(spec, (reg & ~0xffff) | 0x7fff, GNPCI_SYS_CFG_SYSTEM);
}


/**
 * Wait for the FPGA to be configured and ready
 * @spec: device instance
 *
 * Return: 0 on success,-ETIMEDOUT on failure
 */
static int gn4124_fcl_waitdone(struct spec_dev *spec)
{
	unsigned long j;
	uint32_t val;

	j = jiffies + 2 * HZ;
	while (1) {
		val = gennum_readl(spec, FCL_IRQ);

		/* Done */
		if (val & 0x8)
			return 0;

		/* Fail */
		if (val & 0x4)
			return -EIO;

		/* Timeout */
		if (time_after(jiffies, j))
			return -ETIMEDOUT;

		udelay(100);
	}
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


static enum fpga_mgr_states spec_fpga_state(struct fpga_manager *mgr)
{
	return mgr->state;
}


static int spec_fpga_write_init(struct fpga_manager *mgr,
				struct fpga_image_info *info,
				const char *buf, size_t count)
{
	struct spec_dev *spec = mgr->priv;
	int err = 0;

	gn4124_gpio_config(spec);
	err = gn4124_fcl_init(spec, info->count & 0x3);
	if (err < 0)
		goto err;

	return 0;

err:
	gn4124_gpio_restore(spec);
	return err;
}


static int spec_fpga_write(struct fpga_manager *mgr, const char *buf, size_t count)
{
	struct spec_dev *spec = mgr->priv;

	return gn4124_load(spec, buf, count);
}


static int spec_fpga_write_complete(struct fpga_manager *mgr,
				    struct fpga_image_info *info)
{
	struct spec_dev *spec = mgr->priv;
	int err;

	gn4124_fcl_complete(spec);

	err = gn4124_fcl_waitdone(spec);
	if (err < 0)
		return err;

	gn4124_gpio_restore(spec);
	gn4124_reset_fpga(spec);

	return 0;
}


static void spec_fpga_remove(struct fpga_manager *mgr)
{
	/* do nothing */
}


static const struct fpga_manager_ops spec_fpga_ops = {
	.initial_header_size = 0,
	.state = spec_fpga_state,
	.write_init = spec_fpga_write_init,
	.write = spec_fpga_write,
	.write_complete = spec_fpga_write_complete,
	.fpga_remove = spec_fpga_remove,
};


/**
 * It releases device resources (`device->release()`)
 * @dev Linux device instance
 */
static void spec_release(struct device *dev)
{
	struct spec_dev *spec = dev_get_drvdata(dev);
	struct pci_dev *pdev = to_pci_dev(dev->parent);
	int i;

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);

	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
	kfree(spec);
}


static int spec_probe(struct pci_dev *pdev,
		      const struct pci_device_id *id)
{
	struct spec_dev *spec;
	int err, i;

	spec = kzalloc(sizeof(struct spec_dev), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	dev_set_name(&spec->dev, "spec.%04x",
		     pdev->bus->number << 8 | PCI_SLOT(pdev->devfn));
	spec->dev.parent = &pdev->dev;
	spec->dev.release = spec_release;
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

	err = device_register(&spec->dev);
	if (err)
		goto err_dev_reg;

	err =fpga_mgr_register(&spec->dev, dev_name(&spec->dev),
			       &spec_fpga_ops, spec);
	if (err)
		goto err_fpga;

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
	fpga_mgr_unregister(&spec->dev);
err_fpga:
	pci_set_drvdata(pdev, NULL);
	device_unregister(&spec->dev);
err_dev_reg:
	for (i = 0; i < 3; i++) {
		if (spec->remap[i])
			iounmap(spec->remap[i]);
		spec->remap[i] = NULL;
	}
err_remap:
	kfree(spec);
	return err;
}


static void spec_remove(struct pci_dev *pdev)
{
	struct spec_dev *spec = pci_get_drvdata(pdev);

	fpga_mgr_unregister(&spec->dev);
	device_unregister(&spec->dev);
}


static DEFINE_PCI_DEVICE_TABLE(spec_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_45T)},
	{PCI_DEVICE(PCI_VENDOR_ID_CERN, PCI_DEVICE_ID_SPEC_100T)},
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
	return pci_register_driver(&spec_driver);
}


static void __exit spec_exit(void)
{
	pci_unregister_driver(&spec_driver);
}


module_init(spec_init);
module_exit(spec_exit);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(GIT_VERSION);
MODULE_DESCRIPTION("spec driver");

ADDITIONAL_VERSIONS;
