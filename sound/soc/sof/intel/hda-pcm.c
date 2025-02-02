// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Authors: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	    Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *	    Jeeja KP <jeeja.kp@intel.com>
 *	    Rander Wang <rander.wang@intel.com>
 *          Keyon Jie <yang.jie@linux.intel.com>
 */

/*
 * Hardware interface for generic Intel audio DSP HDA IP
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <sound/hdaudio_ext.h>
#include <sound/sof.h>
#include <sound/pcm_params.h>
#include <linux/pm_runtime.h>

#include "../sof-priv.h"
#include "../ops.h"
#include "hda.h"

#define SDnFMT_BASE(x)	(x << 14)
#define SDnFMT_MULT(x)	((x - 1) << 11)
#define SDnFMT_DIV(x)	((x - 1) << 8)
#define SDnFMT_BITS(x)	(x << 4)
#define SDnFMT_CHAN(x)	(x << 0)

static inline u32 get_mult_div(struct snd_sof_dev *sdev, int rate)
{
	switch (rate) {
	case 8000:
		return SDnFMT_DIV(6);
	case 9600:
		return SDnFMT_DIV(5);
	case 11025:
		return SDnFMT_BASE(1) | SDnFMT_DIV(4);
	case 16000:
		return SDnFMT_DIV(3);
	case 22050:
		return SDnFMT_BASE(1) | SDnFMT_DIV(2);
	case 32000:
		return SDnFMT_DIV(3) | SDnFMT_MULT(2);
	case 44100:
		return SDnFMT_BASE(1);
	case 48000:
		return 0;
	case 88200:
		return SDnFMT_BASE(1) | SDnFMT_MULT(2);
	case 96000:
		return SDnFMT_MULT(2);
	case 176400:
		return SDnFMT_BASE(1) | SDnFMT_MULT(4);
	case 192000:
		return SDnFMT_MULT(4);
	default:
		dev_warn(sdev->dev, "can't find div rate %d using 48kHz\n",
			 rate);
		return 0; /* use 48KHz if not found */
	}
};

static inline u32 get_bits(struct snd_sof_dev *sdev, int sample_bits)
{
	switch (sample_bits) {
	case 8:
		return SDnFMT_BITS(0);
	case 16:
		return SDnFMT_BITS(1);
	case 20:
		return SDnFMT_BITS(2);
	case 24:
		return SDnFMT_BITS(3);
	case 32:
		return SDnFMT_BITS(4);
	default:
		dev_warn(sdev->dev, "can't find %d bits using 16bit\n",
			 sample_bits);
		return SDnFMT_BITS(1); /* use 16bits format if not found */
	}
};

int hda_dsp_pcm_hw_params(struct snd_sof_dev *sdev,
			  struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	struct sof_intel_hda_stream *stream = substream->runtime->private_data;
	struct snd_dma_buffer *dmab;
	int ret;
	u32 size, rate, bits;

	size = params_buffer_bytes(params);
	rate = get_mult_div(sdev, params_rate(params));
	bits = get_bits(sdev, params_width(params));

	stream->substream = substream;

	dmab = substream->runtime->dma_buffer_p;

	stream->config = rate | bits | (params_channels(params) - 1);
	stream->bufsize = size;

	ret = hda_dsp_stream_hw_params(sdev, stream, dmab, params);
	if (ret < 0) {
		dev_err(sdev->dev, "error: hdac prepare failed: %x\n", ret);
		return ret;
	}

	/* disable SPIB, to enable buffer wrap for stream */
	hda_dsp_stream_spib_config(sdev, stream, HDA_DSP_SPIB_DISABLE, 0);

	/*
	 * start HDA DMA here, as DSP require the DMA copy is available
	 * at its trigger start, which is actually before stream trigger
	 * start
	 */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
				1 << stream->index,
				1 << stream->index);

	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, stream->sd_offset,
				SOF_HDA_SD_CTL_DMA_START |
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_SD_CTL_DMA_START |
				SOF_HDA_CL_DMA_SD_INT_MASK);

	return stream->tag;
}

int hda_dsp_pcm_open(struct snd_sof_dev *sdev,
		     struct snd_pcm_substream *substream)
{
	struct sof_intel_hda_stream *stream;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		stream = hda_dsp_stream_get_pstream(sdev);
	else
		stream = hda_dsp_stream_get_cstream(sdev);

	if (!stream) {
		dev_err(sdev->dev, "error: no stream available\n");
		return -ENODEV;
	}

	/* binding pcm substream to hda stream */
	substream->runtime->private_data = stream;
	return 0;
}

int hda_dsp_pcm_close(struct snd_sof_dev *sdev,
		      struct snd_pcm_substream *substream)
{
	struct sof_intel_hda_stream *stream = substream->runtime->private_data;
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = hda_dsp_stream_put_pstream(sdev, stream->tag);
	else
		ret = hda_dsp_stream_put_cstream(sdev, stream->tag);

	if (ret) {
		dev_dbg(sdev->dev, "stream %s not opened!\n", substream->name);
		return -ENODEV;
	}

	/* unbinding pcm substream to hda stream */
	substream->runtime->private_data = NULL;
	return 0;
}

