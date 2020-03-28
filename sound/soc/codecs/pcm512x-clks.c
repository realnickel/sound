// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2018-2020 Intel Corporation. All rights reserved.

/*
 * Clock Driver for PCM512x boards
 *
 * based on initial driver "Clock Driver for HiFiBerry DAC Pro"
 *
 * Author: Stuart MacLean
 *         Copyright 2015
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "pcm512x.h"

/* Clock rate of CLK44EN attached to GPIO6 pin */
#define CLK_44EN_RATE 22579200UL
/* Clock rate of CLK48EN attached to GPIO3 pin */
#define CLK_48EN_RATE 24576000UL

/**
 * struct clk_pcm512x_hw - Common struct to the pcm512x clocks
 * @regmap: register access to control clock lock
 * @hw: clk_hw for the common clk framework
 * @mode: 0 => CLK44EN, 1 => CLK48EN
 * @gpio_48: gpiod desc for 48 kHz support
 * @gpio_44: gpiod desc for 44.1kHz support
 * @prepared: boolean caching clock state
 */
struct clk_pcm512x_hw {
	struct regmap *regmap;
	struct clk_hw hw;
	u8 mode;
	struct gpio_desc *gpio_44;
	struct gpio_desc *gpio_48;
	bool prepared;
};

#define to_pcm512x_clk(_hw) container_of(_hw, struct clk_pcm512x_hw, hw)

static unsigned long clk_pcm512x_recalc_rate(struct clk_hw *hw,
					     unsigned long parent_rate)
{
	return (to_pcm512x_clk(hw)->mode == 0) ? CLK_44EN_RATE :
		CLK_48EN_RATE;
}

static long clk_pcm512x_round_rate(struct clk_hw *hw,
				   unsigned long rate,
				   unsigned long *parent_rate)
{
	long actual_rate;

	if (rate <= CLK_44EN_RATE) {
		actual_rate = (long)CLK_44EN_RATE;
	} else if (rate >= CLK_48EN_RATE) {
		actual_rate = (long)CLK_48EN_RATE;
	} else {
		long diff_44_rate = (long)(rate - CLK_44EN_RATE);
		long diff_48_rate = (long)(CLK_48EN_RATE - rate);

		if (diff_44_rate < diff_48_rate)
			actual_rate = (long)CLK_44EN_RATE;
		else
			actual_rate = (long)CLK_48EN_RATE;
	}
	return actual_rate;
}

static int clk_pcm512x_is_prepared(struct clk_hw *hw)
{
	struct clk_pcm512x_hw *clk = to_pcm512x_clk(hw);

	return (clk->prepared) ? 1 : 0;
}

static int clk_pcm512x_set(struct regmap *regmap,
			   struct gpio_desc *gpiod,
			   unsigned int value)
{
	unsigned int val;
	u8 sck;
	u8 pll;
	int ret;

	gpiod_set_value(gpiod, value);

	/* wait 2-3 ms for clock transitions */
	usleep_range(2000, 3000);

	if (value) {
		/* check if clk status is correct */
		ret = regmap_read(regmap,  PCM512x_RATE_DET_4, &val);
		if (ret < 0)
			return ret;

		sck = (val >> 6) & 1; /* Bit6 0: sclk present, 1: not present */
		if (sck) {
			pr_debug("clock problem: register 94: %x, sck %d\n",
				 val, sck);
			return -EIO;
		}
	}

	return 0;
}

static int clk_pcm512x_prepare(struct clk_hw *hw)
{
	struct clk_pcm512x_hw *clk = to_pcm512x_clk(hw);
	int ret = 0;

	if (clk->prepared)
		return 0;

	switch (clk->mode) {
	case 0:
		/* 44.1 kHz */
		ret = clk_pcm512x_set(clk->regmap, clk->gpio_44, 1);
		break;
	case 1:
		/* 48 kHz */
		ret = clk_pcm512x_set(clk->regmap, clk->gpio_48, 1);
		break;
	default:
		return -EINVAL;
	}

	if (ret == 0)
		clk->prepared = 1;

	return ret;
}

static void clk_pcm512x_unprepare(struct clk_hw *hw)
{
	struct clk_pcm512x_hw *clk = to_pcm512x_clk(hw);

	if (!clk->prepared)
		return;

	switch (clk->mode) {
	case 0:
		clk_pcm512x_set(clk->regmap, clk->gpio_44, 0);
		break;
	case 1:
		clk_pcm512x_set(clk->regmap, clk->gpio_48, 0);
		break;
	default:
		return;
	}

	clk->prepared = false;
}

