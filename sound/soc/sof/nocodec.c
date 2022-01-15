// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//

#include <linux/module.h>
#include <sound/sof.h>
#include "sof-audio.h"
#include "sof-priv.h"

static struct snd_soc_card sof_nocodec_card = {
	.name = "nocodec", /* the sof- prefix is added by the core */
	.topology_shortname = "sof-nocodec",
	.owner = THIS_MODULE
};

static int sof_nocodec_bes_setup(struct device *dev,
				 struct snd_soc_dai_driver *drv,
				 struct snd_soc_dai_link *links,
				 int link_num, struct snd_soc_card *card)
{
	struct snd_soc_dai_link_component *dlc;
	int i;
	int j = 0;

	if (!drv || !links || !card)
		return -EINVAL;

	/* set up BE dai_links */
	for (i = 0; i < link_num; i++) {
		int dai_id;

		/* map ssp0 to ssp2, don't enable nocodec-2 dailink */
		if (i == 0)
			dai_id = 2;
		else if (i == 2)
			continue;
		else
			dai_id = i;

		dlc = devm_kzalloc(dev, 3 * sizeof(*dlc), GFP_KERNEL);
		if (!dlc)
			return -ENOMEM;

		links[j].name = devm_kasprintf(dev, GFP_KERNEL,
					       "NoCodec-%d", i);
		if (!links[j].name)
			return -ENOMEM;

		links[j].stream_name = links[j].name;

		links[j].cpus = &dlc[0];
		links[j].codecs = &dlc[1];
		links[j].platforms = &dlc[2];

		links[j].num_cpus = 1;
		links[j].num_codecs = 1;
		links[j].num_platforms = 1;

		links[j].id = i;
		links[j].no_pcm = 1;
		links[j].cpus->dai_name = drv[dai_id].name;
		links[j].platforms->name = dev_name(dev->parent);
		links[j].codecs->dai_name = "snd-soc-dummy-dai";
		links[j].codecs->name = "snd-soc-dummy";
		if (drv[dai_id].playback.channels_min)
			links[j].dpcm_playback = 1;
		if (drv[dai_id].capture.channels_min)
			links[j].dpcm_capture = 1;

		links[j].be_hw_params_fixup = sof_pcm_dai_link_fixup;

		j++;
	}

	card->dai_link = links;
	card->num_links = link_num - 1;

	return 0;
}

static int sof_nocodec_setup(struct device *dev,
			     u32 num_dai_drivers,
			     struct snd_soc_dai_driver *dai_drivers)
{
	struct snd_soc_dai_link *links;

	/* create dummy BE dai_links */
	links = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link) * num_dai_drivers, GFP_KERNEL);
	if (!links)
		return -ENOMEM;

	return sof_nocodec_bes_setup(dev, dai_drivers, links, num_dai_drivers, &sof_nocodec_card);
}

static int sof_nocodec_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &sof_nocodec_card;
	struct snd_soc_acpi_mach *mach;
	int ret;

	card->dev = &pdev->dev;
	card->topology_shortname_created = true;
	mach = pdev->dev.platform_data;

	ret = sof_nocodec_setup(card->dev, mach->mach_params.num_dai_drivers,
				mach->mach_params.dai_drivers);
	if (ret < 0)
		return ret;

	return devm_snd_soc_register_card(&pdev->dev, card);
}

static int sof_nocodec_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sof_nocodec_audio = {
	.probe = sof_nocodec_probe,
	.remove = sof_nocodec_remove,
	.driver = {
		.name = "sof-nocodec",
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(sof_nocodec_audio)

MODULE_DESCRIPTION("ASoC sof nocodec");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:sof-nocodec");
