/*
 *  bxt-pcm512x.c - ASoc Machine driver for Intel Baytrail and
 *             Cherrytrail-based platforms, with TI PCM512x codec
 *
 *  Copyright (C) 2016 Intel Corporation
 *  Author: Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define DEBUG

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/platform_sst_audio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include "../../codecs/pcm512x.h"
#include "../atom/sst-atom-controls.h"

#define HIFIBERRY_DACPRO_NOCLOCK 0
#define HIFIBERRY_DACPRO_CLK44EN 1
#define HIFIBERRY_DACPRO_CLK48EN 2

static int sysclk;

/* Clock rate of CLK44EN attached to GPIO6 pin */
#define CLK_44EN_RATE 22579200UL
/* Clock rate of CLK48EN attached to GPIO3 pin */
#define CLK_48EN_RATE 24576000UL

static bool slave;
static bool snd_rpi_hifiberry_is_dacpro;
static bool digital_gain_0db_limit = true;

static void snd_rpi_hifiberry_dacplus_select_clk(struct snd_soc_codec *codec,
	int clk_id)
{
	switch (clk_id) {
	case HIFIBERRY_DACPRO_NOCLOCK:
		snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x24, 0x00);
		break;
	case HIFIBERRY_DACPRO_CLK44EN:
		snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x24, 0x20);
		break;
	case HIFIBERRY_DACPRO_CLK48EN:
		snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x24, 0x04);
		break;
	}
}

static void snd_rpi_hifiberry_dacplus_clk_gpio(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, PCM512x_GPIO_EN, 0x24, 0x24);
	snd_soc_update_bits(codec, PCM512x_GPIO_OUTPUT_3, 0x0f, 0x02);
	snd_soc_update_bits(codec, PCM512x_GPIO_OUTPUT_6, 0x0f, 0x02);
}

static bool snd_rpi_hifiberry_dacplus_is_sclk(struct snd_soc_codec *codec)
{
	int sck;

	sck = snd_soc_read(codec, PCM512x_RATE_DET_4);
	return (!(sck & 0x40));
}

static bool snd_rpi_hifiberry_dacplus_is_sclk_sleep(
	struct snd_soc_codec *codec)
{
	msleep(2);
	return snd_rpi_hifiberry_dacplus_is_sclk(codec);
}

static bool snd_rpi_hifiberry_dacplus_is_pro_card(struct snd_soc_codec *codec)
{
	bool isClk44EN, isClk48En, isNoClk;

	snd_rpi_hifiberry_dacplus_clk_gpio(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec, HIFIBERRY_DACPRO_CLK44EN);
	isClk44EN = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec, HIFIBERRY_DACPRO_NOCLOCK);
	isNoClk = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec, HIFIBERRY_DACPRO_CLK48EN);
	isClk48En = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	return (isClk44EN && isClk48En && !isNoClk);
}

static int snd_rpi_hifiberry_dacplus_clk_for_rate(int sample_rate)
{
	int type;

	switch (sample_rate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
	case 352800:
		type = HIFIBERRY_DACPRO_CLK44EN;
		break;
	default:
		type = HIFIBERRY_DACPRO_CLK48EN;
		break;
	}
	return type;
}

static void snd_rpi_hifiberry_dacplus_set_sclk(struct snd_soc_pcm_runtime *rtd,
					       int sample_rate)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ctype;
	int ret;

	ctype = snd_rpi_hifiberry_dacplus_clk_for_rate(sample_rate);
	sysclk = (ctype == HIFIBERRY_DACPRO_CLK44EN) ?
		CLK_44EN_RATE : CLK_48EN_RATE;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, sysclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec sysclk configuration\n");
		return;
	}

	snd_rpi_hifiberry_dacplus_select_clk(codec, ctype);
}

static int snd_rpi_hifiberry_dacplus_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;

	if (slave)
		snd_rpi_hifiberry_is_dacpro = false;
	else
		snd_rpi_hifiberry_is_dacpro =
				snd_rpi_hifiberry_dacplus_is_pro_card(codec);

	if (snd_rpi_hifiberry_is_dacpro) {
		struct snd_soc_dai_link *dai = rtd->dai_link;

#if 0
		dai->name = "HiFiBerry DAC+ Pro";
		dai->stream_name = "HiFiBerry DAC+ Pro HiFi";
#endif

		snd_rpi_hifiberry_dacplus_set_sclk(rtd, 48000);

		dai->dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBM_CFM;

		snd_soc_update_bits(codec, PCM512x_BCLK_LRCLK_CFG, 0x31, 0x11);
		snd_soc_update_bits(codec, PCM512x_MASTER_MODE, 0x03, 0x03);
		snd_soc_update_bits(codec, PCM512x_MASTER_CLKDIV_2, 0x7f, 63);
	}

	snd_soc_update_bits(codec, PCM512x_GPIO_EN, 0x08, 0x08);
	snd_soc_update_bits(codec, PCM512x_GPIO_OUTPUT_4, 0x0f, 0x02);
	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x08);

	if (digital_gain_0db_limit) {
		int ret;
		struct snd_soc_card *card = rtd->card;

		ret = snd_soc_limit_volume(card, "Digital Playback Volume", 207);
		if (ret < 0)
			dev_warn(card->dev, "Failed to set volume limit: %d\n", ret);
	}

	return 0;
}

