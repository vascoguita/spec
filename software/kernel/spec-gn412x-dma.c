// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@vaga.pv.it>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>

/**
 * dma_cookie_complete - complete a descriptor
 * @tx: descriptor to complete
 *
 * Mark this descriptor complete by updating the channels completed
 * cookie marker.  Zero the descriptors cookie to prevent accidental
 * repeated completions.
 *
 * Note: caller is expected to hold a lock to prevent concurrency.
 */
static inline void dma_cookie_complete(struct dma_async_tx_descriptor *tx)
{
	BUG_ON(tx->cookie < DMA_MIN_COOKIE);
	tx->chan->completed_cookie = tx->cookie;
	tx->cookie = 0;
}

/**
 * dma_cookie_init - initialize the cookies for a DMA channel
 * @chan: dma channel to initialize
 */
static inline void dma_cookie_init(struct dma_chan *chan)
{
	chan->cookie = DMA_MIN_COOKIE;
	chan->completed_cookie = DMA_MIN_COOKIE;
}

/**
 * dma_cookie_assign - assign a DMA engine cookie to the descriptor
 * @tx: descriptor needing cookie
 *
 * Assign a unique non-zero per-channel cookie to the descriptor.
 * Note: caller is expected to hold a lock to prevent concurrency.
 */
static inline dma_cookie_t dma_cookie_assign(struct dma_async_tx_descriptor *tx)
{
	struct dma_chan *chan = tx->chan;
	dma_cookie_t cookie;

	cookie = chan->cookie + 1;
	if (cookie < DMA_MIN_COOKIE)
		cookie = DMA_MIN_COOKIE;
	tx->cookie = chan->cookie = cookie;

	return cookie;
}

/**
 * dma_cookie_status - report cookie status
 * @chan: dma channel
 * @cookie: cookie we are interested in
 * @state: dma_tx_state structure to return last/used cookies
 *
 * Report the status of the cookie, filling in the state structure if
 * non-NULL.  No locking is required.
 */
static inline enum dma_status dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	dma_cookie_t used, complete;

	used = chan->cookie;
	complete = chan->completed_cookie;

	if (state) {
		state->last = complete;
		state->used = used;
		state->residue = 0;
	}
	return dma_async_is_complete(cookie, complete, used);
}


enum gn412x_dma_regs {
	GN412X_DMA_CTRL = 0x00,
	GN412X_DMA_STAT = 0x04,
	GN412X_DMA_ADDR_MEM = 0x08,
	GN412X_DMA_ADDR_L = 0x0C,
	GN412X_DMA_ADDR_H = 0x10,
	GN412X_DMA_LEN = 0x14,
	GN412X_DMA_NEXT_L = 0x18,
	GN412X_DMA_NEXT_H = 0x1C,
	GN412X_DMA_ATTR = 0x20,
	GN412X_DMA_CUR_ADDR_MEM = 0x24,
	GN412X_DMA_CUR_ADDR_L = 0x28,
	GN412X_DMA_CUR_ADDR_H = 0x2C,
	GN412X_DMA_CUR_LEN = 0x30,
};

enum gn412x_dma_regs_ctrl {
	GN412X_DMA_CTRL_START = BIT(0),
	GN412X_DMA_CTRL_ABORT = BIT(1),
	GN412X_DMA_CTRL_SWAPPING = 0xC,
};

enum gn412x_dma_ctrl_swapping {
	GN412X_DMA_CTRL_SWAPPING_NONE = 0,
	GN412X_DMA_CTRL_SWAPPING_16,
	GN412X_DMA_CTRL_SWAPPING_16_WORD,
	GN412X_DMA_CTRL_SWAPPING_32,
};

#define GN412X_DMA_ATTR_DIR_MEM_TO_DEV (1 << 0)
#define GN412X_DMA_ATTR_CHAIN (1 << 1)
struct gn412x_dma_tx;

/**
 * List of device identifiers
 */
enum gn412x_dma_id_enum {
	GN412X_DMA_GN4124_IPCORE = 0,
};

