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
#include <linux/gpio/consumer.h>
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
 * @gpio_44: gpio_desc for 44.1 kHz
 * @gpio_48: gpio_desc for 48 kHz
 * @mode: 0 => CLK44EN, 1 => CLK48EN
 * @prepared: boolean caching clock state
 */
struct clk_pcm512x_sclk_hw {
	struct clk_hw hw;
	struct regmap *regmap;
	struct gpio_desc *gpio_44;
	struct gpio_desc *gpio_48;
	u8 mode;
	u32 prepared;
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

	pr_info("clk_pcm512x_sclk_is_prepared\n");

	return (clk->prepared) ? 1 : 0;
}

static int clk_pcm512x_sclk_set(struct gpio_desc *gpiod,
				struct regmap *regmap,
				unsigned int value)
{
	int ret;
	unsigned int val;
	unsigned int sck;

	gpiod_set_value(gpiod, value);

	/* wait 2-3 ms for clock transitions */
	usleep_range(2000, 3000);

	/* check if sclk status is correct */
	ret = regmap_read(regmap,  PCM512x_RATE_DET_4, &val);
	if (ret < 0)
		return ret;

	sck = !((val >> 5) & 1); /* Bit5 0: PLL locked, 1: PLL unlocked */
	if (value != sck) {
		pr_err("plb: clock problem: value %d, register %d, sck %d\n",
		       value, val, sck);
		return -EIO; /* FIXME: is this the right value */
	}

	return 0;
}

static int clk_pcm512x_sclk_prepare(struct clk_hw *hw)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);
	struct regmap *regmap = clk->regmap;
	int ret;

	pr_err("clk_pcm512x_sclk_prepare\n");

	switch (clk->mode) {
	case 0:
		ret = clk_pcm512x_sclk_set(clk->gpio_44, regmap, 1);
		break;
	case 1:
		ret = clk_pcm512x_sclk_set(clk->gpio_48, regmap, 1);
		break;
	default:
		return -EINVAL;
	}

	if (ret == 0)
		clk->prepared = 1;

	return ret;
}

static void clk_pcm512x_sclk_unprepare(struct clk_hw *hw)
{
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);
	struct regmap *regmap = clk->regmap;

	pr_err("clk_pcm512x_sclk_unprepare\n");

	switch (clk->mode) {
	case 0:
		clk_pcm512x_sclk_set(clk->gpio_44, regmap, 0);
		break;
	case 1:
		clk_pcm512x_sclk_set(clk->gpio_48, regmap, 0);
		break;
	default:
		return;
	}

	clk->prepared = false;
}

static int clk_pcm512x_sclk_set_rate(struct clk_hw *hw,
				     unsigned long rate,
				     unsigned long parent_rate)
{
	unsigned long actual_rate;
	struct clk_pcm512x_sclk_hw *clk = to_pcm512x_sclk(hw);

	pr_err("clk_pcm512x_sclk_set_rate\n");

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
	int ret;

	pr_err("plb clk_pcm512x_sclk_probe\n");

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

#ifdef DEBUG
	{
		unsigned int val;

		ret = regmap_read(pcm512x_sclk->regmap, PCM512x_RATE_DET_4, &val);
		if (ret < 0) {
			pr_err("plb: regmap2 read failed\n");
			return ret;
		}
		pr_err("plb: regmap2 worked\n");
	}
#endif

	/* we cannot use devm_gpiod_get since the device is NULL */
	pcm512x_sclk->gpio_44 = gpiod_get(NULL, "PCM512x-GPIO6",
					  GPIOD_OUT_LOW);
	if (IS_ERR(pcm512x_sclk->gpio_44)) {
		dev_err(dev, "gpio44 not found\n");
		return PTR_ERR(pcm512x_sclk->gpio_44);
	}

	pcm512x_sclk->gpio_48 = gpiod_get(NULL, "PCM512x-GPIO3",
					  GPIOD_OUT_LOW);
	if (IS_ERR(pcm512x_sclk->gpio_48)) {
		dev_err(dev, "gpio48 not found\n");
		ret = PTR_ERR(pcm512x_sclk->gpio_48);
		goto out1;
	}
	dev_info(dev, "GPIO 44 and 48 ok\n");

	/* check if the clocks actually work */
	ret = clk_pcm512x_sclk_set(pcm512x_sclk->gpio_44,
				   pcm512x_sclk->regmap,
				   1);
	if (ret < 0) {
		dev_err(dev, "Could not set 44.1 kHz clk\n");
		goto out2;
	}

	ret = clk_pcm512x_sclk_set(pcm512x_sclk->gpio_44,
				   pcm512x_sclk->regmap,
				   0);
	if (ret < 0) {
		dev_err(dev, "Could not stop 44.1 kHz clk\n");
		goto out2;
	}

	ret = clk_pcm512x_sclk_set(pcm512x_sclk->gpio_48,
				   pcm512x_sclk->regmap,
				   1);
	if (ret < 0) {
		dev_err(dev, "Could not set 48 kHz clk\n");
		goto out2;
	}

	ret = clk_pcm512x_sclk_set(pcm512x_sclk->gpio_48,
				   pcm512x_sclk->regmap,
				   0);
	if (ret < 0) {
		dev_err(dev, "Could not stop 48 kHz clk\n");
		goto out2;
	}

	/* clock is fully functional, register it */

	clk = devm_clk_register(dev, &pcm512x_sclk->hw);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to register clock driver\n");
		ret = PTR_ERR(clk);
		goto out2;
	}

	platform_set_drvdata(pdev, pcm512x_sclk);

	return 0;

out2:
	gpiod_put(pcm512x_sclk->gpio_48);
out1:
	gpiod_put(pcm512x_sclk->gpio_44);
	return ret;
}

static int clk_pcm512x_sclk_remove(struct platform_device *pdev)
{
	struct clk_pcm512x_sclk_hw *pcm512x_sclk;

	pcm512x_sclk = platform_get_drvdata(pdev);

	/* only deal with gpios, clocks are handled with devm_ */
	gpiod_put(pcm512x_sclk->gpio_48);
	gpiod_put(pcm512x_sclk->gpio_44);

	return 0;
}

static struct platform_driver clk_pcm512x_sclk_driver = {
	.probe = clk_pcm512x_sclk_probe,
	.remove = clk_pcm512x_sclk_remove,
	.driver = {
		.name = "clk-pcm512x-sclk",
	},
};
module_platform_driver(clk_pcm512x_sclk_driver);
MODULE_DESCRIPTION("platform clock driver for PCM512x boards");
MODULE_AUTHOR("Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:clk-pcm512x-sclk");