static int snd_rpi_hifiberry_dacplus_update_rate_den(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_ratnum *rats_no_pll;
	unsigned int num = 0, den = 0;
	int err;

	rats_no_pll = devm_kzalloc(rtd->dev, sizeof(*rats_no_pll), GFP_KERNEL);
	if (!rats_no_pll)
		return -ENOMEM;

	rats_no_pll->num = sysclk / 64;
	rats_no_pll->den_min = 1;
	rats_no_pll->den_max = 128;
	rats_no_pll->den_step = 1;

	err = snd_interval_ratnum(hw_param_interval(params,
		SNDRV_PCM_HW_PARAM_RATE), 1, rats_no_pll, &num, &den);
	if (err >= 0 && den) {
		params->rate_num = num;
		params->rate_den = den;
	}

	devm_kfree(rtd->dev, rats_no_pll);
	return 0;
}

static int snd_rpi_hifiberry_dacplus_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int channels = params_channels(params);
	int width = 32;

	if (snd_rpi_hifiberry_is_dacpro) {
		width = snd_pcm_format_physical_width(params_format(params));

		snd_rpi_hifiberry_dacplus_set_sclk(rtd,
						   params_rate(params));

		ret = snd_rpi_hifiberry_dacplus_update_rate_den(
			substream, params);
	}

#if 0
	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x03, 0x03,
		channels, width);
	if (ret)
		return ret;
#endif
	ret = snd_soc_dai_set_tdm_slot(rtd->codec_dai, 0x03, 0x03,
		channels, width);
	return ret;
}

static int snd_rpi_hifiberry_dacplus_startup(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x08);
	return 0;
}

static void snd_rpi_hifiberry_dacplus_shutdown(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x00);
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_hifiberry_dacplus_ops = {
	.hw_params = snd_rpi_hifiberry_dacplus_hw_params,
	.startup = snd_rpi_hifiberry_dacplus_startup,
	.shutdown = snd_rpi_hifiberry_dacplus_shutdown,
};

static int codec_fixup(struct snd_soc_pcm_runtime *rtd,
		       struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* The ADSP will covert the FE rate to 48k, stereo */
	rate->min = 48000;
	rate->max = 48000;
	channels->min = 2;
	channels->max = 2;

	/* set SSP5 to 24 bit */
	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);

	return 0;
}


static struct snd_soc_dai_link dailink[] = {
	/* CODEC<->CODEC link */
	/* back ends */
	{
		.name = "SSP5-Codec",
		.id = 0,
		.cpu_dai_name = "sof-audio",
		.platform_name = "sof-audio",
		.no_pcm = 1,
		.codec_dai_name = "pcm512x-hifi",
		.codec_name = "i2c-104C5122:00",
#if 0
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBS_CFS,
#endif
		.nonatomic = true,
		.dpcm_playback = 1,
		.ops		= &snd_rpi_hifiberry_dacplus_ops,
		.init		= snd_rpi_hifiberry_dacplus_init,
		.be_hw_params_fixup = codec_fixup,

	},
};

/* SoC card */
static struct snd_soc_card bxt_pcm512x_card = {
	.name = "bxt-pcm512x",
	.owner = THIS_MODULE,
	.dai_link = dailink,
	.num_links = ARRAY_SIZE(dailink),
};

 /* i2c-<HID>:00 with HID being 8 chars */
static char codec_name[SND_ACPI_I2C_ID_LEN];

static int bxt_pcm512x_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct snd_soc_acpi_mach *mach;
	const char *i2c_name = NULL;
	int dai_index = 0;
	int ret_val = 0, i;

	mach = (&pdev->dev)->platform_data;
	card = &bxt_pcm512x_card;
	card->dev = &pdev->dev;

	/* fix index of codec dai */
	for (i = 0; i < ARRAY_SIZE(dailink); i++) {
		if (!strcmp(dailink[i].codec_name, "i2c-104C5122:00")) {
			dai_index = i;
			break;
		}
	}

	/* fixup codec name based on HID */
	i2c_name = acpi_dev_get_first_match_name(mach->id, NULL, -1);
	if (i2c_name) {
		snprintf(codec_name, sizeof(codec_name),
			 "%s%s", "i2c-", i2c_name);
		dailink[dai_index].codec_name = codec_name;
	}

	ret_val = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret_val) {
		dev_err(&pdev->dev,
			"snd_soc_register_card failed %d\n", ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, card);
	return ret_val;
}

static struct platform_driver bxt_pcm521x_driver = {
	.driver = {
		.name = "bxt-pcm512x",
	},
	.probe = bxt_pcm512x_probe,
};
module_platform_driver(bxt_pcm521x_driver);

MODULE_DESCRIPTION("ASoC Intel(R) Broxton + PCM512x Machine driver");
MODULE_AUTHOR("Pierre-Louis Bossart");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bxt-pcm512x");
