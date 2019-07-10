// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <linux/platform_device.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/delay.h>
#include <linux/version.h>

#include "spec.h"
#include "spec-compat.h"
#include "gn412x.h"

/* Compatibility layer */
static int gn412x_fcl_write_init(struct fpga_manager *mgr,
				 struct fpga_image_info *info,
				 const char *buf, size_t count);
static int gn412x_fcl_write_complete(struct fpga_manager *mgr,
				     struct fpga_image_info *info);



int compat_get_fpga_last_word_size(struct fpga_image_info *info, size_t count)
{
#if KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
	return count;
#else
	return info ? info->count : count;
#endif
}

#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
static int compat_gn412x_fcl_write_init(struct fpga_manager *mgr,
				u32 flags,
				const char *buf, size_t count)
{
	return gn412x_fcl_write_init(mgr, NULL, buf, count);
}
static int compat_gn412x_fcl_write_complete(struct fpga_manager *mgr,
				    u32 flags)
{
	return gn412x_fcl_write_complete(mgr, NULL);
}
#else
static int compat_gn412x_fcl_write_init(struct fpga_manager *mgr,
					struct fpga_image_info *info,
					const char *buf, size_t count)
{
	return gn412x_fcl_write_init(mgr, info, buf, count);
}
static int compat_gn412x_fcl_write_complete(struct fpga_manager *mgr,
					    struct fpga_image_info *info)
{
	return gn412x_fcl_write_complete(mgr, info);
}
#endif

