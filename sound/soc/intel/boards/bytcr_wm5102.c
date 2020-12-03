// SPDX-License-Identifier: GPL-2.0-only
/*
 *  byt_cr_dpcm_wm5102.c - ASoc Machine driver for Intel Byt CR platform
 *
 *  Based on bytcr_rt5640 driver
 *  Copyright (C) 2014-2020 Intel Corp
 *  Author: Subhransu S. Prusty <subhransu.s.prusty@intel.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-acpi.h>
#include "../../codecs/wm5102.h"
#include "../atom/sst-atom-controls.h"
#include "../common/soc-intel-quirks.h"

enum {
	BYT_WM5102_DMIC1_MAP,
	BYT_WM5102_DMIC2_MAP,
	BYT_WM5102_IN1_MAP,
	BYT_WM5102_IN3_MAP,
};

#define BYT_WM5102_MAP(quirk)	((quirk) & 0xff)
#define BYT_WM5102_DMIC_EN	BIT(16)
#define BYT_WM5102_MONO_SPEAKER BIT(17)
#define BYT_WM5102_DIFF_MIC	BIT(18) /* default is single-ended */
#define BYT_WM5102_SSP2_AIF2	 BIT(19) /* default is using AIF1  */
#define BYT_WM5102_SSP0_AIF1	 BIT(20)
#define BYT_WM5102_SSP0_AIF2	 BIT(21)
#define BYT_WM5102_MCLK_EN	BIT(22)
#define BYT_WM5102_MCLK_25MHZ	BIT(23)

#define WM5102_MAX_SYSCLK_1 49152000 /*max sysclk for 4K family*/
#define WM5102_MAX_SYSCLK_2 45158400 /*max sysclk for 11.025K family*/

struct byt_wm5102_private {
	struct clk *mclk;
};

static unsigned long byt_wm5102_quirk = BYT_WM5102_MCLK_EN;

static void log_quirks(struct device *dev)
{
	if (BYT_WM5102_MAP(byt_wm5102_quirk) == BYT_WM5102_DMIC1_MAP)
		dev_info(dev, "quirk DMIC1_MAP enabled");
	if (BYT_WM5102_MAP(byt_wm5102_quirk) == BYT_WM5102_DMIC2_MAP)
		dev_info(dev, "quirk DMIC2_MAP enabled");
	if (BYT_WM5102_MAP(byt_wm5102_quirk) == BYT_WM5102_IN1_MAP)
		dev_info(dev, "quirk IN1_MAP enabled");
	if (BYT_WM5102_MAP(byt_wm5102_quirk) == BYT_WM5102_IN3_MAP)
		dev_info(dev, "quirk IN3_MAP enabled");
	if (byt_wm5102_quirk & BYT_WM5102_DMIC_EN)
		dev_info(dev, "quirk DMIC enabled");
	if (byt_wm5102_quirk & BYT_WM5102_MONO_SPEAKER)
		dev_info(dev, "quirk MONO_SPEAKER enabled");
	if (byt_wm5102_quirk & BYT_WM5102_DIFF_MIC)
		dev_info(dev, "quirk DIFF_MIC enabled");
	if (byt_wm5102_quirk & BYT_WM5102_SSP2_AIF2)
		dev_info(dev, "quirk SSP2_AIF2 enabled");
	if (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF1)
		dev_info(dev, "quirk SSP0_AIF1 enabled");
	if (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF2)
		dev_info(dev, "quirk SSP0_AIF2 enabled");
	if (byt_wm5102_quirk & BYT_WM5102_MCLK_EN)
		dev_info(dev, "quirk MCLK_EN enabled");
	if (byt_wm5102_quirk & BYT_WM5102_MCLK_25MHZ)
		dev_info(dev, "quirk MCLK_25MHZ enabled");
}

#define BYT_CODEC_DAI1	"wm5102-aif1"
#define BYT_CODEC_DAI2	"wm5102-aif2"