enum gn412x_dma_state {
	GN412X_DMA_STAT_IDLE = 0,
	GN412X_DMA_STAT_BUSY,
	GN412X_DMA_STAT_ERROR,
	GN412X_DMA_STAT_ABORTED,
};
#define GN412X_DMA_STAT_ACK BIT(2)


/**
 * Transfer descriptor an hardware transfer
 * @start_addr: pointer where start to retrieve data from device memory
 * @dma_addr_l: low 32bit of the dma address on host memory
 * @dma_addr_h: high 32bit of the dma address on host memory
 * @dma_len: number of bytes to transfer from device to host
 * @next_addr_l: low 32bit of the address of the next memory area to use
 * @next_addr_h: high 32bit of the address of the next memory area to use
 * @attribute: dma information about data transferm. At the moment it is used
 *             only to provide the "last item" bit, direction is fixed to
 *             device->host
 *
 * note: it must be a power of 2 in order to be used also as alignement
 *       within the DMA pool
 */
struct gn412x_dma_tx_hw {
	uint32_t start_addr;
	uint32_t dma_addr_l;
	uint32_t dma_addr_h;
	uint32_t dma_len;
	uint32_t next_addr_l;
	uint32_t next_addr_h;
	uint32_t attribute;
	uint32_t reserved; /* alignement */
};


/**
 * DMA channel descriptor
 * @chan: dmaengine channel
 * @pending_list: list of pending transfers
 * @tx_curr: current transfer
 * @task: tasklet for DMA start
 * @lock: protects: pending_list, tx_curr, sconfig
 * @sconfig: channel configuration to be used
 * @error: number of errors detected
 */
struct gn412x_dma_chan {
	struct dma_chan chan;
	struct list_head pending_list;
	struct gn412x_dma_tx *tx_curr;
	struct tasklet_struct task;
	spinlock_t lock;
	struct dma_slave_config sconfig;
	unsigned int error;
};
static inline struct gn412x_dma_chan *to_gn412x_dma_chan(struct dma_chan *_ptr)
{
	return container_of(_ptr, struct gn412x_dma_chan, chan);
}
static inline bool gn412x_dma_has_pending_tx(struct gn412x_dma_chan *gn412x_dma_chan)
{
	return !list_empty(&gn412x_dma_chan->pending_list);
}

/**
 * DMA device descriptor
 * @pdev: platform device associated
 * @addr: component base address
 * @dma: dmaengine device
 * @chan: list of DMA channels
 * @pool: shared DMA pool for HW descriptors
 * @pool_list: list of HW descriptor allocated
 */
struct gn412x_dma_device {
	struct platform_device *pdev;
	void __iomem *addr;
	struct dma_device dma;
	struct gn412x_dma_chan chan;
	struct dma_pool *pool;
	struct list_head *pool_list;

	struct dentry *dbg_dir;
#define GN412X_DMA_DBG_REG_NAME "regs"
	struct dentry *dbg_reg;
	struct debugfs_regset32 dbg_reg32;
};
static inline struct gn412x_dma_device *to_gn412x_dma_device(struct dma_device *_ptr)
{
	return container_of(_ptr, struct gn412x_dma_device, dma);
}


/**
 * DMA transfer descriptor
 * @tx: dmaengine descriptor
 * @sgl_hw: scattelist HW descriptors
 * @sg_len: number of entries in the scatterlist
 * @list: token to indentify this transfer in the pending list
 */
struct gn412x_dma_tx {
	struct dma_async_tx_descriptor tx;
	struct gn412x_dma_tx_hw **sgl_hw;
	unsigned int sg_len;
	struct list_head list;
};
static inline struct gn412x_dma_tx *to_gn412x_dma_tx(struct dma_async_tx_descriptor *_ptr)
{
	return container_of(_ptr, struct gn412x_dma_tx, tx);
}

