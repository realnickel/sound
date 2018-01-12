/*
 * Intel Skylake+ HDMI-only Machine Driver
 *
 * Copyright (C) 2018, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel SKL-RT286 machine driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include "../../codecs/hdac_hdmi.h"

static struct snd_soc_jack hdmi_jack[3];

struct hdmi_pcm {
	struct list_head head;
	struct snd_soc_dai *codec_dai;
	int device;
};

struct hdmi_private {
	struct list_head pcm_list;
};

enum {
	DPCM_AUDIO_HDMI1_PB,
	DPCM_AUDIO_HDMI2_PB,
	DPCM_AUDIO_HDMI3_PB,
};

static const struct snd_soc_dapm_widget hdmi_widgets[] = {
	SND_SOC_DAPM_SPK("HDMI1", NULL),
	SND_SOC_DAPM_SPK("HDMI2", NULL),
	SND_SOC_DAPM_SPK("HDMI3", NULL),
};

static const struct snd_soc_dapm_route hdmi_map[] = {
	{ "hifi3", NULL, "iDisp3 Tx"},
	{ "iDisp3 Tx", NULL, "iDisp3_out"},
	{ "hifi2", NULL, "iDisp2 Tx"},
	{ "iDisp2 Tx", NULL, "iDisp2_out"},
	{ "hifi1", NULL, "iDisp1 Tx"},
	{ "iDisp1 Tx", NULL, "iDisp1_out"},

};

static int hdmi_init(struct snd_soc_pcm_runtime *rtd)
{
	struct hdmi_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *dai = rtd->codec_dai;
	struct hdmi_pcm *pcm;

	pcm = devm_kzalloc(rtd->card->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	pcm->device = DPCM_AUDIO_HDMI1_PB + dai->id;
	pcm->codec_dai = dai;

	list_add_tail(&pcm->head, &ctx->pcm_list);

	return 0;
}

static const unsigned int rates[] = {
	48000,
};

static const struct snd_pcm_hw_constraint_list constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list  = rates,
	.mask = 0,
};

static const unsigned int channels[] = {
	2,
};

static const struct snd_pcm_hw_constraint_list constraints_channels = {
	.count = ARRAY_SIZE(channels),
	.list = channels,
	.mask = 0,
};

static int hdmi_fe_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	/*
	 * on this platform for PCM device we support,
	 *	48Khz
	 *	stereo
	 *	16 bit audio
	 */

	runtime->hw.channels_max = 2;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
					   &constraints_channels);

	runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
	snd_pcm_hw_constraint_msbits(runtime, 0, 16, 16);

	snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &constraints_rates);

	return 0;
}

static const struct snd_soc_ops hdmi_fe_ops = {
	.startup = hdmi_fe_startup,
};

/* digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link hdmi_dais[] = {
	[DPCM_AUDIO_HDMI1_PB] = {
		.name = "HDMI Port1",
		.stream_name = "Hdmi1",
		.cpu_dai_name = "HDMI1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.init = NULL,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &hdmi_fe_ops,
	},
	[DPCM_AUDIO_HDMI2_PB] = {
		.name = "HDMI Port2",
		.stream_name = "Hdmi2",
		.cpu_dai_name = "HDMI2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.init = NULL,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &hdmi_fe_ops,
	},
	[DPCM_AUDIO_HDMI3_PB] = {
		.name = "HDMI Port3",
		.stream_name = "Hdmi3",
		.cpu_dai_name = "HDMI3 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.dpcm_playback = 1,
		.init = NULL,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &hdmi_fe_ops,
	},

	/* Back End DAI links */
	{
		.name = "iDisp1",
		.id = 0,
		.cpu_dai_name = "iDisp1 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi1",
		.platform_name = "0000:00:0e.0",
		.init = hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp2",
		.id = 1,
		.cpu_dai_name = "iDisp2 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi2",
		.platform_name = "0000:00:0e.0",
		.init = hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp3",
		.id = 2,
		.cpu_dai_name = "iDisp3 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi3",
		.platform_name = "0000:00:0e.0",
		.init = hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
};

#define NAME_SIZE	32
static int hdmi_card_late_probe(struct snd_soc_card *card)
{
	struct hdmi_private *ctx = snd_soc_card_get_drvdata(card);
	struct hdmi_pcm *pcm;
	struct snd_soc_codec *codec = NULL;
	int err, i = 0;
	char jack_name[NAME_SIZE];

	list_for_each_entry(pcm, &ctx->pcm_list, head) {
		codec = pcm->codec_dai->codec;
		snprintf(jack_name, sizeof(jack_name),
			"HDMI/DP, pcm=%d Jack", pcm->device);
		err = snd_soc_card_jack_new(card, jack_name,
					SND_JACK_AVOUT, &hdmi_jack[i],
					NULL, 0);

		if (err)
			return err;

		err = hdac_hdmi_jack_init(pcm->codec_dai, pcm->device,
						&hdmi_jack[i]);
		if (err < 0)
			return err;

		i++;
	}

	if (!codec)
		return -EINVAL;

	return hdac_hdmi_jack_port_init(codec, &card->dapm);
}

/* audio machine driver for HDMI-only */
static struct snd_soc_card hdmi = {
	.name = "skl_hdmi",
	.owner = THIS_MODULE,
	.dai_link = hdmi_dais,
	.num_links = ARRAY_SIZE(hdmi_dais),
	.dapm_widgets = hdmi_widgets,
	.num_dapm_widgets = ARRAY_SIZE(hdmi_widgets),
	.dapm_routes = hdmi_map,
	.num_dapm_routes = ARRAY_SIZE(hdmi_map),
	.fully_routed = true,
	.late_probe = hdmi_card_late_probe,
};

static int hdmi_audio_probe(struct platform_device *pdev)
{
	struct hdmi_private *ctx;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_ATOMIC);
	if (!ctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&ctx->pcm_list);

	hdmi.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&hdmi, ctx);

	return devm_snd_soc_register_card(&pdev->dev, &hdmi);
}

static struct platform_driver skl_hdmi_audio = {
	.probe = hdmi_audio_probe,
	.driver = {
		.name = "skl_hdmi",
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(skl_hdmi_audio)

/* Module information */
MODULE_AUTHOR("Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>");
MODULE_DESCRIPTION("Intel HDMI only Audio for Skylake+");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:skl_hdmi");