static int platform_clock_control(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai;
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);
	int ret;

	codec_dai = snd_soc_card_get_codec_dai(card, BYT_CODEC_DAI1);
	if (!codec_dai)
		codec_dai = snd_soc_card_get_codec_dai(card, BYT_CODEC_DAI2);

	if (!codec_dai) {
		dev_err(card->dev,
			"Codec dai not found; Unable to set platform clock\n");
		return -EIO;
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if ((byt_wm5102_quirk & BYT_WM5102_MCLK_EN) && priv->mclk) {
			ret = clk_prepare_enable(priv->mclk);
			if (ret < 0) {
				dev_err(card->dev,
					"could not configure MCLK state");
				return ret;
			}
		}
		ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
					     48000 * 512,
					     SND_SOC_CLOCK_IN);
	} else {
		/*
		 * Set codec clock source to internal clock before
		 * turning off the platform clock. Codec needs clock
		 * for Jack detection and button press
		 */
		ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
					     48000 * 512,
					     SND_SOC_CLOCK_IN);
		if (!ret) {
			if ((byt_wm5102_quirk & BYT_WM5102_MCLK_EN) && priv->mclk)
				clk_disable_unprepare(priv->mclk);
		}
	}

	if (ret < 0) {
		dev_err(card->dev, "can't set codec sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_dapm_widget byt_wm5102_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			    platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMD),

};

static const struct snd_soc_dapm_route byt_wm5102_audio_map[] = {
	{"Headphone", NULL, "Platform Clock"},
	{"Headset Mic", NULL, "Platform Clock"},
	{"Internal Mic", NULL, "Platform Clock"},
	{"Speaker", NULL, "Platform Clock"},

	{"Headset Mic", NULL, "MICBIAS1"},
	{"IN1L", NULL, "Headset Mic"},
	{"Headphone", NULL, "HPOUT1L"},
	{"Headphone", NULL, "HPOUT1R"},
};

static const struct snd_soc_dapm_route byt_wm5102_intmic_dmic1_map[] = {
	{"DMIC1", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_wm5102_intmic_dmic2_map[] = {
	{"DMIC2", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_wm5102_intmic_in1_map[] = {
	{"Internal Mic", NULL, "MICBIAS1"},
	{"IN1P", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_wm5102_intmic_in3_map[] = {
	{"Internal Mic", NULL, "MICBIAS1"},
	{"IN3P", NULL, "Internal Mic"},
};

static const struct snd_soc_dapm_route byt_wm5102_ssp2_aif1_map[] = {
	{"ssp2 Tx", NULL, "codec_out0"},
	{"ssp2 Tx", NULL, "codec_out1"},
	{"codec_in0", NULL, "ssp2 Rx"},
	{"codec_in1", NULL, "ssp2 Rx"},

	{"AIF1 Playback", NULL, "ssp2 Tx"},
	{"ssp2 Rx", NULL, "AIF1 Capture"},
};

static const struct snd_soc_dapm_route byt_wm5102_ssp2_aif2_map[] = {
	{"ssp2 Tx", NULL, "codec_out0"},
	{"ssp2 Tx", NULL, "codec_out1"},
	{"codec_in0", NULL, "ssp2 Rx"},
	{"codec_in1", NULL, "ssp2 Rx"},

	{"AIF2 Playback", NULL, "ssp2 Tx"},
	{"ssp2 Rx", NULL, "AIF2 Capture"},
};

static const struct snd_soc_dapm_route byt_wm5102_ssp0_aif1_map[] = {
	{"ssp0 Tx", NULL, "modem_out"},
	{"modem_in", NULL, "ssp0 Rx"},

	{"AIF1 Playback", NULL, "ssp0 Tx"},
	{"ssp0 Rx", NULL, "AIF1 Capture"},
};

static const struct snd_soc_dapm_route byt_wm5102_ssp0_aif2_map[] = {
	{"ssp0 Tx", NULL, "modem_out"},
	{"modem_in", NULL, "ssp0 Rx"},

	{"AIF2 Playback", NULL, "ssp0 Tx"},
	{"ssp0 Rx", NULL, "AIF2 Capture"},
};

static const struct snd_soc_dapm_route byt_wm5102_stereo_spk_map[] = {
	{"Speaker", NULL, "SPKOUTLP"},
	{"Speaker", NULL, "SPKOUTLN"},
	{"Speaker", NULL, "SPKOUTRP"},
	{"Speaker", NULL, "SPKOUTRN"},
};

static const struct snd_soc_dapm_route byt_wm5102_mono_spk_map[] = {
	{"Speaker", NULL, "SPKOUTLP"},
	{"Speaker", NULL, "SPKOUTLN"},
};

static const struct snd_kcontrol_new byt_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

static int byt_wm5102_aif1_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	int ret;

	int sr = params_rate(params);
	int sr_mult = (params_rate(params) % 4000 == 0) ?
		(WM5102_MAX_SYSCLK_1 / params_rate(params)) :
		(WM5102_MAX_SYSCLK_2 / params_rate(params));

	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
				     params_rate(params) * 512,
				     SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec clock %d\n", ret);
		return ret;
	}

	/*reset FLL1*/
	snd_soc_dai_set_pll(codec_dai, WM5102_FLL1_REFCLK,
			      ARIZONA_FLL_SRC_NONE,
			      0,
			      0);

	snd_soc_dai_set_pll(codec_dai, WM5102_FLL1,
			      ARIZONA_FLL_SRC_NONE,
			      0,
			      0);

	ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL1,
				  ARIZONA_CLK_SRC_MCLK1,
				  25000000,
				  sr * sr_mult);

	if (ret < 0) {
		dev_err(rtd->dev, "can't set codec pll: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai,
				     ARIZONA_CLK_SYSCLK,
				     // FIXME: ARIZONA_CLK_SRC_FLL1,
				     sr * sr_mult,
				     SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(rtd->dev, "Failed to set AYNCCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai,
				     ARIZONA_CLK_SYSCLK,
				     // FIXME: 0,
				     sr * sr_mult,
				     SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(rtd->dev, "can't set OPCLK %d\n", ret);
		return ret;
	}

	return 0;
}

static int byt_wm5102_quirk_cb(const struct dmi_system_id *id)
{
	byt_wm5102_quirk = (unsigned long)id->driver_data;
	return 1;
}

static const struct dmi_system_id byt_wm5102_quirk_table[] = {
	{
		.callback = byt_wm5102_quirk_cb,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_CHASSIS_VERSION, "1051F"),
		},
		.driver_data = (unsigned long *)(BYT_WM5102_MCLK_25MHZ |
						 BYT_WM5102_MCLK_EN |
						 BYT_WM5102_SSP0_AIF1),

	},
	{}
};

static int byt_wm5102_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_card *card = runtime->card;
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);
	int ret;

	card->dapm.idle_bias_off = true;

	ret = snd_soc_add_card_controls(card, byt_wm5102_controls,
					ARRAY_SIZE(byt_wm5102_controls));
	if (ret) {
		dev_err(card->dev, "unable to add card controls\n");
		return ret;
	}

	if (byt_wm5102_quirk & BYT_WM5102_SSP2_AIF2) {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_ssp2_aif2_map,
					      ARRAY_SIZE(byt_wm5102_ssp2_aif2_map));
	} else if (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF1) {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_ssp0_aif1_map,
					      ARRAY_SIZE(byt_wm5102_ssp0_aif1_map));
	} else if (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF2) {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_ssp0_aif2_map,
					      ARRAY_SIZE(byt_wm5102_ssp0_aif2_map));
	} else {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_ssp2_aif1_map,
					      ARRAY_SIZE(byt_wm5102_ssp2_aif1_map));
	}
	if (ret)
		return ret;

	if (byt_wm5102_quirk & BYT_WM5102_MONO_SPEAKER) {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_mono_spk_map,
					      ARRAY_SIZE(byt_wm5102_mono_spk_map));
	} else {
		ret = snd_soc_dapm_add_routes(&card->dapm,
					      byt_wm5102_stereo_spk_map,
					      ARRAY_SIZE(byt_wm5102_stereo_spk_map));
	}
	if (ret)
		return ret;

	snd_soc_dapm_ignore_suspend(&card->dapm, "Headphone");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Speaker");

	if ((byt_wm5102_quirk & BYT_WM5102_MCLK_EN) && priv->mclk) {
		/*
		 * The firmware might enable the clock at
		 * boot (this information may or may not
		 * be reflected in the enable clock register).
		 * To change the rate we must disable the clock
		 * first to cover these cases. Due to common
		 * clock framework restrictions that do not allow
		 * to disable a clock that has not been enabled,
		 * we need to enable the clock first.
		 */
		ret = clk_prepare_enable(priv->mclk);
		if (!ret)
			clk_disable_unprepare(priv->mclk);

		if (byt_wm5102_quirk & BYT_WM5102_MCLK_25MHZ)
			ret = clk_set_rate(priv->mclk, 25000000);
		else
			ret = clk_set_rate(priv->mclk, 19200000);

		if (ret)
			dev_err(card->dev, "unable to set MCLK rate\n");
	}

	return ret;
}

