// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include "platform_data/spi_gcores.h"

#define SPI_GCORES_BUF_SIZE 4
/* SPI register */
#define SPI_GCORES_RX(x) (x * SPI_GCORES_BUF_SIZE)
#define SPI_GCORES_TX(x) (x * SPI_GCORES_BUF_SIZE)
#define SPI_GCORES_CTRL 0x10
#define SPI_GCORES_DIV 0x14
#define SPI_GCORES_CS 0x18

/* SPI control register fields mask */
#define SPI_GCORES_CTRL_CHAR_LEN 0x007F
#define SPI_GCORES_CTRL_GO 0x0100 /* go/busy */
#define SPI_GCORES_CTRL_BUSY 0x0100 /* go/busy */
#define SPI_GCORES_CTRL_Rx_NEG 0x0200
#define SPI_GCORES_CTRL_Tx_NEG 0x0400
#define SPI_GCORES_CTRL_LSB 0x0800
#define SPI_GCORES_CTRL_IE 0x1000
#define SPI_GCORES_CTRL_ASS 0x2000

#define SPI_GCORES_FLAG_POLL BIT(0)

struct spi_gcores {
	struct spi_master *master;
	unsigned long flags;
	void __iomem *mem;
	unsigned int clock_hz;
	uint32_t ctrl_base;

	/* Current transfer */
	struct spi_transfer *cur_xfer;
	uint32_t cur_ctrl;
	uint32_t cur_divider;
	const void *cur_tx_buf;
	unsigned int cur_tx_len;
	void *cur_rx_buf;
	unsigned int cur_rx_len;

	/* Register Access functions */
	uint32_t (*read)(struct spi_gcores *sp, unsigned int reg);
	void (*write)(struct spi_gcores *sp, unsigned int reg, uint32_t val);
};

enum spi_gcores_type {
	TYPE_GCORES = 0,
};

static inline uint32_t spi_gcores_ioread32(struct spi_gcores *sp,
					  unsigned int reg)
{
	return ioread32(sp->mem + reg);
}

static inline void spi_gcores_iowrite32(struct spi_gcores *sp,
				       unsigned int reg, uint32_t val)
{
	iowrite32(val, sp->mem + reg);
}

static inline uint32_t spi_gcores_ioread32be(struct spi_gcores *sp,
					     unsigned int reg)
{
	return ioread32be(sp->mem + reg);
}

static inline void spi_gcores_iowrite32be(struct spi_gcores *sp,
					  unsigned int reg, uint32_t val)
{
	iowrite32be(val, sp->mem + reg);
}

static inline struct spi_gcores *spi_gcoresdev_to_sp(struct spi_device *spi)
{
	return spi_master_get_devdata(spi->master);
}

/**
 * Configure controller according to SPI device needs
 */
static int spi_gcores_setup(struct spi_device *spi)
{
	return 0;
}

/**
 * Release resources used by the SPI device
 */
static void spi_gcores_cleanup(struct spi_device *spi)
{
}

/**
 * Configure controller
 */
static void spi_gcores_hw_xfer_config(struct spi_gcores *sp,
				      uint32_t ctrl,
				      uint16_t divider)
{
	ctrl &= ~SPI_GCORES_CTRL_GO; /* be sure to not start */
	sp->write(sp, ctrl, SPI_GCORES_CTRL);
	sp->write(sp, divider, SPI_GCORES_DIV);
}

/**
 * Transmit data in FIFO
 */
static void spi_gcores_hw_xfer_go(struct spi_gcores *sp)
{
	uint32_t ctrl;

	ctrl = sp->read(sp, SPI_GCORES_CTRL);
	ctrl |= SPI_GCORES_CTRL_GO;
	sp->write(sp, ctrl, SPI_GCORES_CTRL);
}

/**
 * Slave Select
 */
static void spi_gcores_hw_xfer_cs(struct spi_gcores *sp,
				  unsigned int cs,
				  unsigned int val)
{
	uint32_t ss;

	ss = sp->read(sp, SPI_GCORES_CS);
	if (val)
		ss |= BIT(cs);
	else
		ss &= ~BIT(cs);
	sp->write(sp, ss, SPI_GCORES_CS);
}

/**
 * Write a TX register
 */