#define REG32(_name, _offset) {.name = _name, .offset = _offset}
static const struct debugfs_reg32 gn412x_dma_debugfs_reg32[] = {
	REG32("DMACTRLR", GN412X_DMA_CTRL),
	REG32("DMASTATR", GN412X_DMA_STAT),
	REG32("DMACSTARTR", GN412X_DMA_ADDR_MEM),
	REG32("DMAHSTARTLR", GN412X_DMA_ADDR_L),
	REG32("DMAHSTARTHR", GN412X_DMA_ADDR_H),
	REG32("DMALENR", GN412X_DMA_LEN),
	REG32("DMANEXTLR", GN412X_DMA_NEXT_L),
	REG32("DMANEXTHR", GN412X_DMA_NEXT_H),
	REG32("DMAATTRIBR", GN412X_DMA_ATTR),
	REG32("DMACURCSTARTR", GN412X_DMA_CUR_ADDR_MEM),
	REG32("DMACURHSTARTLR", GN412X_DMA_CUR_ADDR_L),
	REG32("DMACURHSTARTHR", GN412X_DMA_CUR_ADDR_H),
	REG32("DMACURLENR", GN412X_DMA_CUR_LEN),
};

/**
 * Start DMA transfer
 * @gn412x_dma: DMA device
 */
static void gn412x_dma_ctrl_start(struct gn412x_dma_device *gn412x_dma)
{
	uint32_t ctrl;

	ctrl = ioread32(gn412x_dma->addr + GN412X_DMA_CTRL);
	ctrl |= GN412X_DMA_CTRL_START;
	iowrite32(ctrl, gn412x_dma->addr + GN412X_DMA_CTRL);
	dev_dbg(&gn412x_dma->pdev->dev, "%s: stat: 0x%x\n",
		__func__, ioread32(gn412x_dma->addr + GN412X_DMA_STAT));
}

/**
 * Abort on going DMA transfer
 * @gn412x_dma: DMA device
 */
static void gn412x_dma_ctrl_abort(struct gn412x_dma_device *gn412x_dma)
{
	uint32_t ctrl;

	ctrl = ioread32(gn412x_dma->addr + GN412X_DMA_CTRL);
	ctrl |= GN412X_DMA_CTRL_ABORT;
	iowrite32(ctrl, gn412x_dma->addr + GN412X_DMA_CTRL);
}

/**
 * Set swapping option
 * @gn412x_dma: DMA device
 * @swap: swapping option
 */
static void gn412x_dma_ctrl_swapping(struct gn412x_dma_device *gn412x_dma,
				     enum gn412x_dma_ctrl_swapping swap)
{
	uint32_t ctrl = swap;

	iowrite32(ctrl, gn412x_dma->addr + GN412X_DMA_CTRL);
}

static enum gn412x_dma_state gn412x_dma_state(struct gn412x_dma_device *gn412x_dma)
{
	return ioread32(gn412x_dma->addr + GN412X_DMA_STAT);
}

static bool gn412x_dma_is_busy(struct gn412x_dma_device *gn412x_dma)
{
	uint32_t status;

	status = ioread32(gn412x_dma->addr + GN412X_DMA_STAT);

	return status & GN412X_DMA_STAT_BUSY;
}

static bool gn412x_dma_is_abort(struct gn412x_dma_device *gn412x_dma)
{
	uint32_t status;

	status = ioread32(gn412x_dma->addr + GN412X_DMA_STAT);

	return status & GN412X_DMA_STAT_ABORTED;
}

static void gn412x_dma_irq_ack(struct gn412x_dma_device *gn412x_dma)
{
	iowrite32(GN412X_DMA_STAT_ACK, gn412x_dma->addr + GN412X_DMA_STAT);
}

