// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include "spec.h"
#include "spec-compat.h"

void spec_gpio_fpga_select(struct spec_dev *spec, enum spec_fpga_select sel)
{
	switch (sel) {
	case SPEC_FPGA_SELECT_FLASH:
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL0], 1);
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL1], 1);
		break;
	case SPEC_FPGA_SELECT_GN4124:
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL0], 1);
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL1], 0);
		break;
	case SPEC_FPGA_SELECT_SPI:
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL0], 0);
		gpiod_set_value(spec->gpiod[GN4124_GPIO_BOOTSEL1], 0);
		break;
	default:
		break;
	}
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
	return sizeof(struct gpiod_lookup_table) + (sizeof(struct gpiod_lookup) * 9);
}

int spec_gpio_init(struct spec_dev *spec)
{
	struct gpiod_lookup_table *lookup;
	int err;

	lookup = devm_kzalloc(&spec->dev,
			      spec_gpiod_table_size(),
			      GFP_KERNEL);
	if (!lookup)
		return -ENOMEM;

	memcpy(lookup, &spec_gpiod_table, spec_gpiod_table_size());

	lookup->dev_id = kstrdup(dev_name(&spec->dev), GFP_KERNEL);
	if (!lookup->dev_id)
		goto err_dup;

	spec->gpiod_table = lookup;
	err = compat_gpiod_add_lookup_table(spec->gpiod_table);
	if (err)
		goto err_lookup;

	spec->gpiod[GN4124_GPIO_BOOTSEL0] = gpiod_get_index(&spec->dev,
								 "bootsel", 0,
								 GPIOD_OUT_HIGH);
	if (IS_ERR(spec->gpiod[GN4124_GPIO_BOOTSEL0]))
		goto err_sel0;

	spec->gpiod[GN4124_GPIO_BOOTSEL1] = gpiod_get_index(&spec->dev,
								 "bootsel", 1,
								 GPIOD_OUT_HIGH);
	if (IS_ERR(spec->gpiod[GN4124_GPIO_BOOTSEL1]))
		goto err_sel1;

	/* Because of a BUG in RedHat kernel 3.10 we re-set direction */
	err = gpiod_direction_output(spec->gpiod[GN4124_GPIO_BOOTSEL0], 1);
	if (err)
		goto err_out0;
	err = gpiod_direction_output(spec->gpiod[GN4124_GPIO_BOOTSEL1], 1);
	if (err)
		goto err_out1;


	spec->gpiod[GN4124_GPIO_IRQ0] = gpiod_get_index(&spec->dev,
							     "irq", 0,
							     GPIOD_IN);
	if (IS_ERR(spec->gpiod[GN4124_GPIO_IRQ0])) {
		err = PTR_ERR(spec->gpiod[GN4124_GPIO_IRQ0]);
		goto err_irq0;
	}
	spec->gpiod[GN4124_GPIO_IRQ1] = gpiod_get_index(&spec->dev,
							     "irq", 1,
							     GPIOD_IN);
	if (IS_ERR(spec->gpiod[GN4124_GPIO_IRQ1])) {
		err = PTR_ERR(spec->gpiod[GN4124_GPIO_IRQ1]);
		goto err_irq1;
	}
	/* Because of a BUG in RedHat kernel 3.10 we re-set direction */
	err = gpiod_direction_input(spec->gpiod[GN4124_GPIO_IRQ0]);
	if (err)
		goto err_in0;
	err = gpiod_direction_input(spec->gpiod[GN4124_GPIO_IRQ1]);
	if (err)
		goto err_in1;

	return 0;

err_in1:
err_in0:
	gpiod_put(spec->gpiod[GN4124_GPIO_IRQ1]);
err_irq1:
	gpiod_put(spec->gpiod[GN4124_GPIO_IRQ0]);
err_irq0:
err_out1:
err_out0:
	gpiod_put(spec->gpiod[GN4124_GPIO_BOOTSEL1]);
err_sel1:
	gpiod_put(spec->gpiod[GN4124_GPIO_BOOTSEL0]);
err_sel0:
	gpiod_remove_lookup_table(spec->gpiod_table);
err_lookup:
	kfree(lookup->dev_id);
err_dup:
	devm_kfree(&spec->dev, lookup);
	spec->gpiod_table = NULL;

	return -ENODEV;
}

void spec_gpio_exit(struct spec_dev *spec)
{
	gpiod_put(spec->gpiod[GN4124_GPIO_IRQ1]);
	gpiod_put(spec->gpiod[GN4124_GPIO_IRQ0]);
	gpiod_put(spec->gpiod[GN4124_GPIO_BOOTSEL1]);
	gpiod_put(spec->gpiod[GN4124_GPIO_BOOTSEL0]);
	gpiod_remove_lookup_table(spec->gpiod_table);
	kfree(spec->gpiod_table->dev_id);
}