static void spi_gcores_tx_set(struct spi_gcores *sp,
			      unsigned int idx, uint32_t val)
{
	if (WARN(idx > 3, "Invalid TX register index %d (min:0, max: 3). Possible data corruption\n",
		 idx))
		return;

	sp->write(sp, val, SPI_GCORES_TX(idx));
}

/**
 * Read an RX register
 */
static uint32_t spi_gcores_rx_get(struct spi_gcores *sp, unsigned int idx)
{
	if (WARN(idx > 3, "Invalid RX register index %d (min:0, max: 3). Possible data corruption\n",
		 idx))
		return 0;

	return sp->read(sp, SPI_GCORES_RX(idx));
}

/**
 * Get current bits per word
 * @sp: SPI OCORE controller
 *
 * Return: number of bits_per_word
 */
static uint8_t spi_gcores_hw_xfer_bits_per_word(struct spi_gcores *sp)
{
	uint8_t nbits;

	nbits = (sp->cur_xfer && sp->cur_xfer->bits_per_word) ?
		sp->cur_xfer->bits_per_word:
		sp->master->cur_msg->spi->bits_per_word;

	return nbits;
}

static void spi_gcores_hw_xfer_tx_push8(struct spi_gcores *sp)
{
	uint32_t data;

	data = *((uint8_t *)sp->cur_tx_buf);
	sp->cur_tx_buf += 1;
	spi_gcores_tx_set(sp, 0, data);
	sp->cur_tx_len -= 1;
}

static void spi_gcores_hw_xfer_tx_push16(struct spi_gcores *sp)
{
	uint32_t data;

	data = *((uint16_t *)sp->cur_tx_buf);
	sp->cur_tx_buf += 2;
	spi_gcores_tx_set(sp, 0, data);
	sp->cur_tx_len -= 2;
}

static void spi_gcores_hw_xfer_tx_push32(struct spi_gcores *sp)
{
	uint32_t data;

	data = *((uint32_t *)sp->cur_tx_buf);
	sp->cur_tx_buf += 4;
	spi_gcores_tx_set(sp, 0, data);
	sp->cur_tx_len -= 4;
}

static void spi_gcores_hw_xfer_tx_push64(struct spi_gcores *sp)
{
	int i;

	for (i = 0; i < 2; ++i) {
		uint32_t data;

		data = *((uint32_t *)sp->cur_tx_buf);
		sp->cur_tx_buf += 4;
		spi_gcores_tx_set(sp, i, data);
		sp->cur_tx_len -= 4;
	}
}

static void spi_gcores_hw_xfer_tx_push128(struct spi_gcores *sp)
{
	int i;

	for (i = 0; i < 4; ++i) {
		uint32_t data;

		data = *((uint32_t *)sp->cur_tx_buf);
		sp->cur_tx_buf += 4;
		spi_gcores_tx_set(sp, i, data);
		sp->cur_tx_len -= 4;
	}
}

static void spi_gcores_hw_xfer_rx_push8(struct spi_gcores *sp)
{
	uint32_t data;

	data = spi_gcores_rx_get(sp, 0) & 0x000000FF;
	*((uint8_t *)sp->cur_rx_buf) = data;
	sp->cur_rx_buf += 1;
	sp->cur_rx_len -= 1;
}

static void spi_gcores_hw_xfer_rx_push16(struct spi_gcores *sp)
{
	uint32_t data;

	data = spi_gcores_rx_get(sp, 0) & 0x0000FFFF;
	*((uint16_t *)sp->cur_rx_buf) = data;
	sp->cur_rx_buf += 2;
	sp->cur_rx_len -= 2;
}

static void spi_gcores_hw_xfer_rx_push32(struct spi_gcores *sp)
{
	uint32_t data;

	data = spi_gcores_rx_get(sp, 0) & 0xFFFFFFFF;
	*((uint32_t *)sp->cur_rx_buf) = data;
	sp->cur_rx_buf += 4;
	sp->cur_rx_len -= 4;
}

static void spi_gcores_hw_xfer_rx_push64(struct spi_gcores *sp)
{
	int i;

	for (i = 0; i < 2; ++i) {
		uint32_t data;

		data = spi_gcores_rx_get(sp, i) & 0xFFFFFFFF;
		*((uint32_t *)sp->cur_rx_buf) = data;
		sp->cur_rx_buf += 4;
		sp->cur_rx_len -= 4;
	}
}