static void gn412x_dma_config(struct gn412x_dma_device *gn412x_dma,
			      struct gn412x_dma_tx_hw *tx_hw)
{
	iowrite32(tx_hw->start_addr, gn412x_dma->addr + GN412X_DMA_ADDR_MEM);
	iowrite32(tx_hw->dma_addr_l, gn412x_dma->addr + GN412X_DMA_ADDR_L);
	iowrite32(tx_hw->dma_addr_h, gn412x_dma->addr + GN412X_DMA_ADDR_H);
	iowrite32(tx_hw->dma_len, gn412x_dma->addr + GN412X_DMA_LEN);
	iowrite32(tx_hw->next_addr_l, gn412x_dma->addr + GN412X_DMA_NEXT_L);
	iowrite32(tx_hw->next_addr_h, gn412x_dma->addr + GN412X_DMA_NEXT_H);
	iowrite32(tx_hw->attribute, gn412x_dma->addr + GN412X_DMA_ATTR);
}

static int gn412x_dma_alloc_chan_resources(struct dma_chan *dchan)
{
	struct gn412x_dma_chan *chan = to_gn412x_dma_chan(dchan);

	memset(&chan->sconfig, 0, sizeof(struct dma_slave_config));
	chan->sconfig.direction = DMA_DEV_TO_MEM;

	return 0;
}

static void gn412x_dma_free_chan_resources(struct dma_chan *dchan)
{
}

/**
 * Add a descriptor to the pending DMA transfer queue.
 * This will not trigger any DMA transfer: here we just collect DMA
 * transfer descriptions.
 */
static dma_cookie_t gn412x_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct gn412x_dma_tx *gn412x_dma_tx = to_gn412x_dma_tx(tx);
	struct gn412x_dma_chan *chan = to_gn412x_dma_chan(tx->chan);
	dma_cookie_t cookie;
	unsigned long flags;

	dev_dbg(&tx->chan->dev->device, "%s submit %p\n", __func__, tx);
	spin_lock_irqsave(&chan->lock, flags);
	cookie = dma_cookie_assign(tx);
	list_add_tail(&gn412x_dma_tx->list, &chan->pending_list);
	spin_unlock_irqrestore(&chan->lock, flags);

	return cookie;
}

static void gn412x_dma_prep_fixup(struct gn412x_dma_tx_hw *tx_hw,
				  dma_addr_t next_addr)
{
	if (!tx_hw || !next_addr)
		return;
	tx_hw->next_addr_l = ((next_addr >> 0)  & 0xFFFFFFFF);
	tx_hw->next_addr_h = ((next_addr >> 32) & 0xFFFFFFFF);
}

static void gn412x_dma_prep(struct gn412x_dma_tx_hw *tx_hw,
			    struct scatterlist *sg,
			    dma_addr_t start_addr)
{
	tx_hw->start_addr = start_addr & 0xFFFFFFFF;
	tx_hw->dma_addr_l = sg_dma_address(sg);
	tx_hw->dma_addr_l &= 0xFFFFFFFF;
	tx_hw->dma_addr_h = ((uint64_t)sg_dma_address(sg) >> 32);
	tx_hw->dma_addr_h &= 0xFFFFFFFF;
	tx_hw->dma_len = sg_dma_len(sg);
	tx_hw->next_addr_l = 0x00000000;
	tx_hw->next_addr_h = 0x00000000;
	tx_hw->attribute = 0x0;
	if (!sg_is_last(sg))
		tx_hw->attribute = GN412X_DMA_ATTR_CHAIN;
}