static int clk_pcm512x_set_rate(struct clk_hw *hw,
				unsigned long rate,
				unsigned long parent_rate)
{
	unsigned long actual_rate;
	struct clk_pcm512x_hw *clk = to_pcm512x_clk(hw);

	actual_rate = (unsigned long)clk_pcm512x_round_rate(hw, rate,
		&parent_rate);
	clk->mode = (actual_rate == CLK_44EN_RATE) ? 0 : 1;
	return 0;
}

const struct clk_ops clk_pcm512x_rate_ops = {
	.is_prepared = clk_pcm512x_is_prepared,
	.prepare = clk_pcm512x_prepare,
	.unprepare = clk_pcm512x_unprepare,
	.recalc_rate = clk_pcm512x_recalc_rate,
	.round_rate = clk_pcm512x_round_rate,
	.set_rate = clk_pcm512x_set_rate,
};

int pcm512x_clk_probe(struct device *dev, struct regmap *regmap)
{
	struct clk_pcm512x_hw *pcm512x_clk;
	struct clk_init_data init;
	int ret = 0;

	pcm512x_clk = devm_kzalloc(dev, sizeof(*pcm512x_clk), GFP_KERNEL);
	if (!pcm512x_clk)
		return -ENOMEM;

	pcm512x_clk->regmap = regmap;

	/* we cannot use devm_gpiod_get since the device is NULL */
	pcm512x_clk->gpio_44 = gpiod_get(NULL, "PCM512x-GPIO6",
					 GPIOD_OUT_LOW);
	if (IS_ERR(pcm512x_clk->gpio_44)) {
		dev_err(dev, "gpio44 not found\n");
		return PTR_ERR(pcm512x_clk->gpio_44);
	}

	pcm512x_clk->gpio_48 = gpiod_get(NULL, "PCM512x-GPIO3",
					 GPIOD_OUT_LOW);
	if (IS_ERR(pcm512x_clk->gpio_48)) {
		dev_err(dev, "gpio48 not found\n");
		ret = PTR_ERR(pcm512x_clk->gpio_48);
		goto err;
	}

	/* check if the clocks actually work */
	ret = clk_pcm512x_set(pcm512x_clk->regmap,
			      pcm512x_clk->gpio_44,
			      1);
	if (ret < 0) {
		dev_dbg(dev, "Could not set 44.1 kHz clk\n");
		goto skip_clock;
	}

	ret = clk_pcm512x_set(pcm512x_clk->regmap,
			      pcm512x_clk->gpio_44,
			      0);
	if (ret < 0) {
		dev_dbg(dev, "Could not stop 44.1 kHz clk\n");
		goto skip_clock;
	}

	ret = clk_pcm512x_set(pcm512x_clk->regmap,
			      pcm512x_clk->gpio_48,
			      1);
	if (ret < 0) {
		dev_dbg(dev, "Could not set 48 kHz clk\n");
		goto skip_clock;
	}

	ret = clk_pcm512x_set(pcm512x_clk->regmap,
			      pcm512x_clk->gpio_48,
			      0);
	if (ret < 0) {
		dev_err(dev, "Could not stop 48 kHz clk\n");
		goto skip_clock;
	}

	/* clock is fully functional, register it */
	init.name = "pcm512x-clk";
	init.ops = &clk_pcm512x_rate_ops;
	init.flags = 0;
	init.parent_names = NULL;
	init.num_parents = 0;

	pcm512x_clk->mode = 1; /* 48 kHz default */
	pcm512x_clk->hw.init = &init;

	ret = devm_clk_hw_register(dev, &pcm512x_clk->hw);
	if (ret < 0) {
		dev_err(dev, "Fail to register clock driver\n");
		goto err1;
	}

	ret = devm_clk_hw_register_clkdev(dev, &pcm512x_clk->hw,
					  init.name, NULL);
	if (ret)
		dev_err(dev, "Failed to create clock driver\n");

	return ret;

skip_clock:
err1:
	gpiod_put(pcm512x_clk->gpio_48);
err:
	gpiod_put(pcm512x_clk->gpio_44);
	return ret;
}
