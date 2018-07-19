/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Driver for SPEC (Simple PCI FMC carrier) board.
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include "spec.h"

#define CHAIN 0

#define GN4124_GPIO_IRQ_MAX 16

static int spec_use_msi = 0;
module_param_named(use_msi, spec_use_msi, int, 0444);

static int spec_test_irq = 1;
module_param_named(test_irq, spec_test_irq, int, 0444);

/**
 * This bitmask describes the GPIO which can be used as interrupt lines.
 * By default SPEC uses GPIO8 and GPIO9
 */
static int spec_gpio_int = 0x00000300;

static int spec_irq_dbg_info(struct seq_file *s, void *offset)
{
	struct spec_dev *spec = s->private;
	int i;

	seq_printf(s, "'%s':\n",dev_name(&spec->pdev->dev));

	seq_printf(s, "  redirect: %d\n", spec->pdev->irq);
	seq_printf(s, "  irq-mapping:\n");
	for (i = 0; i < GN4124_GPIO_IRQ_MAX; ++i) {
		seq_printf(s, "    - hardware: %d\n", i);
		seq_printf(s, "      linux: %d\n",
			   irq_find_mapping(spec->gpio_domain, i));
	}

	return 0;
}

static int spec_irq_dbg_info_open(struct inode *inode, struct file *file)
{
	struct spec_dev *spec = inode->i_private;

	return single_open(file, spec_irq_dbg_info, spec);
}

