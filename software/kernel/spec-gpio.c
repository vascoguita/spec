// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include "spec.h"
#include "spec-compat.h"

void spec_gpio_fpga_select_set(struct spec_gn412x *spec_gn412x,
			       enum spec_fpga_select sel)
{
	switch (sel) {
	case SPEC_FPGA_SELECT_FPGA_FLASH:
	case SPEC_FPGA_SELECT_GN4124_FPGA:
	case SPEC_FPGA_SELECT_GN4124_FLASH:
		gpiod_set_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0],
				!!(sel & 0x1));
		gpiod_set_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1],
				!!(sel & 0x2));
		break;
	default:
		break;
	}
}

enum spec_fpga_select spec_gpio_fpga_select_get(struct spec_gn412x *spec_gn412x)
{
	enum spec_fpga_select sel = 0;

	sel |= !!gpiod_get_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1]) << 1;
	sel |= !!gpiod_get_value(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]) << 0;

	return sel;
}


static const struct gpiod_lookup_table spec_gpiod_table = {
	.table = {
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_BOOTSEL0,
				"bootsel", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_BOOTSEL1,
				"bootsel", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SPRI_DIN,
				"spi", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SPRI_FLASH_CS,
				"spi", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_IRQ0,
				"irq", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_IRQ1,
				"irq", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SCL,
				"i2c", 0,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		GPIO_LOOKUP_IDX("gn412x-gpio", GN4124_GPIO_SDA,
				"i2c", 1,
				GPIO_ACTIVE_HIGH | GPIO_PERSISTENT),
		{},
	}
};

static inline size_t spec_gpiod_table_size(void)
{
	return sizeof(struct gpiod_lookup_table) +
	       (sizeof(struct gpiod_lookup) * 9);
}

int spec_gpio_init(struct spec_gn412x *spec_gn412x)
{
	struct gpio_desc *gpiod;
	struct gpiod_lookup_table *lookup;
	int err = 0;

	lookup = devm_kzalloc(&spec_gn412x->pdev->dev,
			      spec_gpiod_table_size(),
			      GFP_KERNEL);
	if (!lookup) {
		err = -ENOMEM;
		goto err_alloc;
	}

	memcpy(lookup, &spec_gpiod_table, spec_gpiod_table_size());

	lookup->dev_id = kstrdup(dev_name(&spec_gn412x->pdev->dev), GFP_KERNEL);
	if (!lookup->dev_id)
		goto err_dup;

	spec_gn412x->gpiod_table = lookup;
	err = compat_gpiod_add_lookup_table(spec_gn412x->gpiod_table);
	if (err)
		goto err_lookup;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "bootsel", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel0;
	}
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0] = gpiod;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "bootsel", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_sel1;
	}
	spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1] = gpiod;

	/* Because of a BUG in RedHat kernel 3.10 we re-set direction */
	err = gpiod_direction_output(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0], 1);
	if (err)
		goto err_out0;
	err = gpiod_direction_output(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1], 1);
	if (err)
		goto err_out1;


	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "irq", 0, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_irq0;
	}
	spec_gn412x->gpiod[GN4124_GPIO_IRQ0] = gpiod;

	gpiod = gpiod_get_index(&spec_gn412x->pdev->dev, "irq", 1, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		err = PTR_ERR(gpiod);
		goto err_irq1;
	}
	spec_gn412x->gpiod[GN4124_GPIO_IRQ1] = gpiod;

	/* Because of a BUG in RedHat kernel 3.10 we re-set direction */
	err = gpiod_direction_input(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]);
	if (err)
		goto err_in0;
	err = gpiod_direction_input(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]);
	if (err)
		goto err_in1;

	return 0;

err_in1:
err_in0:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]);
err_irq1:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]);
err_irq0:
err_out1:
err_out0:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1]);
err_sel1:
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]);
err_sel0:
	gpiod_remove_lookup_table(spec_gn412x->gpiod_table);
err_lookup:
	kfree(lookup->dev_id);
err_dup:
	devm_kfree(&spec_gn412x->pdev->dev, lookup);
	spec_gn412x->gpiod_table = NULL;
err_alloc:

	return err;
}

void spec_gpio_exit(struct spec_gn412x *spec_gn412x)
{
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ1]);
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_IRQ0]);
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL1]);
	gpiod_put(spec_gn412x->gpiod[GN4124_GPIO_BOOTSEL0]);
	gpiod_remove_lookup_table(spec_gn412x->gpiod_table);
	kfree(spec_gn412x->gpiod_table->dev_id);
}