static struct dma_async_tx_descriptor *gn412x_dma_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct gn412x_dma_device *gn412x_dma = to_gn412x_dma_device(chan->device);
	struct dma_slave_config *sconfig = &to_gn412x_dma_chan(chan)->sconfig;
	struct gn412x_dma_tx *gn412x_dma_tx;
	struct scatterlist *sg;
	dma_addr_t src_addr;
	int i;

	if (unlikely(direction != DMA_DEV_TO_MEM)) {
		dev_err(&chan->dev->device,
			"Support only DEV -> MEM transfers\n");
		goto err;
	}

	if (unlikely(sconfig->direction != direction)) {
		dev_err(&chan->dev->device,
			"Transfer and slave configuration disagree on DMA direction\n");
		goto err;
	}

	if (unlikely(!sgl || !sg_len)) {
		dev_err(&chan->dev->device,
			"You must provide a DMA scatterlist\n");
		goto err;
	}

	gn412x_dma_tx = kzalloc(sizeof(struct gn412x_dma_tx), GFP_NOWAIT);
	if (!gn412x_dma_tx)
		goto err;

	dma_async_tx_descriptor_init(&gn412x_dma_tx->tx, chan);
	gn412x_dma_tx->tx.tx_submit = gn412x_dma_tx_submit;
	gn412x_dma_tx->sg_len = sg_len;

	/* Configure the hardware for this transfer */
	gn412x_dma_tx->sgl_hw = kcalloc(sizeof(struct gn412x_dma_tx_hw *),
					gn412x_dma_tx->sg_len,
					GFP_KERNEL);
	if (!gn412x_dma_tx->sgl_hw)
		goto err_alloc_sglhw;


	src_addr = sconfig->src_addr;
	for_each_sg(sgl, sg, sg_len, i) {
		dma_addr_t phys;

		if (sg_dma_len(sg) > dma_get_max_seg_size(chan->device->dev)) {
			dev_err(&chan->dev->device,
				"Maximum transfer size %d, got %d on transfer %d\n",
				0x3FFF, sg_dma_len(sg), i);
			goto err_alloc_pool;
		}
		gn412x_dma_tx->sgl_hw[i] = dma_pool_alloc(gn412x_dma->pool,
							  GFP_DMA,
							  &phys);
		if (!gn412x_dma_tx->sgl_hw[i])
			goto err_alloc_pool;

		if (i > 0) {
			/*
			 * To build the chained transfer the previous
			 * descriptor (sgl_hw[i - 1]) must point to
			 * the physical address of current one (phys)
			 */
			gn412x_dma_prep_fixup(gn412x_dma_tx->sgl_hw[i - 1],
					      phys);
		} else {
			gn412x_dma_tx->tx.phys = phys;
		}
		gn412x_dma_prep(gn412x_dma_tx->sgl_hw[i], sg, src_addr);
		src_addr += sg_dma_len(sg);
	}

	for_each_sg(sgl, sg, sg_len, i) {
		struct gn412x_dma_tx_hw *tx_hw = gn412x_dma_tx->sgl_hw[i];

		dev_dbg(&chan->dev->device,
			"%s\n"
			"\tsegment: %d\n"
			"\tstart_addr: 0x%x\n"
			"\tdma_addr_l: 0x%x\n"
			"\tdma_addr_h: 0x%x\n"
			"\tdma_len: 0x%x\n"
			"\tnext_addr_l: 0x%x\n"
			"\tnext_addr_h: 0x%x\n"
			"\tattribute: 0x%x\n",
			__func__, i,
			tx_hw->start_addr,
			tx_hw->dma_addr_l,
			tx_hw->dma_addr_h,
			tx_hw->dma_len,
			tx_hw->next_addr_l,
			tx_hw->next_addr_h,
			tx_hw->attribute);
	}

	dev_dbg(&chan->dev->device, "%s prepared %p\n", __func__,
		&gn412x_dma_tx->tx);

	return &gn412x_dma_tx->tx;

err_alloc_pool:
	while (--i >= 0)
		dma_pool_free(gn412x_dma->pool,
			      gn412x_dma_tx->sgl_hw[i],
			      gn412x_dma_tx->tx.phys);
	kfree(gn412x_dma_tx->sgl_hw);
err_alloc_sglhw:
	kfree(gn412x_dma_tx);
err:
	return NULL;
}

static void gn412x_dma_schedule_next(struct gn412x_dma_chan *gn412x_dma_chan)
{
	unsigned long flags;
	bool pending;

	spin_lock_irqsave(&gn412x_dma_chan->lock, flags);
	pending = gn412x_dma_has_pending_tx(gn412x_dma_chan);
	spin_unlock_irqrestore(&gn412x_dma_chan->lock, flags);

	if (pending)
		tasklet_schedule(&gn412x_dma_chan->task);
}

