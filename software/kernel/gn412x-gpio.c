// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/module.h>
#include <linux/gpio/driver.h>

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

	struct completion	compl;
	struct gn412x_platform_data *pdata;
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

static uint32_t gn412x_ioread32(struct gn412x_gpio_dev *gn412x, int reg)
{
	return ioread32(gn412x->mem + reg);
}

static void gn412x_iowrite32(struct gn412x_gpio_dev *gn412x, uint32_t val, int reg)
{
	return iowrite32(val, gn412x->mem + reg);
}


static int gn412x_gpio_reg_read(struct gpio_chip *chip,
				int reg, unsigned offset)
{
	struct gn412x_gpio_dev *gn412x = to_gn412x_gpio_dev_gpio(chip);

	return gn412x_ioread32(gn412x, reg) & BIT(offset);
}

static void gn412x_gpio_reg_write(struct gpio_chip *chip,
				  int reg, unsigned offset, int value)
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
 * Enable GPIO interrupts
 * @gn412x gn412x device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static void gn412x_gpio_int_cfg_enable(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg |= GNINT_STAT_GPIO;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

/**
 * Disable GPIO interrupts from a single configuration space
 * @gn412x gn412x device
 *
 * Return: 0 on success, otherwise a negative error number
 */
static void gn412x_gpio_int_cfg_disable(struct gn412x_gpio_dev *gn412x)
{
	uint32_t int_cfg;

	int_cfg = gn412x_ioread32(gn412x, GNINT_CFG(gn412x->pdata->int_cfg));
	int_cfg &= ~GNINT_STAT_GPIO;
	gn412x_iowrite32(gn412x, int_cfg, GNINT_CFG(gn412x->pdata->int_cfg));
}

static int gn412x_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	int val;

	val = gn412x_gpio_reg_read(chip, GNGPIO_BYPASS_MODE, offset);
	if (val) {
		dev_err(chip->dev, "%s GPIO %d is in BYPASS mode\n",
			chip->label, offset);
		return -EBUSY;
	}

	return 0;
}

static void gn412x_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	/* set it as input to avoid to drive anything */
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 1);
}

static int gn412x_gpio_get_direction(struct gpio_chip *chip,
				     unsigned offset)
{
	return !gn412x_gpio_reg_read(chip, GNGPIO_DIRECTION_MODE, offset);
}

static int gn412x_gpio_direction_input(struct gpio_chip *chip,
				       unsigned offset)
{
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 1);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_ENABLE, offset, 0);

	return 0;
}

static int gn412x_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	gn412x_gpio_reg_write(chip, GNGPIO_DIRECTION_MODE, offset, 0);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_ENABLE, offset, 1);
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_VALUE, offset, value);

	return 0;
}

static int gn412x_gpio_get(struct gpio_chip *chip,
			   unsigned offset)
{
	return gn412x_gpio_reg_read(chip, GNGPIO_INPUT_VALUE, offset);
}

