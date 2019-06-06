// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */
#include <linux/module.h>
#include <linux/gpio/driver.h>

#include "spec.h"

enum gn412x_gpio_versions {
	GN412X_VER = 0,
};

enum htvic_mem_resources {
	GN412X_MEM_BASE = 0,
};

static uint32_t gn412x_ioread32(struct gn412x_dev *gn412x, int reg)
{
	return ioread32(gn412x->mem + reg);
}

static void gn412x_iowrite32(struct gn412x_dev *gn412x, uint32_t val, int reg)
{
	return iowrite32(val, gn412x->mem + reg);
}


static int gn412x_gpio_reg_read(struct gpio_chip *chip,
				int reg, unsigned offset)
{
	struct gn412x_dev *gn412x = to_gn412x_dev_gpio(chip);

	return gn412x_ioread32(gn412x, reg) & BIT(offset);
}

static void gn412x_gpio_reg_write(struct gpio_chip *chip,
				  int reg, unsigned offset, int value)
{
	struct gn412x_dev *gn412x = to_gn412x_dev_gpio(chip);
	uint32_t regval;

	regval = gn412x_ioread32(gn412x, reg);
	if (value)
		regval |= BIT(offset);
	else
		regval &= ~BIT(offset);
	gn412x_iowrite32(gn412x, regval, reg);
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
	return !gn412x_gpio_reg_read(chip, GNGPIO_OUTPUT_ENABLE, offset);
}

static int gn412x_gpio_direction_input(struct gpio_chip *chip,
				       unsigned offset)
{
	gn412x_gpio_reg_write(chip, GNGPIO_OUTPUT_ENABLE, offset, 0);

	return 0;
}

static int gn412x_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
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

int gn412x_gpio_init(struct gn412x_dev *gn412x)
{
	int err;

	memset(&gn412x->gpiochip, 0, sizeof(gn412x->gpiochip));
	gn412x->gpiochip.label = "gn412x-gpio";
	gn412x->gpiochip.owner = THIS_MODULE;
	gn412x->gpiochip.request = gn412x_gpio_request;
	gn412x->gpiochip.free = gn412x_gpio_free;
	gn412x->gpiochip.get_direction = gn412x_gpio_get_direction,
	gn412x->gpiochip.direction_input = gn412x_gpio_direction_input,
	gn412x->gpiochip.direction_output = gn412x_gpio_direction_output,
	gn412x->gpiochip.get = gn412x_gpio_get,
	gn412x->gpiochip.set = gn412x_gpio_set,
	gn412x->gpiochip.base = -1,
	gn412x->gpiochip.ngpio = GN4124_GPIO_MAX,

	err = gpiochip_add(&gn412x->gpiochip);
	if (err)
		return err;

	return 0;
}
void gn412x_gpio_exit(struct gn412x_dev *gn412x)
{
	gpiochip_remove(&gn412x->gpiochip);
}