static void gn412x_dma_issue_pending(struct dma_chan *chan)
{
	gn412x_dma_schedule_next(to_gn412x_dma_chan(chan));
}

static void gn412x_dma_start_task(unsigned long arg)
{
	struct gn412x_dma_chan *chan = (struct gn412x_dma_chan *)arg;
	struct gn412x_dma_device *gn412x_dma;
	unsigned long flags;

	gn412x_dma = to_gn412x_dma_device(chan->chan.device);
	if (unlikely(gn412x_dma_is_busy(gn412x_dma))) {
		dev_err(&gn412x_dma->pdev->dev,
			"Failed to start DMA transfer: channel busy\n");
		return;
	}

	spin_lock_irqsave(&chan->lock, flags);
	if (gn412x_dma_has_pending_tx(chan)) {
		struct gn412x_dma_tx *tx;

		tx = list_first_entry(&chan->pending_list,
				      struct gn412x_dma_tx, list);
		list_del(&tx->list);
		gn412x_dma_config(gn412x_dma, tx->sgl_hw[0]);
		gn412x_dma_ctrl_swapping(gn412x_dma,
					 GN412X_DMA_CTRL_SWAPPING_NONE);
		gn412x_dma_ctrl_start(gn412x_dma);
		chan->tx_curr = tx;
	}
	spin_unlock_irqrestore(&chan->lock, flags);

}

static enum dma_status gn412x_dma_tx_status(struct dma_chan *chan,
					    dma_cookie_t cookie,
					    struct dma_tx_state *state)
{
	return DMA_ERROR;
}

static int gn412x_dma_slave_config(struct dma_chan *chan,
				   struct dma_slave_config *sconfig)
{
	struct gn412x_dma_chan *gn412x_dma_chan = to_gn412x_dma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&gn412x_dma_chan->lock, flags);
	memcpy(&gn412x_dma_chan->sconfig, sconfig,
	       sizeof(struct dma_slave_config));
	spin_unlock_irqrestore(&gn412x_dma_chan->lock, flags);

	return 0;
}

static int gn412x_dma_terminate_all(struct dma_chan *chan)
{
	struct gn412x_dma_device *gn412x_dma;

	gn412x_dma = to_gn412x_dma_device(chan->device);
	gn412x_dma_ctrl_abort(gn412x_dma);
	/* FIXME remove all pending */
	if (!gn412x_dma_is_abort(gn412x_dma)) {
		dev_err(&gn412x_dma->pdev->dev,
			"Failed to abort DMA transfer\n");
		return -EINVAL;
	}

	return 0;
}

static int gn412x_dma_device_control(struct dma_chan *chan,
				enum dma_ctrl_cmd cmd,
				unsigned long arg)
{
	switch (cmd) {
	case DMA_SLAVE_CONFIG:
		return gn412x_dma_slave_config(chan,
					       (struct dma_slave_config *)arg);
	case DMA_TERMINATE_ALL:
		return gn412x_dma_terminate_all(chan);
	case DMA_PAUSE:
		break;
	case DMA_RESUME:
		break;
	default:
		break;
	}
	return -ENODEV;
}


static irqreturn_t gn412x_dma_irq_handler(int irq, void *arg)
{
	struct gn412x_dma_device *gn412x_dma = arg;
	struct gn412x_dma_chan *chan = &gn412x_dma->chan;
	struct gn412x_dma_tx *tx;
	unsigned long flags;
	unsigned int i;
	enum gn412x_dma_state state;

	/* FIXME check for spurious - need HDL fix */
	gn412x_dma_irq_ack(gn412x_dma);

	spin_lock_irqsave(&chan->lock, flags);
	tx = chan->tx_curr;
	chan->tx_curr = NULL;
	spin_unlock_irqrestore(&chan->lock, flags);

	state = gn412x_dma_state(gn412x_dma);
	switch (state) {
	case GN412X_DMA_STAT_IDLE:
		dma_cookie_complete(&tx->tx);
		if (tx->tx.callback)
			tx->tx.callback(tx->tx.callback_param);
		break;
	case GN412X_DMA_STAT_ERROR:
		dev_err(&gn412x_dma->pdev->dev,
			"DMA transfer failed: error\n");
		break;
	default:
		dev_err(&gn412x_dma->pdev->dev,
			"DMA transfer failed: inconsitent state %d\n",
			state);
		break;
	}

	/* Clean up memory */
	for (i = 0; i < tx->sg_len; ++i)
		dma_pool_free(gn412x_dma->pool, tx->sgl_hw[i], tx->tx.phys);
	kfree(tx->sgl_hw);
	kfree(tx);

	gn412x_dma_schedule_next(chan);

	return IRQ_HANDLED;
}