static void spi_gcores_hw_xfer_rx_push128(struct spi_gcores *sp)
{
	int i;

	for (i = 0; i < 4; ++i) {
		uint32_t data;

		data = spi_gcores_rx_get(sp, i) & 0xFFFFFFFF;
		*((uint32_t *)sp->cur_rx_buf) = data;
		sp->cur_rx_buf += 4;
		sp->cur_rx_len -= 4;
	}
}

/**
 * Write pending data in RX registers
 * @sp: SPI OCORE controller
 */
static void spi_gcores_hw_xfer_tx_push(struct spi_gcores *sp)
{
	uint8_t nbits;

	nbits = spi_gcores_hw_xfer_bits_per_word(sp);
	if (nbits >= 8) {
		spi_gcores_hw_xfer_tx_push8(sp);
	} else if (nbits >= 16) {
		spi_gcores_hw_xfer_tx_push16(sp);
	} else if (nbits >= 32) {
		spi_gcores_hw_xfer_tx_push32(sp);
	} else if (nbits >= 64) {
		spi_gcores_hw_xfer_tx_push64(sp);
	} else if (nbits >= 128) {
		spi_gcores_hw_xfer_tx_push128(sp);
	}
}

/**
 * Read received data from TX registers
 * @sp: SPI OCORE controller
 */
static void spi_gcores_hw_xfer_rx_pop(struct spi_gcores *sp)
{
	uint8_t nbits;

	nbits = spi_gcores_hw_xfer_bits_per_word(sp);
	if (nbits >= 8) {
		spi_gcores_hw_xfer_rx_push8(sp);
	} else if (nbits >= 16) {
		spi_gcores_hw_xfer_rx_push16(sp);
	} else if (nbits >= 32) {
		spi_gcores_hw_xfer_rx_push32(sp);
	} else if (nbits >= 64) {
		spi_gcores_hw_xfer_rx_push64(sp);
	} else if (nbits >= 128) {
		spi_gcores_hw_xfer_rx_push128(sp);
	}
}

static void spi_gcores_hw_xfer_start(struct spi_gcores *sp)
{
	unsigned int cs = sp->master->cur_msg->spi->chip_select;

	/* Optimize:
	 * Probably we can avoid to write CTRL DIVIDER and CS everytime
	 */
	spi_gcores_hw_xfer_config(sp, sp->cur_ctrl, sp->cur_divider);
	spi_gcores_hw_xfer_cs(sp, cs, 1);
	spi_gcores_hw_xfer_go(sp);
}

/**
 * Wait until something change in a given register
 * @sp: SPI OCORE controller
 * @reg: register to query
 * @mask: bitmask to apply on register value
 * @val: expected result
 * @timeout: timeout in jiffies
 *
 * Timeout is necessary to avoid to stay here forever when the chip
 * does not answer correctly.
 *
 * Return: 0 on success, -ETIMEDOUT on timeout
 */
static int spi_gcores_wait(struct spi_gcores *sp,
			   int reg, uint32_t mask, uint32_t val,
			   const unsigned long timeout)
{
	unsigned long j;

	j = jiffies + timeout;
	while (1) {
		uint32_t tmp = sp->read(sp, reg);

		if ((tmp & mask) == val)
			break;

		if (time_after(jiffies, j))
			return -ETIMEDOUT;
	}
	return 0;
}

/**
 * Wait transfer completion
 * @sp: SPI OCORE controller
 * @timeoute: in jiffies
 *
 * return: 0 on success, -ETIMEDOUT on timeout
 */
static int spi_gcores_hw_xfer_wait_complete(struct spi_gcores *sp,
					    const unsigned long timeout)
{
	return spi_gcores_wait(sp, SPI_GCORES_CTRL,
			       SPI_GCORES_CTRL_BUSY, 0, timeout);
}

/**
 * Finish Linux SPI transfer
 * @sp: SPI OCORE controller
 * @xfer: Linux SPI transfer to finish
 *
 * Return: 0 on success, otherwise a negative errno
 */