static const struct file_operations spec_irq_dbg_info_ops = {
	.owner = THIS_MODULE,
	.open  = spec_irq_dbg_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


/**
 * It initializes the debugfs interface
 * @spec: SPEC instance
 *
 * Return: 0 on success, otherwise a negative error number
 */
static int spec_irq_debug_init(struct spec_dev *spec)
{
	spec->dbg_dir = debugfs_create_dir(dev_name(&spec->pdev->dev), NULL);
	if (IS_ERR_OR_NULL(spec->dbg_dir)) {
		dev_err(&spec->pdev->dev,
			"Cannot create debugfs directory (%ld)\n",
			PTR_ERR(spec->dbg_dir));
		return PTR_ERR(spec->dbg_dir);
	}

	spec->dbg_info = debugfs_create_file(SPEC_DBG_INFO_NAME, 0444,
					     spec->dbg_dir, spec,
					     &spec_irq_dbg_info_ops);
	if (IS_ERR_OR_NULL(spec->dbg_info)) {
		dev_err(&spec->pdev->dev,
			"Cannot create debugfs file \"%s\" (%ld)\n",
			SPEC_DBG_INFO_NAME, PTR_ERR(spec->dbg_info));
		return PTR_ERR(spec->dbg_info);
	}

	return 0;
}


/**
 * It removes the debugfs interface
 * @spec: SPEC instance
 */
static void spec_irq_debug_exit(struct spec_dev *spec)
{
	if (spec->dbg_dir)
		debugfs_remove_recursive(spec->dbg_dir);
}


/**
 * (disable)
 */
static void spec_irq_gpio_mask(struct irq_data *d)
{
	struct spec_dev *spec = irq_data_get_irq_chip_data(d);

	gennum_writel(spec, BIT(d->hwirq), GNGPIO_INT_MASK_SET);
}


/**
 * (enable)
 */
static void spec_irq_gpio_unmask(struct irq_data *d)
{
	struct spec_dev *spec = irq_data_get_irq_chip_data(d);

	gennum_writel(spec, BIT(d->hwirq), GNGPIO_INT_MASK_CLR);
}


static int spec_irq_gpio_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct spec_dev *spec = irq_data_get_irq_chip_data(d);
	int bit;

	/*
	 * detect errors:
	 * - level and edge together cannot work
	 */
	if ((flow_type & IRQ_TYPE_LEVEL_MASK) &&
	    (flow_type & IRQ_TYPE_EDGE_BOTH)) {
		dev_err(&spec->pdev->dev, "Impossible to set GPIO IRQ %ld to both LEVEL and EDGE (0x%x)\n",
			d->hwirq, flow_type);
		return -EINVAL;
	}

	bit = BIT(d->hwirq);
	/* Configure: level or edge (default)? */
	if (flow_type & IRQ_TYPE_LEVEL_MASK) {
		gennum_mask_val(spec, bit, bit, GNGPIO_INT_TYPE);
#if CHAIN
		irq_set_handler(d->irq, handle_level_irq);
#endif
	} else {
		gennum_mask_val(spec, bit, 0, GNGPIO_INT_TYPE);

		/* if we want to trigger on any edge */
		if ((flow_type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
			gennum_mask_val(spec, bit, 0, GNGPIO_INT_ON_ANY);
#if CHAIN
		irq_set_handler(d->irq, handle_edge_irq);
#endif
	}


	/* Configure: level-low or falling-edge, level-high or raising-edge (default)? */
	if (flow_type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		gennum_mask_val(spec, bit, 0, GNGPIO_INT_VALUE);
	else
		gennum_mask_val(spec, bit, bit, GNGPIO_INT_VALUE);

	return IRQ_SET_MASK_OK;
}


/**
 * A new IRQ interrupt has been requested
 * @d IRQ related data
 *
 * We need to set the GPIO line to be input and disable completely any
 * kind of output. We do not want any alternative function (bypass mode).
 */
static unsigned int spec_irq_gpio_startup(struct irq_data *d)
{
	struct spec_dev *spec = irq_data_get_irq_chip_data(d);
	unsigned int bit = BIT(d->hwirq);

	gennum_mask_val(spec, bit, 0, GNGPIO_BYPASS_MODE);
	gennum_mask_val(spec, bit, bit, GNGPIO_DIRECTION_MODE);
	gennum_mask_val(spec, bit, 0, GNGPIO_OUTPUT_ENABLE);
	/* FIXME in the original code we had this? What is it? */
	/* !!(gennum_readl(spec, GNGPIO_INPUT_VALUE) & bit); */

	spec_irq_gpio_unmask(d);

	return 0;
}


/**
 * It disables the GPIO interrupt by masking it
 */
static void spec_irq_gpio_disable(struct irq_data *d)
{
	spec_irq_gpio_mask(d);
}


static struct irq_chip spec_irq_gpio_chip = {
	.name = "GN4124-GPIO",
	.irq_startup = spec_irq_gpio_startup,
	.irq_disable = spec_irq_gpio_disable,
	.irq_mask = spec_irq_gpio_mask,
	.irq_unmask = spec_irq_gpio_unmask,
	.irq_set_type = spec_irq_gpio_set_type,
};


/**
 * It match a given device with the irq_domain. `struct device_node *` is just
 * a convention. actually it can be anything (I do not understand why kernel
 * people did not use `void *`)
 *
 * In our case here we expect a string because we identify this domain by
 * name
 */
static int spec_irq_gpio_domain_match(struct irq_domain *d, struct device_node *node)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	char *name = (char *)node;

	if (strcmp(d->name, name) == 0)
		return 1;
#endif
	return 0;
}


/**
 * Given the hardware IRQ and the Linux IRQ number (virtirq), configure the
 * Linux IRQ number in order to handle properly the incoming interrupts
 * on the hardware IRQ line.
 */
static int spec_irq_gpio_domain_map(struct irq_domain *h,
				    unsigned int virtirq,
				    irq_hw_number_t hwirq)
{
	struct spec_dev *spec = h->host_data;

	irq_set_chip_data(virtirq, spec);
	irq_set_chip(virtirq, &spec_irq_gpio_chip);

	/* all handlers are directly nested to our handler */
	irq_set_nested_thread(virtirq, 1);

	return 0;
}


static struct irq_domain_ops spec_irq_gpio_domain_ops = {
	.match = spec_irq_gpio_domain_match,
	.map = spec_irq_gpio_domain_map,
};


/**
 * Handle IRQ from the GPIO block
 */
static irqreturn_t spec_irq_gpio_handler(int irq, struct spec_dev *spec)
{
	struct irq_desc *desc;
	unsigned int cascade_irq;
	uint32_t gpio_int_status;
	int i;

	gennum_readl(spec, GNGPIO_INPUT_VALUE);
	gpio_int_status = gennum_readl(spec, GNGPIO_INT_STATUS);
	if (!gpio_int_status)
		return IRQ_NONE;

	for (i = 0; i < GN4124_GPIO_IRQ_MAX; ++i) {
		if (!(gpio_int_status & BIT(i)))
			continue; /* no interrupt on GPIO 'i'*/

		cascade_irq = irq_find_mapping(spec->gpio_domain, i);
		desc = irq_to_desc(cascade_irq);
		/*
		 * Ok, now we execute the handler for the given IRQ. Please
		 * note that this is not the action requested by the device driver
		 * but it is the handler defined during the IRQ mapping
		 */
		handle_nested_irq(cascade_irq);
	}

	return IRQ_HANDLED;
}

static irqreturn_t spec_irq_sw_handler(int irq, struct spec_dev *spec, uint32_t int_stat)
{
	/* Ack the interrupts */
	gennum_writel(spec, 0x0000, GNINT_STAT);

	complete(&spec->compl);

	return IRQ_HANDLED;
}

#if CHAIN
static void spec_irq_chain_handler(unsigned int irq, struct irq_desc *desc)
{
}
#endif

/**
 * This is the place to re-route interrupts to the proper handler
 */
static irqreturn_t spec_irq_handler_threaded(int irq, void *arg)
{
	struct spec_dev *spec = arg;
	uint32_t int_stat, int_cfg;
	irqreturn_t ret;

	int_cfg = gennum_readl(spec, GNINT_CFG(0));
	int_stat = gennum_readl(spec, GNINT_STAT);
	if (unlikely(!(int_stat & int_cfg)))
		return IRQ_NONE;

	if (int_stat & GNINT_STAT_GPIO)
		ret = spec_irq_gpio_handler(irq, spec);
	if (int_stat & GNINT_STAT_SW_ALL)
		ret = spec_irq_sw_handler(irq, spec, int_stat);

	return IRQ_HANDLED;
}

/**
 * Configure GPIO interrupts
 * @spec SPEC instance
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int spec_irq_gpio_init(struct spec_dev *spec)
{
	int i, irq;

	/* Disable eery possible GPIO interrupt */
	gennum_writel(spec, 0xFFFF, GNGPIO_INT_MASK_SET);

	spec->gpio_domain = irq_domain_add_linear(NULL, GN4124_GPIO_IRQ_MAX,
						  &spec_irq_gpio_domain_ops,
						  spec);
	if (!spec->gpio_domain)
		return -ENOMEM;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	spec->gpio_domain->name = kasprintf(GFP_KERNEL, "%s-gn4124-gpio-irq",
					    dev_name(&spec->pdev->dev));
#endif


	/*
	 * Create the mapping between HW irq and virtual IRQ number. On SPEC
	 * we have recuded set of GPIOs which can be used as interrupt:
	 * activate only these ones
	 */
	for (i = 0; i < GN4124_GPIO_IRQ_MAX; ++i) {
		if (!(spec_gpio_int & BIT(i)))
			continue;
		irq = irq_create_mapping(spec->gpio_domain, i);
		if (irq <= 0)
			goto err;
	}

	gennum_mask_val(spec, GNINT_STAT_GPIO, GNINT_STAT_GPIO, GNINT_CFG(0));

	return 0;
err:
	irq_domain_remove(spec->gpio_domain);
	return irq;
}


/**
 * Disable GPIO IRQ
 * @spec SPEC instance
 */
static void spec_irq_gpio_exit(struct spec_dev *spec)
{
	gennum_mask_val(spec, GNINT_STAT_GPIO, 0, GNINT_CFG(0));
	gennum_writel(spec, 0xFFFF, GNGPIO_INT_MASK_SET);
	gennum_readl(spec, GNINT_STAT); /* ack any pending GPIO interrupt */
	irq_domain_remove(spec->gpio_domain);
}


/**
 * Configure software interrupts
 * @spec SPEC instance
 *
 * This kind on interrupt is used only for testing purpose
 *
 * Return: 0 on success, otherwise a negative error code
 */
static int spec_irq_sw_init(struct spec_dev *spec)
{
	gennum_mask_val(spec, GNINT_STAT_SW_ALL, GNINT_STAT_SW_ALL,
			GNINT_CFG(0));

	return 0;
}


/**
 * Disable software IRQ
 * @spec SPEC instance
 */
static void spec_irq_sw_exit(struct spec_dev *spec)
{
	gennum_mask_val(spec, GNINT_STAT_SW_ALL, 0, GNINT_CFG(0));
}


static int spec_irq_sw_test(struct spec_dev *spec)
{
	long ret;

	if (!spec_test_irq)
		return 0;

	/* produce a software interrupt on SW1 and wait for its completion */
	init_completion(&spec->compl);
	gennum_writel(spec, 0x0008, GNINT_STAT);
	ret = wait_for_completion_timeout(&spec->compl,
					  msecs_to_jiffies(10000));
	if (ret == 0) {
		gennum_writel(spec, 0x0000, GNINT_STAT); /* disable */
		dev_err(&spec->pdev->dev, "Cannot receive interrupts\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * Initialize interrupts
 * @spec SPEC instance
 *
 * Return: 0 on success, otherwise a negative error number
 */
int spec_irq_init(struct spec_dev *spec)
{
	int err;
	int i;

	if (!spec)
		return -EINVAL;

	/* disable all source of interrupts */
	for (i = 0; i < 7; i++)
		gennum_writel(spec, 0, GNINT_CFG(i));

	err = spec_irq_gpio_init(spec);
	if (err)
		goto err_gpio;
	err = spec_irq_sw_init(spec);
	if (err)
		goto err_sw;

#if CHAIN
	irq_set_chained_handler(spec->pdev->irq, spec_irq_chain_handler);
	irq_set_handler_data(spec->pdev->irq, spec);
#else
	/*
	 * It depends on the platform and on the IRQ on which we are connecting to
	 * but most likely our interrupt handler will be a thread.
	 *
	 * We need the ONESHOT option because we do not want to receive interrupts
	 * until we finish with our handler
	 */
	err = request_threaded_irq(spec->pdev->irq, NULL,
				   spec_irq_handler_threaded,
				   IRQF_SHARED | IRQF_ONESHOT,
				   dev_name(&spec->pdev->dev),
				   spec);
	if (err) {
		dev_err(&spec->pdev->dev, "Can't request IRQ %d (%d)\n",
			spec->pdev->irq, err);
		goto err_req;
	}
#endif
	spec_irq_debug_init(spec);

	err = spec_irq_sw_test(spec);
	if (err)
		goto err_test;

	return 0;

err_test:
	spec_irq_debug_exit(spec);
	free_irq(spec->pdev->irq, spec);
err_req:
	spec_irq_sw_exit(spec);
err_sw:
	spec_irq_gpio_exit(spec);
err_gpio:
	return err;
}

void spec_irq_exit(struct spec_dev *spec)
{
	int i;

	if (!spec)
		return;

	/* disable all source of interrupts */
	for (i = 0; i < 7; i++)
		gennum_writel(spec, 0, GNINT_CFG(i));
	spec_irq_debug_exit(spec);
	free_irq(spec->pdev->irq, spec);
	spec_irq_sw_exit(spec);
	spec_irq_gpio_exit(spec);
}