static int gn412x_dma_dbg_init(struct gn412x_dma_device *gn412x_dma)
{
	struct dentry *dir, *file;
	int err;

	dir = debugfs_create_dir(dev_name(&gn412x_dma->pdev->dev), NULL);
	if (IS_ERR_OR_NULL(dir)) {
		err = PTR_ERR(dir);
		dev_warn(&gn412x_dma->pdev->dev,
			"Cannot create debugfs directory \"%s\" (%d)\n",
			dev_name(&gn412x_dma->pdev->dev), err);
		goto err_dir;
	}

	gn412x_dma->dbg_reg32.regs = gn412x_dma_debugfs_reg32;
	gn412x_dma->dbg_reg32.nregs = ARRAY_SIZE(gn412x_dma_debugfs_reg32);
	gn412x_dma->dbg_reg32.base = gn412x_dma->addr;
	file = debugfs_create_regset32(GN412X_DMA_DBG_REG_NAME, 0200,
				       dir, &gn412x_dma->dbg_reg32);
	if (IS_ERR_OR_NULL(file)) {
		err = PTR_ERR(file);
		dev_warn(&gn412x_dma->pdev->dev,
			 "Cannot create debugfs file \"%s\" (%d)\n",
			 GN412X_DMA_DBG_REG_NAME, err);
		goto err_reg32;
	}

	gn412x_dma->dbg_dir = dir;
	gn412x_dma->dbg_reg = file;
	return 0;

err_reg32:
	debugfs_remove_recursive(dir);
err_dir:
	return err;
}

static void gn412x_dma_dbg_exit(struct gn412x_dma_device *gn412x_dma)
{
	debugfs_remove_recursive(gn412x_dma->dbg_dir);
}

/**
 * Configure DMA Engine configuration
 */
static int gn412x_dma_engine_init(struct gn412x_dma_device *gn412x_dma,
				  struct device *parent)
{
	struct dma_device *dma = &gn412x_dma->dma;

	dma->dev = parent;
	if (dma_set_mask(dma->dev, DMA_BIT_MASK(64))) {
		/* Check if hardware supports 32-bit DMA */
		if (dma_set_mask(dma->dev, DMA_BIT_MASK(32))) {
			dev_err(dma->dev,
				"32-bit DMA addressing not available\n");
			return -EINVAL;
		}
	}

	INIT_LIST_HEAD(&dma->channels);

	dma_cap_zero(dma->cap_mask);
	dma_cap_set(DMA_SLAVE, dma->cap_mask);
	dma_cap_set(DMA_PRIVATE, dma->cap_mask);

	dma->device_alloc_chan_resources = gn412x_dma_alloc_chan_resources;
	dma->device_free_chan_resources = gn412x_dma_free_chan_resources;
	dma->device_prep_slave_sg = gn412x_dma_prep_slave_sg;
	dma->device_control = gn412x_dma_device_control;
	dma->device_tx_status = gn412x_dma_tx_status;
	dma->device_issue_pending = gn412x_dma_issue_pending;

	gn412x_dma->chan.chan.device = dma;
	list_add_tail(&gn412x_dma->chan.chan.device_node, &dma->channels);

	INIT_LIST_HEAD(&gn412x_dma->chan.pending_list);
	spin_lock_init(&gn412x_dma->chan.lock);
	tasklet_init(&gn412x_dma->chan.task, gn412x_dma_start_task,
		     (unsigned long)&gn412x_dma->chan);

