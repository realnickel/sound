/*
 *  bytcht-wm8731.c - ASoc Machine driver for Intel Baytrail and
 *             Cherrytrail-based platforms, with Wolfson WM8731 codec
 *
 *  Copyright (C) 2017 Intel Corporation
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

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/platform_sst_audio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../../codecs/wm8731.h"
#include "../atom/sst-atom-controls.h"
#include "../common/sst-acpi.h"

static const struct snd_soc_dapm_widget dapm_widgets[] = {
  	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_LINE("Line In Jacks", NULL),
	SND_SOC_DAPM_MIC("Microphone", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* headphone connected to LHPOUT, RHPOUT */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	/* speaker connected to LOUT, ROUT */
	{"Ext Spk", NULL, "ROUT"},
	{"Ext Spk", NULL, "LOUT"},

	/* line inputs */
	{"Line In Jacks", NULL, "Line Input"},

	/* mic is connected to Mic Jack, with WM8731 Mic Bias */
	{"Microphone", NULL, "Mic Bias"},

	/* SOC-codec link */
	{"ssp2 Tx", NULL, "codec_out0"},
	{"ssp2 Tx", NULL, "codec_out1"},
	{"codec_in0", NULL, "ssp2 Rx"},
	{"codec_in1", NULL, "ssp2 Rx"},

	/* FIXME: is this correct? */
	{"Playback", NULL, "ssp2 Tx"},
	{"ssp2 Rx", NULL, "Capture"},
};

static int codec_fixup(struct snd_soc_pcm_runtime *rtd,
		       struct snd_pcm_hw_params *params)
{
	int ret;
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	/* The DSP will covert the FE rate to 48k, stereo, 24bits */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP2 to 24-bit */
	params_set_format(params, SNDRV_PCM_FORMAT_S24_LE);

	/* FIXME: verify that codec can work in slave mode with 50 bitslots */

	/*
	 * Default mode for SSP configuration is TDM 4 slot, override config
	 * with explicit setting to I2S 2ch 24-bit. The word length is set with
	 * dai_set_tdm_slot() since there is no other API exposed
	 */
	ret = snd_soc_dai_set_fmt(rtd->cpu_dai,
				  SND_SOC_DAIFMT_I2S     |
				  SND_SOC_DAIFMT_NB_NF   |
				  SND_SOC_DAIFMT_CBS_CFS
				  );
	if (ret < 0) {
		dev_err(rtd->dev, "can't set format to I2S, err %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x3, 0x3, 2, 24);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set I2S config, err %d\n", ret);
		return ret;
	}

	return 0;
}

static int aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_single(substream->runtime,
			SNDRV_PCM_HW_PARAM_RATE, 48000);
}

static int init(struct snd_soc_pcm_runtime *rtd)
{
	return snd_soc_dai_set_sysclk(rtd->codec_dai, WM8731_SYSCLK_XTAL,
				      12000000, SND_SOC_CLOCK_IN);
}

static const struct snd_soc_ops aif1_ops = {
	.startup = aif1_startup,
};

static struct snd_soc_dai_link dailink[] = {
	[MERR_DPCM_AUDIO] = {
		.name = "Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "media-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &aif1_ops,
	},
	[MERR_DPCM_DEEP_BUFFER] = {
		.name = "Deep-Buffer Audio Port",
		.stream_name = "Deep-Buffer Audio",
		.cpu_dai_name = "deepbuffer-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.ops = &aif1_ops,
	},
	[MERR_DPCM_COMPR] = {
		.name = "Compressed Port",
		.stream_name = "Compress",
		.cpu_dai_name = "compress-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-mfld-platform",
	},
	/* CODEC<->CODEC link */
	/* back ends */
	{
		.name = "SSP2-Codec",
		.id = 1,
		.cpu_dai_name = "ssp2-port",
		.platform_name = "sst-mfld-platform",
		.no_pcm = 1,
		.codec_dai_name = "wm8731-hifi",
		.codec_name = "i2c-1AEC8731:00",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS,
		.init = init,
		.be_hw_params_fixup = codec_fixup,
		.nonatomic = true,
		.dpcm_playback = 1,
	},
};

/* SoC card */
static struct snd_soc_card bytcht_wm8731_card = {
	.name = "bytcht-wm8731",
	.owner = THIS_MODULE,
	.dai_link = dailink,
	.num_links = ARRAY_SIZE(dailink),
	.dapm_widgets = dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static char codec_name[16]; /* i2c-<HID>:00 with HID being 8 chars */

static int bytcht_wm8731_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	int i;
	struct snd_soc_card *card;
	struct sst_acpi_mach *mach;
	const char *i2c_name = NULL;
	int dai_index = 0;

	mach = (&pdev->dev)->platform_data;
	card = &bytcht_wm8731_card;
	card->dev = &pdev->dev;

	/* fix index of codec dai */
	dai_index = MERR_DPCM_COMPR + 1;
	for (i = 0; i < ARRAY_SIZE(dailink); i++) {
		if (!strcmp(dailink[i].codec_name, "i2c-1AEC8731:00")) {
			dai_index = i;
			break;
		}
	}

	/* fixup codec name based on HID */
	i2c_name = sst_acpi_find_name_from_hid(mach->id);
	if (i2c_name != NULL) {
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

static struct platform_driver bytcht_wm8731_driver = {
	.driver = {
		.name = "bytcht-wm8731",
	},
	.probe = bytcht_wm8731_probe,
};
module_platform_driver(bytcht_wm8731_driver);

MODULE_DESCRIPTION("ASoC Intel(R) Baytrail/Cherrytrail+WM8731 Machine driver");
MODULE_AUTHOR("Pierre-Louis Bossart");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bytcht-wm8731");
