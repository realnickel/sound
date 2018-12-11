// SPDX-License-Identifier: GPL-2.0
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>
//

/* Based on initial Clock Driver for HiFiBerry DAC Pro
 *
 * Author: Stuart MacLean
 *         Copyright 2015
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "../../codecs/pcm512x.h"

/* Clock rate of CLK44EN attached to GPIO6 pin */
#define CLK_44EN_RATE 22579200UL
/* Clock rate of CLK48EN attached to GPIO3 pin */
#define CLK_48EN_RATE 24576000UL

/**
 * struct clk_pcm512x_sclk_hw - Common struct
 * @hw: clk_hw for the common clk framework
 * @mode: 0 => CLK44EN, 2 => CLK48EN
 */
struct clk_pcm512x_sclk_hw {
	struct clk_hw hw;
	struct regmap *regmap;
	u8 mode;
	u32 prepared; /* TODO: check if PCM512x can provide clock status */
};

#define to_pcm512x_sclk(_hw) container_of(_hw, struct clk_pcm512x_sclk_hw, hw)

static unsigned long clk_pcm512x_sclk_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);

	switch (clk->mode) {
	case 0:
		return CLK_44EN_RATE;
	case 1:
	default:
		return CLK_48EN_RATE;
	}

	/* this should not happen */
	return CLK_48EN_RATE;
}

static long clk_pcm512x_sclk_round_rate(struct clk_hw *hw,
					unsigned long rate,
					unsigned long *parent_rate)
{
	long actual_rate;

	if (rate <= CLK_44EN_RATE) {
		actual_rate = (long)CLK_44EN_RATE;
	} else if (rate >= CLK_48EN_RATE) {
		actual_rate = (long)CLK_48EN_RATE;
	} else {
		long diff44_rate = (long)(rate - CLK_44EN_RATE);
		long diff48_rate = (long)(CLK_48EN_RATE - rate);

		if (diff44_rate < diff48_rate)
			actual_rate = (long)CLK_44EN_RATE;
		else
			actual_rate = (long)CLK_48EN_RATE;
	}
	return actual_rate;
}

static int clk_pcm512x_sclk_is_prepared(struct clk_hw *hw)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);

	printk(KERN_ERR "clk_pcm512x_sclk_is_prepared\n");

	return (clk->prepared) ? 1 : 0;
}

static int clk_pcm512x_sclk_prepare(struct clk_hw *hw)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);
	struct regmap *regmap = clk->regmap;

	printk(KERN_ERR "clk_pcm512x_sclk_prepare\n");

	switch (clk->mode) {
	case 0:
		regmap_update_bits(regmap, PCM512x_GPIO_CONTROL_1, 0x24, 0x20);
		break;
	case 1:
	default:
		clk->mode = 2;
		regmap_update_bits(regmap, PCM512x_GPIO_CONTROL_1, 0x24, 0x04);
		break;
	}

	/* wait 2-3 ms for clock to stabilize */
	usleep_range(2000, 3000);

	clk->prepared = true;

	return 0;
}

static void clk_pcm512x_sclk_unprepare(struct clk_hw *hw)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);
	struct regmap *regmap = clk->regmap;

	printk(KERN_ERR "clk_pcm512x_sclk_unprepare\n");

	regmap_update_bits(regmap, PCM512x_GPIO_CONTROL_1, 0x24, 0x00);

	/* wait 2-3 ms for clock to stabilize */
	usleep_range(2000, 3000);

	clk->prepared = false;
}

static int clk_pcm512x_sclk_set_rate(struct clk_hw *hw,
				     unsigned long rate,
				     unsigned long parent_rate)
{
	unsigned long actual_rate;
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);

	printk(KERN_ERR "clk_pcm512x_sclk_set_rate\n");

	/* don't change clock if it's already prepared */
	if (clk_pcm512x_sclk_is_prepared(hw))
		return -EPERM;

	actual_rate = (unsigned long)clk_pcm512x_sclk_round_rate(hw, rate,
								 &parent_rate);
	clk->mode = (actual_rate == CLK_44EN_RATE) ? 0 : 1;

	return 0;
}

const struct clk_ops clk_pcm512x_sclk_rate_ops = {
	.is_prepared = clk_pcm512x_sclk_is_prepared,
	.prepare = clk_pcm512x_sclk_prepare,
	.unprepare = clk_pcm512x_sclk_unprepare,
	.recalc_rate = clk_pcm512x_sclk_recalc_rate,
	.round_rate = clk_pcm512x_sclk_round_rate,
	.set_rate = clk_pcm512x_sclk_set_rate,
};

static int clk_pcm512x_sclk_probe(struct platform_device *pdev)
{
	struct clk_pcm512x_sclk_hw *pcm512x_sclk;
	struct clk_init_data init;
	struct device *dev;
	struct clk *clk;

	dev = &pdev->dev;

	pcm512x_sclk = devm_kzalloc(dev,
				    sizeof(struct clk_pcm512x_sclk_hw),
				    GFP_KERNEL);
	if (!pcm512x_sclk)
		return -ENOMEM;

	init.name = "clk-pcm512x-sclk";
	init.ops = &clk_pcm512x_sclk_rate_ops;
	init.flags = CLK_IS_BASIC;
	init.parent_names = NULL;
	init.num_parents = 0;

	pcm512x_sclk->regmap = dev->platform_data;
	pcm512x_sclk->mode = 1; /* default 48 kHz */
	pcm512x_sclk->prepared = false;
	pcm512x_sclk->hw.init = &init;

	clk = devm_clk_register(dev, &pcm512x_sclk->hw);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to register clock driver\n");
		return PTR_ERR(clk);
	}
	return 0;
}

static struct platform_driver clk_pcm512x_sclk_driver = {
	.probe = clk_pcm512x_sclk_probe,
	.driver = {
		.name = "clk-pcm512x-sclk",
	},
};
module_platform_driver(clk_pcm512x_sclk_driver);
MODULE_DESCRIPTION("platform clock driver for PCM512x boards");
MODULE_AUTHOR("Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:clk-pcm512x-sclk");
