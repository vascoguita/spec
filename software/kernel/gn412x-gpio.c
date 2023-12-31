// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/debugfs.h>

#include "spec.h"
#include "gn412x.h"
#include "platform_data/gn412x-gpio.h"

/**
 * struct gn412x_gpio_dev GN412X device descriptor
 * @compl: for IRQ testing
 * @int_cfg_gpio: INT_CFG used for GPIO interrupts
 */
struct gn412x_gpio_dev {
	void __iomem *mem;
	struct gpio_chip gpiochip;
	struct irq_chip irqchip;

	struct completion	compl;
	struct gn412x_platform_data *pdata;

	struct dentry *dbg_dir;
#define GN412X_DBG_REG_NAME "regs"
	struct dentry *dbg_reg;
	struct debugfs_regset32 dbg_reg32;
};

static struct gn412x_platform_data gn412x_gpio_pdata_default = {
	.int_cfg = 0,
};

static inline struct gn412x_gpio_dev *to_gn412x_gpio_dev_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct gn412x_gpio_dev, gpiochip);
}

enum gn412x_gpio_versions {
	GN412X_VER = 0,
};

enum htvic_mem_resources {
	GN412X_MEM_BASE = 0,
};

#define REG32(_name, _offset) {.name = _name, .offset = _offset}
static const struct debugfs_reg32 gn412x_debugfs_reg32[] = {
	REG32("INT_CTRL", GNINT_CTRL),
	REG32("INT_STAT", GNINT_STAT),
	REG32("INT_CFG0", GNINT_CFG_0),
	REG32("INT_CFG1", GNINT_CFG_1),
	REG32("INT_CFG2", GNINT_CFG_2),
	REG32("INT_CFG3", GNINT_CFG_3),
	REG32("INT_CFG4", GNINT_CFG_4),
	REG32("INT_CFG5", GNINT_CFG_5),
	REG32("INT_CFG6", GNINT_CFG_6),
	REG32("INT_CFG7", GNINT_CFG_7),
	REG32("GPIO_BYPASS_MODE", GNGPIO_BYPASS_MODE),
	REG32("GPIO_DIRECTION_MODE", GNGPIO_DIRECTION_MODE),
	REG32("GPIO_OUTPUT_ENABLE", GNGPIO_OUTPUT_ENABLE),
	REG32("GPIO_OUTPUT_VALUE", GNGPIO_OUTPUT_VALUE),
	REG32("GPIO_INPUT_VALUE", GNGPIO_INPUT_VALUE),
	REG32("GPIO_INT_MASK", GNGPIO_INT_MASK),
	REG32("GPIO_INT_MASK_CLR", GNGPIO_INT_MASK_CLR),
	REG32("GPIO_INT_MASK_SET", GNGPIO_INT_MASK_SET),
	REG32("GPIO_INT_STATUS", GNGPIO_INT_STATUS),
	REG32("GPIO_INT_TYPE", GNGPIO_INT_TYPE),
	REG32("GPIO_INT_VALUE", GNGPIO_INT_VALUE),
	REG32("GPIO_INT_ON_ANY", GNGPIO_INT_ON_ANY),
};

static int gn412x_dbg_init(struct gn412x_gpio_dev *gn412x)
{
	struct dentry *dir;
#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
	struct dentry *file;
#endif
	int err;

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	struct device *dev = gn412x->gpiochip.dev;
#else
	struct device *dev = gn412x->gpiochip.parent;
#endif

	dir = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ERR_OR_NULL(dir)) {
		err = PTR_ERR(dir);
		dev_warn(dev,
			"Cannot create debugfs directory \"%s\" (%d)\n",
			dev_name(dev), err);
		goto err_dir;
	}

	gn412x->dbg_reg32.regs = gn412x_debugfs_reg32;
	gn412x->dbg_reg32.nregs = ARRAY_SIZE(gn412x_debugfs_reg32);
	gn412x->dbg_reg32.base = gn412x->mem;