	dma_set_max_seg_size(dma->dev, 0x7FFF);

	gn412x_dma->pool = dma_pool_create(dev_name(dma->dev), dma->dev,
					   sizeof(struct gn412x_dma_tx_hw),
					   sizeof(struct gn412x_dma_tx_hw),
					   0);
	if (!gn412x_dma->pool)
		return -ENOMEM;

	return 0;
}

/**
 * Release DMa engine configuration
 */
static void gn412x_dma_engine_exit(struct gn412x_dma_device *gn412x_dma)
{
	dma_pool_destroy(gn412x_dma->pool);
}

/**
 * It creates a new instance of the GN4124 DMA engine
 * @pdev: platform device
 *
 * @return: 0 on success otherwise a negative error code
 */
static int gn412x_dma_probe(struct platform_device *pdev)
{
	struct gn412x_dma_device *gn412x_dma;
	const struct resource *r;
	int err;

	/* FIXME set DMA mask on pdev? */
	gn412x_dma = kzalloc(sizeof(struct gn412x_dma_device), GFP_KERNEL);
	if (!gn412x_dma)
		return -ENOMEM;
	gn412x_dma->pdev = pdev;
	platform_set_drvdata(pdev, gn412x_dma);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "Missing memory resource\n");
		err = -EINVAL;
		goto err_res_mem;
	}
	gn412x_dma->addr = ioremap(r->start, resource_size(r));
	if (!gn412x_dma->addr) {
		err = -EADDRNOTAVAIL;
		goto err_map;
	}

	err = request_any_context_irq(platform_get_irq(pdev, 0),
				      gn412x_dma_irq_handler, 0,
				      dev_name(&pdev->dev), gn412x_dma);
	if (err < 0)
		goto err_irq;

	/* Get the pci_dev device because it is the one configured for DMA */
	err = gn412x_dma_engine_init(gn412x_dma, pdev->dev.parent->parent);
	if (err) {
		dev_err(&pdev->dev, "Can't allocate DMA pool\n");
		goto err_dma_init;
	}

	err = dma_async_device_register(&gn412x_dma->dma);
	if (err)
		goto err_reg;

	gn412x_dma_dbg_init(gn412x_dma);

	return 0;
err_reg:
	gn412x_dma_engine_exit(gn412x_dma);
err_dma_init:
	free_irq(platform_get_irq(pdev, 0), gn412x_dma);
err_irq:
	iounmap(gn412x_dma->addr);
err_map:
err_res_mem:
	kfree(gn412x_dma);
	return err;
}


/**
 * It removes an instance of the GN4124 DMA engine
 * @pdev: platform device
 *
 * @return: 0 on success otherwise a negative error code
 */

static int gn412x_dma_remove(struct platform_device *pdev)
{
	struct gn412x_dma_device *gn412x_dma = platform_get_drvdata(pdev);

	gn412x_dma_dbg_exit(gn412x_dma);

	dmaengine_terminate_all(&gn412x_dma->chan.chan);
	dma_async_device_unregister(&gn412x_dma->dma);
	gn412x_dma_engine_exit(gn412x_dma);
	free_irq(platform_get_irq(pdev, 0), gn412x_dma);
	iounmap(gn412x_dma->addr);
	kfree(gn412x_dma);

	return 0;
}


/**
 * List of all the compatible devices
 */
static const struct platform_device_id gn412x_dma_id[] = {
	{
		.name = "spec-gn412x-dma",
		.driver_data = GN412X_DMA_GN4124_IPCORE,
	},
	{ .name = "" }, /* last */
};

struct platform_driver gn412x_dma_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
	},
	.probe = gn412x_dma_probe,
	.remove = gn412x_dma_remove,
	.id_table = gn412x_dma_id
};

module_platform_driver(gn412x_dma_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_DESCRIPTION("SPEC GN4124 IP-Core DMA engine");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DEVICE_TABLE(platform, gn412x_dma_id);
