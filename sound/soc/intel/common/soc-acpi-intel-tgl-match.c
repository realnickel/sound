// SPDX-License-Identifier: GPL-2.0
/*
 * soc-apci-intel-tgl-match.c - tables and support for ICL ACPI enumeration.
 *
 * Copyright (c) 2019, Intel Corporation.
 *
 */

#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>

static const u64 rt711_0_adr[] = {
	0x000010025D071100
};

static const struct snd_soc_acpi_link tgl_rvp[] = {
	{
		.mask = BIT(0),
		.dev_num = ARRAY_SIZE(rt711_0_adr),
		.adr = rt711_0_adr,
	},
	{}
};

struct snd_soc_acpi_mach snd_soc_acpi_intel_tgl_machines[] = {
	{
		.id = "10EC1308",
		.drv_name = "rt711_rt1308",
		.link_mask = 0x1, /* RT711 on SoundWire link0 */
		.links = tgl_rvp,
		.sof_fw_filename = "sof-tgl.ri",
		.sof_tplg_filename = "sof-tgl-rt711-rt1308.tplg",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_tgl_machines);

/* this table is used when there is no I2S codec present */
struct snd_soc_acpi_mach snd_soc_acpi_intel_tgl_sdw_machines[] = {
	{
		.link_mask = 0x1, /* this will only enable rt711 for now */
		.links = tgl_rvp,
		.drv_name = "sdw_rt711_rt1308_rt715",
		.sof_fw_filename = "sof-tgl.ri",
		.sof_tplg_filename = "sof-tgl-rt711.tplg",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_tgl_sdw_machines);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Common ACPI Match module");