static int spi_gcores_sw_xfer_finish(struct spi_gcores *sp)
{
	if (sp->cur_xfer->delay_usecs)
		udelay(sp->cur_xfer->delay_usecs);
	if (sp->cur_xfer->cs_change) {
		unsigned int cs;

		cs = sp->master->cur_msg->spi->chip_select;
		spi_gcores_hw_xfer_cs(sp, cs, 0);
	}

	spi_gcores_hw_xfer_config(sp, 0, 0);

	sp->cur_xfer = NULL;
	sp->cur_tx_buf = NULL;
	sp->cur_tx_len = 0;
	sp->cur_rx_buf = NULL;
	sp->cur_rx_len = 0;

	return 0;
}

/**
 * Initialize data for next software transfer
 * @sp: SPI OCORE controller
 *
 * Return: 0 on success,
 *         -ENODATA when there are no transfers left,
 *         -EINVAL invalid bit per word
 */
static int spi_gcores_sw_xfer_next_init(struct spi_gcores *sp)
{
	struct list_head *head = &sp->master->cur_msg->transfers;
	uint32_t hz;

	if (spi_gcores_hw_xfer_bits_per_word(sp) > 128)
		return -EINVAL;

	if (!sp->cur_xfer) {
		sp->cur_xfer = list_first_entry_or_null(head,
							struct spi_transfer,
							transfer_list);
		if (!sp->cur_xfer)
			return -ENODATA;
	} else {
		if (list_is_last(&sp->cur_xfer->transfer_list, head))
			return -ENODATA;

		sp->cur_xfer = list_next_entry(sp->cur_xfer, transfer_list);
	}

	sp->cur_ctrl = sp->ctrl_base;
	sp->cur_ctrl |= spi_gcores_hw_xfer_bits_per_word(sp);
	if (sp->cur_xfer->speed_hz)
		hz = sp->cur_xfer->speed_hz;
	else
		hz = sp->master->cur_msg->spi->max_speed_hz;
	sp->cur_divider = 1 + (sp->clock_hz / (hz * 2));
	sp->cur_tx_buf = sp->cur_xfer->tx_buf;
	sp->cur_tx_len = sp->cur_xfer->len;
	sp->cur_rx_buf = sp->cur_xfer->rx_buf;
	sp->cur_rx_len = sp->cur_xfer->len;

	return 0;
}

/**
 * Start next software transfer
 * @sp: SPI OCORE controller
 *
 * Return: 0 on success, -ENODEV when missing transfers
 */
static int spi_gcores_sw_xfer_next_start(struct spi_gcores *sp)
{
	int err;

	err = spi_gcores_sw_xfer_next_init(sp);
	if (err) {
		spi_finalize_current_message(sp->master);
		return err;
	}
	spi_gcores_hw_xfer_tx_push(sp);
	spi_gcores_hw_xfer_start(sp);

	return 0;
}

/**
 * TX pending status
 * @sp: SPI OCORE controller
 * Return: True is there are still pending data in the current transfer
 */
static bool spi_gcores_sw_xfer_tx_pending(struct spi_gcores *sp)
{
	return sp->cur_tx_len;
}

/**
 * Process an SPI transfer
 * @sp: SPI OCORE controller
 *
 * Return: 0 on success, -ENODATA when there is nothing to process
 */
static int spi_gcores_process(struct spi_gcores *sp)
{
	uint32_t ctrl = sp->read(sp, SPI_GCORES_CTRL);

	if (ctrl & SPI_GCORES_CTRL_BUSY)
		return -ENODATA;

	spi_gcores_hw_xfer_rx_pop(sp);
	if (spi_gcores_sw_xfer_tx_pending(sp)) {
		spi_gcores_hw_xfer_tx_push(sp);
		spi_gcores_hw_xfer_start(sp);
	} else {
		spi_gcores_sw_xfer_finish(sp);
		spi_gcores_sw_xfer_next_start(sp);
	}

	return 0;
}

/**
 * Process an SPI transfer
 * @sp: SPI OCORE controller
 * @timeout: timeout in milli-seconds
 *
 * Return: 0 on success, -ENODATA when there is nothing to process, -ETIMEDOUT
 *         when is still pending after timeout
 */