#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE
	debugfs_create_regset32(GN412X_DBG_REG_NAME, 0200,
				       dir, &gn412x->dbg_reg32);
#else
	file = debugfs_create_regset32(GN412X_DBG_REG_NAME, 0200,
				       dir, &gn412x->dbg_reg32);
	if (IS_ERR_OR_NULL(file)) {
		err = PTR_ERR(file);
		dev_warn(dev,
			 "Cannot create debugfs file \"%s\" (%d)\n",
			 GN412X_DBG_REG_NAME, err);
		goto err_reg32;
	}
	gn412x->dbg_reg = file;
#endif

	gn412x->dbg_dir = dir;
	return 0;

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
err_reg32:
	debugfs_remove_recursive(dir);
#endif
err_dir:
	return err;
}

static void gn412x_dbg_exit(struct gn412x_gpio_dev *gn412x)
{
	debugfs_remove_recursive(gn412x->dbg_dir);
}

static uint32_t gn412x_ioread32(struct gn412x_gpio_dev *gn412x,
				int reg)
{
	return ioread32(gn412x->mem + reg);
}

static void gn412x_iowrite32(struct gn412x_gpio_dev *gn412x,
			     uint32_t val, int reg)
{
	return iowrite32(val, gn412x->mem + reg);
}


static int gn412x_gpio_reg_read(struct gpio_chip *chip,
				int reg, unsigned int offset)
{
	struct gn412x_gpio_dev *gn412x = to_gn412x_gpio_dev_gpio(chip);

	return gn412x_ioread32(gn412x, reg) & BIT(offset);
}

static void gn412x_gpio_reg_write(struct gpio_chip *chip,
				  int reg, unsigned int offset, int value)
{
	struct gn412x_gpio_dev *gn412x = to_gn412x_gpio_dev_gpio(chip);
	uint32_t regval;

	regval = gn412x_ioread32(gn412x, reg);
	if (value)
		regval |= BIT(offset);
	else
		regval &= ~BIT(offset);
	gn412x_iowrite32(gn412x, regval, reg);
}

/**
 * Enable Internal Gennum error's interrupts
 * @gn412x gn412x device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static void gn412x_gpio_int_cfg_enable_err(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg |= GNINT_STAT_ERR_ALL;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

/**
 * disable Internal Gennum error's interrupts
 * @gn412x gn412x device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static void gn412x_gpio_int_cfg_disable_err(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg &= ~GNINT_STAT_ERR_ALL;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

/**
 * Enable GPIO interrupts
 * @gn412x gn412x device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static void gn412x_gpio_int_cfg_enable_gpio(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg |= GNINT_STAT_GPIO;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

/**
 * Disable GPIO interrupts from a single configuration space
 * @gn412x gn412x device
 */
static void gn412x_gpio_int_cfg_disable_gpio(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg &= ~GNINT_STAT_GPIO;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

static int gn412x_gpio_request(struct gpio_chip *chip, unsigned int offset)
{
	int val;

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	struct device *dev = chip->dev;
#else
	struct device *dev = chip->parent;
#endif

	val = gn412x_gpio_reg_read(chip, GNGPIO_BYPASS_MODE, offset);
	if (val) {
		dev_err(dev, "%s GPIO %d is in BYPASS mode\n",
			chip->label, offset);
		return -EBUSY;
	}

	return 0;
}

static void gn412x_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	/* set it as input to avoid to drive anything */
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 1);
}

static int gn412x_gpio_get_direction(struct gpio_chip *chip,
				     unsigned int offset)
{
	return !!gn412x_gpio_reg_read(chip, GNGPIO_DIRECTION_MODE, offset);
}

static int gn412x_gpio_direction_input(struct gpio_chip *chip,
				       unsigned int offset)
{
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 1);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_ENABLE, offset, 0);

	return 0;
}

static int gn412x_gpio_direction_output(struct gpio_chip *chip,
					unsigned int offset, int value)
{
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 0);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_ENABLE, offset, 1);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_VALUE, offset, value);

	return 0;
}

