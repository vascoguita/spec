/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <linux/delay.h>

#include "spec.h"

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
 * it resets the FPGA
 */
static void gn4124_fpga_reset(struct spec_dev *spec)
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
 * configure Gennum GPIO to select GN4124->FPGA configuration mode
 * @spec: spec device instance
 */
static void gn4124_fpga_gpio_config(struct spec_dev *spec)
{
	gpio_out(spec, GNGPIO_DIRECTION_MODE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_DIRECTION_MODE, GPIO_BOOTSEL1, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL0, 1);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL1, 1);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL0, 1);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL1, 0);
}


/**
 * After programming, we fix gpio lines so pci can access the flash
 * @spec: spec device instance
 */
static void gn4124_fpga_gpio_restore(struct spec_dev *spec)
{
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_OUTPUT_VALUE, GPIO_BOOTSEL1, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL0, 0);
	gpio_out(spec, GNGPIO_OUTPUT_ENABLE, GPIO_BOOTSEL1, 0);
}


/**
 * Initialize the gennum
 * @spec: spec device instance
 * @last_word_size: last word size in the FPGA bitstream
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_fpga_fcl_init(struct spec_dev *spec, int last_word_size)
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
 * Wait for the FPGA to be configured and ready
 * @spec: device instance
 *
 * Return: 0 on success,-ETIMEDOUT on failure
 */
static int gn4124_fpga_fcl_waitdone(struct spec_dev *spec)
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
 * It configures the FPGA with the given image
 * @spec: spec instance
 * @data: FPGA configuration code
 * @len: image length in bytes
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int gn4124_fpga_load(struct spec_dev *spec, const void *data, int len)
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
 * It notifies the gennum that the configuration is over
 * @spec: spec device instance
 */
static void gn4124_fpga_fcl_complete(struct spec_dev *spec)
{
	gennum_writel(spec, 0x186, FCL_CTRL); /* "last data written" */
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

	gn4124_fpga_gpio_config(spec);
	err = gn4124_fpga_fcl_init(spec, info->count & 0x3);
	if (err < 0)
		goto err;

	return 0;

err:
	gn4124_fpga_gpio_restore(spec);
	return err;
}


static int spec_fpga_write(struct fpga_manager *mgr, const char *buf, size_t count)
{
	struct spec_dev *spec = mgr->priv;

	return gn4124_fpga_load(spec, buf, count);
}


static int spec_fpga_write_complete(struct fpga_manager *mgr,
				    struct fpga_image_info *info)
{
	struct spec_dev *spec = mgr->priv;
	int err;

	gn4124_fpga_fcl_complete(spec);

	err = gn4124_fpga_fcl_waitdone(spec);
	if (err < 0)
		return err;

	gn4124_fpga_gpio_restore(spec);
	gn4124_fpga_reset(spec);

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
	.groups = NULL,
};


int spec_fpga_init(struct spec_dev *spec)
{
	int err;

	if (!spec)
		return -EINVAL;

	spec->mgr = fpga_mgr_create(&spec->dev,
				    dev_name(&spec->dev),
				    &spec_fpga_ops, spec);
	if (!spec->mgr)
		return -EPERM;

	err = fpga_mgr_register(spec->mgr);
	if (err) {
		fpga_mgr_free(spec->mgr);
		return err;
	}

	return 0;
}

void spec_fpga_exit(struct spec_dev *spec)
{
	if (!spec || !spec->mgr)
		return;
	fpga_mgr_unregister(spec->mgr);
}