static int spi_gcores_process_poll(struct spi_gcores *sp, unsigned int timeout)
{
	int err;

	err = spi_gcores_hw_xfer_wait_complete(sp, msecs_to_jiffies(timeout));
	if (err)
		return err;
	err = spi_gcores_process(sp);
	if (err)
		return err;
	return 0;
}

static irqreturn_t spi_gcores_irq_handler(int irq, void *arg)
{
	struct spi_gcores *sp = arg;
	int err;

	err = spi_gcores_process(sp);
	if (err)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

/**
 * Transfer one SPI message
 */
static int spi_gcores_transfer_one_message(struct spi_master *master,
					   struct spi_message *mesg)
{
	struct spi_gcores *sp = spi_master_get_devdata(master);
	int err = 0;

	err = spi_gcores_sw_xfer_next_start(sp);
	if (sp->flags & SPI_GCORES_FLAG_POLL) {
		do {
			err =spi_gcores_process_poll(sp, 100);
		} while (!err);
	}

	return 0;
}

static int spi_gcores_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct spi_gcores *sp;
	struct spi_gcores_platform_data *pdata;
	struct resource *r;
	int err;
	int irq;

	master = spi_alloc_master(&pdev->dev, sizeof(*sp));
	if (!master) {
		dev_err(&pdev->dev, "failed to allocate spi master\n");
		return -ENOMEM;
	}

	sp = spi_master_get_devdata(master);
	platform_set_drvdata(pdev, sp);
	sp->master = master;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		err = -EINVAL;
		goto err_get_pdata;
	}

	sp->clock_hz = pdata->clock_hz;

	/* configure SPI master */
	master->setup = spi_gcores_setup;
	master->cleanup = spi_gcores_cleanup;
	master->transfer_one_message = spi_gcores_transfer_one_message;
	master->num_chipselect = pdata->num_chipselect;
	master->bits_per_word_mask = BIT(32 - 1);
	if (pdata->big_endian) {
		sp->read = spi_gcores_ioread32be;
		sp->write = spi_gcores_iowrite32be;
	} else {
		sp->read = spi_gcores_ioread32;
		sp->write = spi_gcores_iowrite32;
	}

	/* assign resources */
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sp->mem = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(sp->mem)) {
		err = PTR_ERR(sp->mem);
		goto err_get_mem;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq == -ENXIO) {
		sp->flags |= SPI_GCORES_FLAG_POLL;
	} else {
		if (irq < 0) {
			err = irq;
			goto err_get_irq;
		}
	}

	if (!(sp->flags & SPI_GCORES_FLAG_POLL)) {
		sp->ctrl_base |= SPI_GCORES_CTRL_IE;
		err = request_any_context_irq(irq, spi_gcores_irq_handler,
					      0, pdev->name, sp);
		if (err < 0) {
			dev_err(&pdev->dev, "Cannot claim IRQ\n");
			goto err_irq;
		}
	}

	err = spi_register_master(master);
	if (err)
		goto err_reg_spi;

	return 0;

err_reg_spi:
	if (!(sp->flags & SPI_GCORES_FLAG_POLL))
		free_irq(irq, sp);
err_irq:
err_get_irq:
	devm_iounmap(&pdev->dev, sp->mem);
err_get_mem:
err_get_pdata:
	spi_master_put(master);
	return err;
}

static int spi_gcores_remove(struct platform_device *pdev)
{
	struct spi_gcores *sp = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	if (irq > 0)
		free_irq(irq, sp);
	spi_unregister_master(sp->master);
	devm_iounmap(&pdev->dev, sp->mem);
	platform_set_drvdata(pdev, NULL);
	spi_master_put(sp->master);

	return 0;
}

static const struct platform_device_id spi_gcores_id_table[] = {
	{
		.name = "ocores-i2c",
		.driver_data = TYPE_GCORES,
	},
	{ .name = "" }, /* last */
};

static struct platform_driver spi_gcores_driver = {
	.probe		= spi_gcores_probe,
	.remove	= spi_gcores_remove,
	.driver	= {
		.name	= "spi-gcores",
		.owner	= THIS_MODULE,
	},
	.id_table = spi_gcores_id_table,
};
module_platform_driver(spi_gcores_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_DESCRIPTION("SPI controller driver for OHWR General-Cores SPI Master");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DEVICE_TABLE(platform, spi_gcores_id_table);