static int gn412x_gpio_get(struct gpio_chip *chip,
			   unsigned int offset)
{
	return gn412x_gpio_reg_read(chip, GNGPIO_INPUT_VALUE, offset);
}

static void gn412x_gpio_set(struct gpio_chip *chip,
			    unsigned int offset, int value)
{
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_VALUE, offset, value);
}

/**
 * (disable)
 */
static void gn412x_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gn412x_gpio_dev *gn412x = to_gn412x_gpio_dev_gpio(gc);

	gn412x_iowrite32(gn412x, BIT(d->hwirq), GNGPIO_INT_MASK_SET);
}


/**
 * (enable)
 */
static void gn412x_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gn412x_gpio_dev *gn412x = to_gn412x_gpio_dev_gpio(gc);

	gn412x_iowrite32(gn412x, BIT(d->hwirq), GNGPIO_INT_MASK_CLR);
}


static int gn412x_gpio_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	struct device *dev = gc->dev;
#else
	struct device *dev = gc->parent;
#endif

	/*
	 * detect errors:
	 * - level and edge together cannot work
	 */
	if ((flow_type & IRQ_TYPE_LEVEL_MASK) &&
	    (flow_type & IRQ_TYPE_EDGE_BOTH)) {
		dev_err(dev,
			"Impossible to set GPIO IRQ %ld to both LEVEL and EDGE (0x%x)\n",
			d->hwirq, flow_type);
		return -EINVAL;
	}

	/* Configure: level or edge (default)? */
	if (flow_type & IRQ_TYPE_LEVEL_MASK) {
		gn412x_gpio_reg_write(gc, GNGPIO_INT_TYPE, d->hwirq, 1);
	} else {
		gn412x_gpio_reg_write(gc, GNGPIO_INT_TYPE, d->hwirq, 0);

		/* if we want to trigger on any edge */
		if ((flow_type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
			gn412x_gpio_reg_write(gc, GNGPIO_INT_ON_ANY,
					      d->hwirq, 1);
	}


	/* Configure: level-low or falling-edge, level-high or raising-edge */
	if (flow_type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		gn412x_gpio_reg_write(gc, GNGPIO_INT_VALUE, d->hwirq, 0);
	else
		gn412x_gpio_reg_write(gc, GNGPIO_INT_VALUE, d->hwirq, 1);

	return IRQ_SET_MASK_OK;
}


/**
 * A new IRQ interrupt has been requested
 * @d IRQ related data
 *
 * We need to set the GPIO line to be input and disable completely any
 * kind of output. We do not want any alternative function (bypass mode).
 */
static unsigned int gn412x_gpio_irq_startup(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);

	gn412x_gpio_reg_write(gc, GNGPIO_BYPASS_MODE, d->hwirq, 0);
	gn412x_gpio_reg_write(gc, GNGPIO_DIRECTION_MODE, d->hwirq, 1);
	gn412x_gpio_reg_write(gc, GNGPIO_OUTPUT_ENABLE, d->hwirq, 0);

	gn412x_gpio_direction_input(gc, d->hwirq);

	/* FIXME in the original code we had this? What is it? */
	/* !!(gennum_readl(spec, GNGPIO_INPUT_VALUE) & bit); */

	gn412x_gpio_irq_unmask(d);

	return 0;
}


/**
 * It disables the GPIO interrupt by masking it
 */
static void gn412x_gpio_irq_disable(struct irq_data *d)
{
	gn412x_gpio_irq_mask(d);
}

static void gn412x_gpio_irq_ack(struct irq_data *d)
{
	/*
	 * based on HW design,there is no need to ack HW
	 * before handle current irq. But this routine is
	 * necessary for handle_edge_irq
	 */
}

/**
 * This will run in hard-IRQ context since we do not have much to do
 */
static irqreturn_t spec_irq_sw_handler(int irq, void *arg)
{
	struct gn412x_gpio_dev *gn412x = arg;

	/* Ack the interrupts */
	gn412x_ioread32(gn412x, GNINT_STAT);
	gn412x_iowrite32(gn412x, 0x0000, GNINT_STAT);

	complete(&gn412x->compl);

	return IRQ_HANDLED;
}

/**
 * Handle IRQ from the GPIO block
 */
static irqreturn_t gn412x_gpio_irq_handler_t(int irq, void *arg)
{
	struct gn412x_gpio_dev *gn412x = arg;
	struct gpio_chip *gc = &gn412x->gpiochip;
	unsigned int cascade_irq;
	uint32_t gpio_int_status, gpio_int_type;
	unsigned long loop;
	irqreturn_t ret = IRQ_NONE;
	int i;

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	struct device *dev = gc->dev;
#else
	struct device *dev = gc->parent;
#endif

	gpio_int_status = gn412x_ioread32(gn412x, GNGPIO_INT_STATUS);
	gpio_int_status &= ~gn412x_ioread32(gn412x, GNGPIO_INT_MASK);
	if (!gpio_int_status)
		goto out_enable_irq;

	loop = gpio_int_status;
	for_each_set_bit(i, &loop, GN4124_GPIO_MAX) {
#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
		cascade_irq = irq_find_mapping(gc->irqdomain, i);
#else
		cascade_irq = irq_find_mapping(gc->irq.domain, i);
#endif
		dev_dbg(dev, "GPIO: %d, IRQ: %d\n", i, cascade_irq);
		/*
		 * Ok, now we execute the handler for the given IRQ. Please
		 * note that this is not the action requested by the device
		 * driver but it is the handler defined during the IRQ mapping
		 */
		handle_nested_irq(cascade_irq);
	}
	/*
	 * Level interrupts are cleared only when they are processed. This
	 * means that at this point (interrupts have been processed) the
	 * GPIO_INT_STATUS is still set because it is a sticky bit that got
	 * set again after our first read because we did not handle yet
	 * the IRQ. For this reason we have to clear it (defentivelly) by
	 * reading again the register ...
	 */
	gpio_int_status = gn412x_ioread32(gn412x, GNGPIO_INT_STATUS);
	/*
	 * ... but like this edge interrupts are lost. So write back
	 * in the register the status of all pending edge interrupts.
	 * (NOTE: not yet prooven that it works)
	 */
	gpio_int_type = gn412x_ioread32(gn412x, GNGPIO_INT_TYPE);
	gn412x_iowrite32(gn412x, (gpio_int_status & (~gpio_int_type)),
			 GNGPIO_INT_STATUS);
	ret = IRQ_HANDLED;

out_enable_irq:
	/* Re-enable the GPIO interrupts, we are done here */
	gn412x_gpio_int_cfg_enable_gpio(gn412x);

	return ret;
}

static bool gn412x_gpio_fpga_is_programmed(struct gn412x_gpio_dev *gn412x)
{
	bool done;

	done = gn412x_ioread32(gn412x, FCL_STATUS) & FCL_SPRI_DONE;

	return done;
}

static irqreturn_t gn412x_gpio_irq_handler_h(int irq, void *arg)
{
	struct gn412x_gpio_dev *gn412x = arg;
	uint32_t int_stat, int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_stat = gn412x_ioread32(gn412x, GNINT_STAT);
	if (unlikely(!(int_stat & int_cfg)))
		return IRQ_NONE;

	if (unlikely(int_stat & GNINT_STAT_SW_ALL)) /* only for testing */
		return spec_irq_sw_handler(irq, gn412x);

	if (unlikely(int_stat & GNINT_STAT_ERR_ALL)) {
		gn412x_iowrite32(gn412x, int_stat & GNINT_STAT_ERR_ALL,
						 GNINT_STAT);
		if (!gn412x_gpio_fpga_is_programmed(gn412x))
			return IRQ_NONE;  /* spurious interrupt if FPGA is not programmed */
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
		dev_err(gn412x->gpiochip.dev, "Local bus error 0x%08x\n", int_stat);
#else
		dev_err(gn412x->gpiochip.parent, "Local bus error 0x%08x\n", int_stat);
#endif

		return IRQ_HANDLED;
	}

	/*
	 * Do not listen to new interrupts while handling the current GPIOs.
	 * This may take a while since the chain behind each GPIO can be long.
	 * If the IRQ behind is level, we do not want this IRQ handeler to be
	 * called continuously. But on the other hand we do not want other
	 * devices sharing the same IRQ to wait for us; just to play safe,
	 * let's disable interrupts. Within the thread we will re-enable them
	 * when we are ready (like IRQF_ONESHOT).
	 *
	 * We keep the error interrupts enabled
	 */
	gn412x_gpio_int_cfg_disable_gpio(gn412x);

	return IRQ_WAKE_THREAD;
}


static void gn412x_gpio_irq_set_nested_thread(struct gn412x_gpio_dev *gn412x,
					      unsigned int gpio, bool nest)
{
	int irq;

#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
	irq = irq_find_mapping(gn412x->gpiochip.irqdomain, gpio);
#else
	irq = irq_find_mapping(gn412x->gpiochip.irq.domain, gpio);
#endif
	irq_set_nested_thread(irq, nest);
}

static void gn412x_gpio_irq_set_nested_thread_all(struct gn412x_gpio_dev *gn412x,
						  bool nest)
{
	int i;

	for (i = 0; i < GN4124_GPIO_MAX; ++i)
		gn412x_gpio_irq_set_nested_thread(gn412x, i, nest);
}

static int gn412x_gpio_probe(struct platform_device *pdev)
{
	struct gn412x_gpio_dev *gn412x;
	struct resource *r;
	int err;

	gn412x = kzalloc(sizeof(*gn412x), GFP_KERNEL);
	if (!gn412x)
		return -ENOMEM;

	platform_set_drvdata(pdev, gn412x);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "Missing memory resource\n");
		err = -EINVAL;
		goto err_res_mem;
	}
	gn412x->mem = ioremap(r->start, resource_size(r));
	if (!gn412x->mem) {
		err = -EADDRNOTAVAIL;
		goto err_map;
	}
	gn412x->pdata = dev_get_platdata(&pdev->dev);
	if (!gn412x->pdata) {
		dev_warn(&pdev->dev, "Missing platform data, use default\n");
		gn412x->pdata = &gn412x_gpio_pdata_default;
	}
	gn412x_iowrite32(gn412x, 0, GNGPIO_BYPASS_MODE);

	gn412x_gpio_int_cfg_disable_err(gn412x);
	gn412x_gpio_int_cfg_disable_gpio(gn412x);
	gn412x_iowrite32(gn412x, 0xFFFF, GNGPIO_INT_MASK_SET);

	gn412x->irqchip.name = "GN412X-GPIO",
	gn412x->irqchip.irq_startup = gn412x_gpio_irq_startup,
	gn412x->irqchip.irq_disable = gn412x_gpio_irq_disable,
	gn412x->irqchip.irq_mask = gn412x_gpio_irq_mask,
	gn412x->irqchip.irq_unmask = gn412x_gpio_irq_unmask,
	gn412x->irqchip.irq_set_type = gn412x_gpio_irq_set_type,
	gn412x->irqchip.irq_ack = gn412x_gpio_irq_ack,

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	gn412x->gpiochip.dev = &pdev->dev;
#else
	gn412x->gpiochip.parent = &pdev->dev;