#if KERNEL_VERSION(4, 18, 0) > LINUX_VERSION_CODE && !defined(CONFIG_FPGA_MGR_BACKPORT)
struct fpga_manager *compat_fpga_mgr_create(struct device *dev,
					    const char *name,
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

/* The real code */

/**
 * struct gn412x_dev GN412X device descriptor
 * @compl: for IRQ testing
 * @int_cfg_gpio: INT_CFG used for GPIO interrupts
 */
struct gn412x_fcl_dev {
	void __iomem *mem;

	struct fpga_manager *mgr;
};

static uint32_t gn412x_ioread32(struct gn412x_fcl_dev *gn412x,
				int reg)
{
	return ioread32(gn412x->mem + reg);
}

static void gn412x_iowrite32(struct gn412x_fcl_dev *gn412x,
			     uint32_t val, int reg)
{
	return iowrite32(val, gn412x->mem + reg);
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


/**
 * it resets the FPGA
 */
static void gn4124_fpga_reset(struct gn412x_fcl_dev *gn412x)
{
	uint32_t reg;

	/* After reprogramming, reset the FPGA using the gennum register */
	reg = gn412x_ioread32(gn412x, GNPCI_SYS_CFG_SYSTEM);
	/*
	 * This _fucking_ register must be written with extreme care,
	 * becase some fields are "protected" and some are not. *hate*
	 */
	gn412x_iowrite32(gn412x, (reg & ~0xffff) | 0x3fff,
			 GNPCI_SYS_CFG_SYSTEM);
	gn412x_iowrite32(gn412x, (reg & ~0xffff) | 0x7fff,
			 GNPCI_SYS_CFG_SYSTEM);
}


/**
 * Initialize the gennum
 * @gn412x: gn412x device instance
 * @last_word_size: last word size in the FPGA bitstream
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_fpga_fcl_init(struct gn412x_fcl_dev *gn412x,
				int last_word_size)
{
	uint32_t ctrl;
	int i;

	gn412x_iowrite32(gn412x, 0x00, FCL_CLK_DIV);
	gn412x_iowrite32(gn412x, 0x40, FCL_CTRL); /* Reset */
	i = gn412x_ioread32(gn412x, FCL_CTRL);
	if (i != 0x40) {
		pr_err("%s: %i: error\n", __func__, __LINE__);
		return -EIO;
	}
	gn412x_iowrite32(gn412x, 0x00, FCL_CTRL);
	gn412x_iowrite32(gn412x, 0x00, FCL_IRQ); /* clear pending irq */

	switch (last_word_size) {
	case 3:
		ctrl = 0x116;
		break;
	case 2:
		ctrl = 0x126;
		break;
	case 1:
		ctrl = 0x136;
		break;
	case 0:
		ctrl = 0x106;
		break;
	default: return -EINVAL;
	}
	gn412x_iowrite32(gn412x, ctrl, FCL_CTRL);
	gn412x_iowrite32(gn412x, 0x00, FCL_CLK_DIV);
	gn412x_iowrite32(gn412x, 0x00, FCL_TIMER_CTRL);
	gn412x_iowrite32(gn412x, 0x10, FCL_TIMER_0);
	gn412x_iowrite32(gn412x, 0x00, FCL_TIMER_1);

	/*
	 * Set delay before data and clock is applied by FCL
	 * after SPRI_STATUS is	detected being assert.
	 */
	gn412x_iowrite32(gn412x, 0x08, FCL_TIMER2_0);
	gn412x_iowrite32(gn412x, 0x00, FCL_TIMER2_1);
	gn412x_iowrite32(gn412x, 0x17, FCL_EN);

	ctrl |= 0x01; /* "start FSM configuration" */
	gn412x_iowrite32(gn412x, ctrl, FCL_CTRL);

	return 0;
}


/**
 * Wait for the FPGA to be configured and ready
 * @gn412x: device instance
 *
 * Return: 0 on success,-ETIMEDOUT on failure
 */
static int gn4124_fpga_fcl_waitdone(struct gn412x_fcl_dev *gn412x)
{
	unsigned long j;

	j = jiffies + 2 * HZ;
	while (1) {
		uint32_t val = gn412x_ioread32(gn412x, FCL_IRQ);

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
 * It configures the FPGA with the given image
 * @gn412x: gn412x instance
 * @data: FPGA configuration code
 * @len: image length in bytes
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_fpga_load(struct gn412x_fcl_dev *gn412x,
			    const void *data, int len)
{
	int size32 = (len + 3) >> 2;
	int done = 0, wrote = 0;
	const uint32_t *data32 = data;

	while (size32 > 0) {
		/* Check to see if FPGA configuation has error */
		int i = gn412x_ioread32(gn412x, FCL_IRQ);

		if ((i & 8) && wrote) {
			done = 1;
			pr_err("%s: %i: done after %i%i\n",
			       __func__, __LINE__,
			       wrote, ((len + 3) >> 2));
		} else if ((i & 0x4) && !done) {
			pr_err("%s: %i: error after %i/%i\n",
			       __func__, __LINE__,
				wrote, ((len + 3) >> 2));
			return -EIO;
		}

		/* Wait until at least 1/2 of the fifo is empty */
		while (gn412x_ioread32(gn412x, FCL_IRQ)  & (1<<5))
			;

		/* Write a few dwords into FIFO at a time. */
		for (i = 0; size32 && i < 32; i++) {
			gn412x_iowrite32(gn412x, unaligned_bitswap_le32(data32),
				  FCL_FIFO);
			data32++; size32--; wrote++;
		}
	}

	return 0;
}


/**
 * It notifies the gennum that the configuration is over
 * @gn412x: gn412x device instance
 */
static void gn4124_fpga_fcl_complete(struct gn412x_fcl_dev *gn412x)
{
	gn412x_iowrite32(gn412x, 0x186, FCL_CTRL); /* "last data written" */
}


static enum fpga_mgr_states gn412x_fcl_state(struct fpga_manager *mgr)
{
	return mgr->state;
}


static int gn412x_fcl_write_init(struct fpga_manager *mgr,
				 struct fpga_image_info *info,
				 const char *buf, size_t count)
{
	struct gn412x_fcl_dev *gn412x = mgr->priv;
	int err = 0, last_word_size;

	last_word_size = compat_get_fpga_last_word_size(info, count) & 0x3;
	err = gn4124_fpga_fcl_init(gn412x, last_word_size);
	if (err < 0)
		goto err;

	return 0;

err:
	return err;
}

static int gn412x_fcl_write(struct fpga_manager *mgr,
			   const char *buf, size_t count)
{
	struct gn412x_fcl_dev *gn412x = mgr->priv;

	return gn4124_fpga_load(gn412x, buf, count);
}


static int gn412x_fcl_write_complete(struct fpga_manager *mgr,
				     struct fpga_image_info *info)
{
	struct gn412x_fcl_dev *gn412x = mgr->priv;
	int err;

	gn4124_fpga_fcl_complete(gn412x);

	err = gn4124_fpga_fcl_waitdone(gn412x);
	if (err < 0)
		return err;

	gn4124_fpga_reset(gn412x);

	return 0;
}


static void gn412x_fcl_fpga_remove(struct fpga_manager *mgr)
{
	/* do nothing */
}

static const struct fpga_manager_ops gn412x_fcl_ops = {
	compat_fpga_ops_initial_header_size
	compat_fpga_ops_groups

	.state = gn412x_fcl_state,
	.write_init = compat_gn412x_fcl_write_init,
	.write = gn412x_fcl_write,
	.write_complete = compat_gn412x_fcl_write_complete,
	.fpga_remove = gn412x_fcl_fpga_remove,
};


static int gn412x_fcl_probe(struct platform_device *pdev)
{
	struct gn412x_fcl_dev *gn412x;
	struct resource *r;
	int err;

	gn412x = devm_kzalloc(&pdev->dev, sizeof(*gn412x), GFP_KERNEL);
	if (!gn412x)
		return -ENOMEM;
	platform_set_drvdata(pdev, gn412x);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "Missing memory resource\n");
		err = -EINVAL;
		goto err_res_mem;
	}
	gn412x->mem = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!gn412x->mem) {
		err = -EADDRNOTAVAIL;
		goto err_map;
	}


	gn412x->mgr = compat_fpga_mgr_create(&pdev->dev,
					     dev_name(&pdev->dev),
					     &gn412x_fcl_ops, gn412x);
	if (!gn412x->mgr) {
		err = -EPERM;
		goto err_fpga_create;
	}

	err = compat_fpga_mgr_register(gn412x->mgr);
	if (err) {
		compat_fpga_mgr_free(gn412x->mgr);
		goto err_fpga_reg;
	}

	return 0;

err_fpga_reg:
	compat_fpga_mgr_free(gn412x->mgr);
err_fpga_create:
	devm_iounmap(&pdev->dev, gn412x->mem);
err_map:
err_res_mem:
	devm_kfree(&pdev->dev, gn412x);
	return err;
}

static int gn412x_fcl_remove(struct platform_device *pdev)
{
	struct gn412x_fcl_dev *gn412x = platform_get_drvdata(pdev);

	if (!gn412x->mgr)
		return -ENODEV;

	compat_fpga_mgr_unregister(gn412x->mgr);
	compat_fpga_mgr_free(gn412x->mgr);

	return 0;
}

static const struct platform_device_id gn412x_fcl_id[] = {
	{
		.name = "gn412x-fcl",
	},
	{ .name = "" }, /* last */
};

static struct platform_driver gn412x_fcl_platform_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
	},
	.probe = gn412x_fcl_probe,
	.remove = gn412x_fcl_remove,
	.id_table = gn412x_fcl_id,
};

module_platform_driver(gn412x_fcl_platform_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_DESCRIPTION("GN412X FCL driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DEVICE_TABLE(platform, gn412x_fcl_id);
