/*
 *  intel_hdmi_lpe_audio.c - Intel HDMI LPE audio driver for Atom platforms
 *
 *  Copyright (C) 2016 Intel Corp
 *  Authors:	Sailaja Bandarupalli <sailaja.bandarupalli@intel.com>
 *		Ramesh Babu K V	<ramesh.babu@intel.com>
 *		Vaibhav Agarwal <vaibhav.agarwal@intel.com>
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

#define pr_fmt(fmt)	"hdmi_lpe_audio: " fmt

#include <linux/platform_device.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <drm/i915_hdmi_lpe_audio.h>
#include "intel_hdmi_lpe_audio.h"

struct hdmi_lpe_audio_ctx {
	int irq;
	void __iomem* mmio_start;
};

static irqreturn_t display_pipe_interrupt_handler(int irq, void *dev_id)
{
    return IRQ_HANDLED;
}

static void pin_eld_notify(void *audio_ptr, int port)
{
	/* audio_ptr is not used here */

	/* we assume only from port-B to port-D */
	if (port < 1 || port > 3)
		return;

	printk(KERN_ERR "ELD notification received for port %d\n", port);
}

/**
 * hdmi_lpe_audio_probe - start bridge with i915
 *
 * This function is called when the i915 driver creates the hdmi-lpe-audio
 * platform device. Card creation is deferred until a hot plug event is
 * received
 */
static int hdmi_lpe_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hdmi_lpe_audio_ctx *ctx;
	struct i915_hdmi_lpe_audio_ops *pdata;
	int irq;
	struct resource *res_mmio;
	void __iomem* mmio_start;
	int ret;

	dev_dbg(dev, "Enter %s\n", __func__);

	/* get resources */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_dbg(dev, "Could not get irq resource\n");
		return -ENODEV;
	}

	res_mmio = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mmio) {
		dev_dbg(dev, "Could not get IO_MEM resources\n");
		return -ENXIO;
	}

	mmio_start = ioremap_nocache(res_mmio->start,
					res_mmio->end - res_mmio->start);
	if (!mmio_start) {
		dev_dbg(dev, "Could not get ioremap\n");
		return -EACCES;
	}

	/* setup interrupt handler */
	ret = request_irq(irq, display_pipe_interrupt_handler,
			0, /* FIXME: is IRQF_SHARED needed ? */
			pdev->name,
			NULL);
	if (ret < 0) {
		dev_dbg(dev, "request_irq failed\n");
		iounmap(mmio_start);
		return -ENODEV;
	}

	/* alloc and save context */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		dev_dbg(dev, "out of memory for ctx\n");
		free_irq(irq, NULL);
		iounmap(mmio_start);
		return -ENOMEM;
	}

	ctx->irq = irq;
	ctx->mmio_start = mmio_start;

	/* assign pdata for ELD notification */
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_dbg(dev, "out of memory for pdata\n");
		kfree(ctx);
		free_irq(irq, NULL);
		iounmap(mmio_start);
		return -ENOMEM;
	}

	printk(KERN_ERR "hdmi lpe audio: setting pin eld notify callback\n");
	pdata->pin_eld_notify = pin_eld_notify;
	pdev->dev.platform_data = pdata;

	platform_set_drvdata(pdev, ctx);

	return 0;
}

/**
 * hdmi_lpe_audio_remove - stop bridge with i915
 *
 * This function is called when the platform device is destroyed. The sound
 * card should have been removed on hot plug event.
 */
static int hdmi_lpe_audio_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hdmi_lpe_audio_ctx *ctx;
	struct i915_hdmi_lpe_audio_ops *pdata = dev_get_platdata(&pdev->dev);

	dev_dbg(dev, "Enter %s\n", __func__);

	/* get context, release resources */
	ctx = platform_get_drvdata(pdev);
	iounmap(ctx->mmio_start);
	free_irq(ctx->irq, NULL);
	kfree(ctx);
	pdev->dev.platform_data = NULL;
	kfree(pdata);
	return 0;
}

static struct platform_driver hdmi_lpe_audio_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name  = "hdmi-lpe-audio",
	},
	.probe          = hdmi_lpe_audio_probe,
	.remove		= hdmi_lpe_audio_remove,
};
module_platform_driver(hdmi_lpe_audio_driver);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:hdmi_lpe_audio");