#endif
	gn412x->gpiochip.label = "gn412x-gpio";
	gn412x->gpiochip.owner = THIS_MODULE;
	gn412x->gpiochip.request = gn412x_gpio_request;
	gn412x->gpiochip.free = gn412x_gpio_free;
	gn412x->gpiochip.get_direction = gn412x_gpio_get_direction;
	gn412x->gpiochip.direction_input = gn412x_gpio_direction_input;
	gn412x->gpiochip.direction_output = gn412x_gpio_direction_output;
	gn412x->gpiochip.get = gn412x_gpio_get;
	gn412x->gpiochip.set = gn412x_gpio_set;
	gn412x->gpiochip.base = -1;
	gn412x->gpiochip.ngpio = GN4124_GPIO_MAX;
	gn412x->gpiochip.can_sleep = 0;
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	gn412x->gpiochip.irq.chip = &gn412x->irqchip;
	gn412x->gpiochip.irq.first = 0;
	gn412x->gpiochip.irq.handler = handle_simple_irq;
	gn412x->gpiochip.irq.default_type = IRQ_TYPE_NONE;
	gn412x->gpiochip.irq.threaded = true;
#endif
	err = gpiochip_add(&gn412x->gpiochip);
	if (err)
		goto err_add;

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	err = gpiochip_irqchip_add(&gn412x->gpiochip,
				   &gn412x->irqchip,
				   0, handle_simple_irq,
				   IRQ_TYPE_NONE);
	if (err)
		goto err_add_irq;