static const struct snd_soc_pcm_stream byt_wm5102_dai_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static int byt_wm5102_codec_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
							  SNDRV_PCM_HW_PARAM_CHANNELS);
	int ret;

	/* The DSP will covert the FE rate to 48k, stereo */
	rate->min = 48000;
	rate->max = 48000;
	channels->min = 2;
	channels->max = 2;

	if ((byt_wm5102_quirk & BYT_WM5102_SSP0_AIF1) ||
	    (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF2)) {

		/* set SSP0 to 16-bit */
		params_set_format(params, SNDRV_PCM_FORMAT_S16_LE);

		/*
		 * Default mode for SSP configuration is TDM 4 slot, override config
		 * with explicit setting to I2S 2ch 16-bit. The word length is set with
		 * dai_set_tdm_slot() since there is no other API exposed
		 */
		ret = snd_soc_dai_set_fmt(asoc_rtd_to_cpu(rtd, 0),
					  SND_SOC_DAIFMT_I2S     |
					  SND_SOC_DAIFMT_NB_NF   |
					  SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0) {
			dev_err(rtd->dev, "can't set format to I2S, err %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_tdm_slot(asoc_rtd_to_cpu(rtd, 0), 0x3, 0x3, 2, 16);
		if (ret < 0) {
			dev_err(rtd->dev, "can't set I2S config, err %d\n", ret);
			return ret;
		}
	} else {

		/* set SSP2 to 24-bit */
		params_set_format(params, SNDRV_PCM_FORMAT_S24_LE);

		/*
		 * Default mode for SSP configuration is TDM 4 slot, override config
		 * with explicit setting to I2S 2ch 24-bit. The word length is set with
		 * dai_set_tdm_slot() since there is no other API exposed
		 */
		ret = snd_soc_dai_set_fmt(asoc_rtd_to_cpu(rtd, 0),
					  SND_SOC_DAIFMT_I2S     |
					  SND_SOC_DAIFMT_NB_NF   |
					  SND_SOC_DAIFMT_CBS_CFS
			);
		if (ret < 0) {
			dev_err(rtd->dev, "can't set format to I2S, err %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_tdm_slot(asoc_rtd_to_cpu(rtd, 0), 0x3, 0x3, 2, 24);
		if (ret < 0) {
			dev_err(rtd->dev, "can't set I2S config, err %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int byt_wm5102_aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_single(substream->runtime,
					    SNDRV_PCM_HW_PARAM_RATE, 48000);
}

static const struct snd_soc_ops byt_wm5102_aif1_ops = {
	.startup = byt_wm5102_aif1_startup,
};

static const struct snd_soc_ops byt_wm5102_be_ssp2_ops = {
	.hw_params = byt_wm5102_aif1_hw_params,
};

SND_SOC_DAILINK_DEF(dummy,
	DAILINK_COMP_ARRAY(COMP_DUMMY()));

SND_SOC_DAILINK_DEF(media,
	DAILINK_COMP_ARRAY(COMP_CPU("media-cpu-dai")));

SND_SOC_DAILINK_DEF(deepbuffer,
	DAILINK_COMP_ARRAY(COMP_CPU("deepbuffer-cpu-dai")));

SND_SOC_DAILINK_DEF(ssp2_port,
	/* overwritten for ssp0 routing */
	DAILINK_COMP_ARRAY(COMP_CPU("ssp2-port")));

SND_SOC_DAILINK_DEF(ssp2_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC(
	/* overwritten with HID */ "i2c-10EC5640:00", // FIXME
	/* changed w/ quirk */	"rt5640-aif1"))); // FIXME

SND_SOC_DAILINK_DEF(platform,
	DAILINK_COMP_ARRAY(COMP_PLATFORM("sst-mfld-platform")));

static struct snd_soc_dai_link byt_wm5102_dais[] = {
	[MERR_DPCM_AUDIO] = {
		.name = "Baytrail Audio Port",
		.stream_name = "Baytrail Audio",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &byt_wm5102_aif1_ops,
		SND_SOC_DAILINK_REG(media, dummy, platform),

	},
	[MERR_DPCM_DEEP_BUFFER] = {
		.name = "Deep-Buffer Audio Port",
		.stream_name = "Deep-Buffer Audio",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.ops = &byt_wm5102_aif1_ops,
		SND_SOC_DAILINK_REG(deepbuffer, dummy, platform),
	},
		/* back ends */
	{
		.name = "SSP2-Codec",
		.id = 0,
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS,
		.be_hw_params_fixup = byt_wm5102_codec_fixup,
		.nonatomic = true,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = byt_wm5102_init,
		.ops = &byt_wm5102_be_ssp2_ops,
		SND_SOC_DAILINK_REG(ssp2_port, ssp2_codec, platform),
	},
};

/* use space before codec name to simplify card ID, and simplify driver name */
#define SOF_CARD_NAME "bytcht wm5102" /* card name will be 'sof-bytcht wm5102' */
#define SOF_DRIVER_NAME "SOF"

#define CARD_NAME "bytcr-wm5102"
#define DRIVER_NAME NULL /* card name will be used for driver name */

/* SoC card */
static struct snd_soc_card byt_wm5102_card = {
	.owner = THIS_MODULE,
	.dai_link = byt_wm5102_dais,
	.num_links = ARRAY_SIZE(byt_wm5102_dais),
	.dapm_widgets = byt_wm5102_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_wm5102_widgets),
	.dapm_routes = byt_wm5102_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_wm5102_audio_map),
	.fully_routed = true,
};

static char byt_wm5102_codec_name[13]; /* wm5102-codec */
static char byt_wm5102_codec_aif_name[12]; /*  = "wm5102-aif[1|2]" */
static char byt_wm5102_cpu_dai_name[10]; /*  = "ssp[0|2]-port" */

struct acpi_chan_package {   /* ACPICA seems to require 64 bit integers */
	u64 aif_value;	     /* 1: AIF1, 2: AIF2 */
	u64 mclock_value;    /* usually 25MHz (0x17d7940), ignored */
};

static int snd_byt_wm5102_mc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct byt_wm5102_private *priv;
	struct snd_soc_acpi_mach *mach;
	const char *platform_name;
	struct acpi_device *adev;
	bool is_bytcr = false;
	bool sof_parent;
	int ret_val = 0;
	int dai_index = 0;
	int i;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_ATOMIC);
	if (!priv)
		return -ENOMEM;

	/* register the soc card */
	byt_wm5102_card.dev = dev;
	mach = byt_wm5102_card.dev->platform_data;
	snd_soc_card_set_drvdata(&byt_wm5102_card, priv);

	/* fix index of codec dai */
	for (i = 0; i < ARRAY_SIZE(byt_wm5102_dais); i++) {
		if (!strcmp(byt_wm5102_dais[i].codecs->name,
			    "wm5102-codec")) { //FIXME
			dai_index = i;
			break;
		}
	}

	/* fixup codec name based on HID */
	adev = acpi_dev_get_first_match_dev(mach->id, NULL, -1);
	if (adev) {
		snprintf(byt_wm5102_codec_name, sizeof(byt_wm5102_codec_name),
			 "spi-%s", acpi_dev_name(adev)); // FIXME
		put_device(&adev->dev);
		byt_wm5102_dais[dai_index].codecs->name = byt_wm5102_codec_name;
	}

	/*
	 * swap SSP0 if bytcr is detected
	 * (will be overridden if DMI quirk is detected)
	 */
	if (soc_intel_is_byt()) {
		if (mach->mach_params.acpi_ipc_irq_index == 0)
			is_bytcr = true;
	}

	if (is_bytcr) {
		/*
		 * Baytrail CR platforms may have CHAN package in BIOS, try
		 * to find relevant routing quirk based as done on Windows
		 * platforms. We have to read the information directly from the
		 * BIOS, at this stage the card is not created and the links
		 * with the codec driver/pdata are non-existent
		 */

		struct acpi_chan_package chan_package;

		/* format specified: 2 64-bit integers */
		struct acpi_buffer format = {sizeof("NN"), "NN"};
		struct acpi_buffer state = {0, NULL};
		struct snd_soc_acpi_package_context pkg_ctx;
		bool pkg_found = false;

		state.length = sizeof(chan_package);
		state.pointer = &chan_package;

		pkg_ctx.name = "CHAN";
		pkg_ctx.length = 2;
		pkg_ctx.format = &format;
		pkg_ctx.state = &state;
		pkg_ctx.data_valid = false;

		pkg_found = snd_soc_acpi_find_package_from_hid(mach->id, &pkg_ctx);
		if (pkg_found) {
			if (chan_package.aif_value == 1) {
				dev_info(dev, "BIOS Routing: AIF1 connected\n");
				byt_wm5102_quirk |= BYT_WM5102_SSP0_AIF1;
			} else	if (chan_package.aif_value == 2) {
				dev_info(dev, "BIOS Routing: AIF2 connected\n");
				byt_wm5102_quirk |= BYT_WM5102_SSP0_AIF2;
			} else {
				dev_info(dev, "BIOS Routing isn't valid, ignored\n");
				pkg_found = false;
			}
		}

		if (!pkg_found) {
			/* no BIOS indications, assume SSP0-AIF1 connection */
			byt_wm5102_quirk |= BYT_WM5102_SSP0_AIF1;
		}

		/* change defaults for Baytrail-CR capture */
		byt_wm5102_quirk |= BYT_WM5102_IN1_MAP;
		byt_wm5102_quirk |= BYT_WM5102_DIFF_MIC;
	} else {
		byt_wm5102_quirk |= (BYT_WM5102_DMIC1_MAP |
				BYT_WM5102_DMIC_EN);
	}

	/* check quirks before creating card */
	dmi_check_system(byt_wm5102_quirk_table);
	log_quirks(dev);

	if ((byt_wm5102_quirk & BYT_WM5102_SSP2_AIF2) ||
	    (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF2)) {

		/* fixup codec aif name */
		snprintf(byt_wm5102_codec_aif_name,
			 sizeof(byt_wm5102_codec_aif_name),
			 "%s", "wm5102-aif2");

		byt_wm5102_dais[dai_index].codecs->dai_name =
			byt_wm5102_codec_aif_name;
	}

	if ((byt_wm5102_quirk & BYT_WM5102_SSP0_AIF1) ||
	    (byt_wm5102_quirk & BYT_WM5102_SSP0_AIF2)) {

		/* fixup cpu dai name */
		snprintf(byt_wm5102_cpu_dai_name,
			 sizeof(byt_wm5102_cpu_dai_name),
			 "%s", "ssp0-port");

		byt_wm5102_dais[dai_index].cpus->dai_name =
			byt_wm5102_cpu_dai_name;
	}

	if (byt_wm5102_quirk & BYT_WM5102_MCLK_EN) {
		priv->mclk = devm_clk_get(dev, "pmc_plt_clk_3");
		if (IS_ERR(priv->mclk)) {
			ret_val = PTR_ERR(priv->mclk);

			dev_err(dev,
				"Failed to get MCLK from pmc_plt_clk_3: %d\n",
				ret_val);

			/*
			 * Fall back to bit clock usage for -ENOENT (clock not
			 * available likely due to missing dependencies), bail
			 * for all other errors, including -EPROBE_DEFER
			 */
			if (ret_val != -ENOENT)
				return ret_val;
			byt_wm5102_quirk &= ~BYT_WM5102_MCLK_EN;
		}
	}

	/* override platform name, if required */
	platform_name = mach->mach_params.platform;

	ret_val = snd_soc_fixup_dai_links_platform_name(&byt_wm5102_card,
							platform_name);
	if (ret_val)
		return ret_val;

	sof_parent = snd_soc_acpi_sof_parent(dev);

	/* set card and driver name */
	if (sof_parent) {
		byt_wm5102_card.name = SOF_CARD_NAME;
		byt_wm5102_card.driver_name = SOF_DRIVER_NAME;
	} else {
		byt_wm5102_card.name = CARD_NAME;
		byt_wm5102_card.driver_name = DRIVER_NAME;
	}

	/* set pm ops */
	if (sof_parent)
		dev->driver->pm = &snd_soc_pm_ops;

	ret_val = devm_snd_soc_register_card(dev, &byt_wm5102_card);

	if (ret_val) {
		dev_err(dev, "devm_snd_soc_register_card failed %d\n",
			ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, &byt_wm5102_card);
	return ret_val;
}

static struct platform_driver snd_byt_wm5102_mc_driver = {
	.driver = {
		.name = "bytcr_wm5102",
	},
	.probe = snd_byt_wm5102_mc_probe,
};

module_platform_driver(snd_byt_wm5102_mc_driver);

MODULE_DESCRIPTION("ASoC Intel(R) Baytrail CR Machine driver");
MODULE_AUTHOR("Subhransu S. Prusty <subhransu.s.prusty@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bytcr_wm5102");