static void gn412x_gpio_set(struct gpio_chip *chip,
			    unsigned offset, int value)
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

	/*
	 * detect errors:
	 * - level and edge together cannot work
	 */
	if ((flow_type & IRQ_TYPE_LEVEL_MASK) &&
	    (flow_type & IRQ_TYPE_EDGE_BOTH)) {
		dev_err(gc->dev,
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


	/* Configure: level-low or falling-edge, level-high or raising-edge (default)? */
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

static struct irq_chip gn412x_gpio_irq_chip = {
	.name = "GN412X-GPIO",
	.irq_startup = gn412x_gpio_irq_startup,
	.irq_disable = gn412x_gpio_irq_disable,
	.irq_mask = gn412x_gpio_irq_mask,
	.irq_unmask = gn412x_gpio_irq_unmask,
	.irq_set_type = gn412x_gpio_irq_set_type,
	.irq_ack = gn412x_gpio_irq_ack,
};

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
	uint32_t gpio_int_status;
	unsigned long loop;
	irqreturn_t ret = IRQ_NONE;
	int i;

	gpio_int_status = gn412x_ioread32(gn412x, GNGPIO_INT_STATUS);
	if (!gpio_int_status)
		goto out_enable_irq;

	loop = gpio_int_status;
	for_each_set_bit(i, &loop, GN4124_GPIO_MAX) {
		cascade_irq = irq_find_mapping(gc->irqdomain, i);
		/*
		 * Ok, now we execute the handler for the given IRQ. Please
		 * note that this is not the action requested by the device driver
		 * but it is the handler defined during the IRQ mapping
		 */
		handle_nested_irq(cascade_irq);
	}
	ret = IRQ_HANDLED;

out_enable_irq:
	/* Re-enable the GPIO interrupts, we are done here */
	gn412x_gpio_int_cfg_enable(gn412x);

	return ret;
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

	/*
	 * Do not listen to new interrupts while handling the current GPIOs.
	 * This may take a while since the chain behind each GPIO can be long.
	 * If the IRQ behind is level, we do not want this IRQ handeler to be
	 * called continuously. But on the other hand we do not want other
	 * devices sharing the same IRQ to wait for us; just to play safe,
	 * let's disable interrupts. Within the thread we will re-enable them
	 * when we are ready (like IRQF_ONESHOT).
	 */
	gn412x_gpio_int_cfg_disable(gn412x);

	return IRQ_WAKE_THREAD;
}


static void gn412x_gpio_irq_set_nested_thread(struct gn412x_gpio_dev *gn412x,
					      unsigned int gpio, bool nest)
{
	int irq;

	irq= irq_find_mapping(gn412x->gpiochip.irqdomain, gpio);
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
	gn412x->pdata = dev_get_platdata(&pdev->dev);
	if (!gn412x->pdata) {
		dev_warn(&pdev->dev, "Missing platform data, use default\n");
		gn412x->pdata = &gn412x_gpio_pdata_default;
	}
	gn412x->gpiochip.dev = &pdev->dev;
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

	err = gpiochip_add(&gn412x->gpiochip);
	if (err)
		goto err_add;

	gn412x_gpio_int_cfg_disable(gn412x);
	gn412x_iowrite32(gn412x, 0xFFFF, GNGPIO_INT_MASK_SET);
	err = gpiochip_irqchip_add(&gn412x->gpiochip,
				   &gn412x_gpio_irq_chip,
				   0, handle_simple_irq,
				   IRQ_TYPE_NONE);
	if (err)
		goto err_add_irq;

	gn412x_gpio_irq_set_nested_thread_all(gn412x, true);

	err = request_threaded_irq(platform_get_irq(pdev, 0),
				   gn412x_gpio_irq_handler_h,
				   gn412x_gpio_irq_handler_t,
				   IRQF_SHARED,
				   dev_name(gn412x->gpiochip.dev),
				   gn412x);
	if (err) {
		dev_err(gn412x->gpiochip.dev, "Can't request IRQ %d (%d)\n",
			platform_get_irq(pdev, 0), err);
		goto err_req;
	}

	gn412x_gpio_int_cfg_enable(gn412x);

	return 0;

err_req:
	gn412x_gpio_irq_set_nested_thread_all(gn412x, false);
err_add_irq:
	gpiochip_remove(&gn412x->gpiochip);
err_add:
	devm_iounmap(&pdev->dev, gn412x->mem);
err_map:
err_res_mem:
	devm_kfree(&pdev->dev, gn412x);
	return err;
}

static int gn412x_gpio_remove(struct platform_device *pdev)
{
	struct gn412x_gpio_dev *gn412x = platform_get_drvdata(pdev);

	gn412x_gpio_int_cfg_disable(gn412x);

	free_irq(platform_get_irq(pdev, 0), gn412x);
	gn412x_gpio_irq_set_nested_thread_all(gn412x, false);
	gpiochip_remove(&gn412x->gpiochip);

	return 0;
}

static const struct platform_device_id gn412x_gpio_id[] = {
	{
		.name = "gn412x-gpio",
	},
	{ .name = "" }, /* last */
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