#endif

	gn412x_gpio_irq_set_nested_thread_all(gn412x, true);

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
	err = request_threaded_irq(platform_get_irq(pdev, 0),
				   gn412x_gpio_irq_handler_h,
				   gn412x_gpio_irq_handler_t,
				   IRQF_SHARED,
				   dev_name(gn412x->gpiochip.dev),
				   gn412x);
	if (err) {
		dev_err(gn412x->gpiochip.dev, "Can't request IRQ %d (%d)\n",
#else
	err = request_threaded_irq(platform_get_irq(pdev, 0),
				   gn412x_gpio_irq_handler_h,
				   gn412x_gpio_irq_handler_t,
				   IRQF_SHARED,
				   dev_name(gn412x->gpiochip.parent),
				   gn412x);
	if (err) {
		dev_err(gn412x->gpiochip.parent, "Can't request IRQ %d (%d)\n",
#endif
			platform_get_irq(pdev, 0), err);
		goto err_req;
	}

	gn412x_gpio_int_cfg_enable_err(gn412x);
	gn412x_gpio_int_cfg_enable_gpio(gn412x);

	gn412x_dbg_init(gn412x);

	return 0;

err_req:
	gn412x_gpio_irq_set_nested_thread_all(gn412x, false);
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
err_add_irq:
	gpiochip_remove(&gn412x->gpiochip);
#endif
err_add:
	iounmap(gn412x->mem);
err_map:
err_res_mem:
	kfree(gn412x);
	return err;
}

static int gn412x_gpio_remove(struct platform_device *pdev)
{
	struct gn412x_gpio_dev *gn412x = platform_get_drvdata(pdev);


	gn412x_dbg_exit(gn412x);

	gn412x_gpio_int_cfg_disable_gpio(gn412x);
	gn412x_gpio_int_cfg_disable_err(gn412x);


	free_irq(platform_get_irq(pdev, 0), gn412x);
	gn412x_gpio_irq_set_nested_thread_all(gn412x, false);
	gpiochip_remove(&gn412x->gpiochip);
	iounmap(gn412x->mem);
	kfree(gn412x);
	dev_dbg(&pdev->dev, "%s\n", __func__);

	return 0;
}

static const struct platform_device_id gn412x_gpio_id[] = {
	{
		.name = "gn412x-gpio",
	},
	{}, /* last */
};

static struct platform_driver gn412x_gpio_platform_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
	},
	.probe = gn412x_gpio_probe,
	.remove = gn412x_gpio_remove,
	.id_table = gn412x_gpio_id,
};

module_platform_driver(gn412x_gpio_platform_driver);

MODULE_AUTHOR("Federico Vaga <federico.vaga@cern.ch>");
MODULE_DESCRIPTION("GN412X GPIO driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DEVICE_TABLE(platform, gn412x_gpio_id);
