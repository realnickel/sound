/*
 * Copyright (C) 2013-15, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_SND_SOC_ACPI_H
#define __LINUX_SND_SOC_ACPI_H

#include <linux/stddef.h>
#include <linux/acpi.h>

struct snd_soc_acpi_package_context {
	char *name;           /* package name */
	int length;           /* number of elements */
	struct acpi_buffer *format;
	struct acpi_buffer *state;
	bool data_valid;
};

#if IS_ENABLED(CONFIG_ACPI)
/* translation fron HID to I2C name, needed for DAI codec_name */
const char *snd_soc_acpi_find_name_from_hid(const u8 hid[ACPI_ID_LEN]);
bool snd_soc_acpi_find_package_from_hid(const u8 hid[ACPI_ID_LEN],
				    struct snd_soc_acpi_package_context *ctx);
#else
static inline const char *
snd_soc_acpi_find_name_from_hid(const u8 hid[ACPI_ID_LEN])
{
	return NULL;
}
static inline bool
snd_soc_acpi_find_package_from_hid(const u8 hid[ACPI_ID_LEN],
				   struct snd_soc_acpi_package_context *ctx)
{
	return false;
}
#endif

/* acpi match */
struct snd_soc_acpi_mach *
snd_soc_acpi_find_machine(struct snd_soc_acpi_mach *machines);

/* acpi check hid */
bool snd_soc_acpi_check_hid(const u8 hid[ACPI_ID_LEN]);

/* Descriptor for SST ASoC machine driver */
struct snd_soc_acpi_mach {
	/* ACPI ID for the matching machine driver. Audio codec for instance */
	const u8 id[ACPI_ID_LEN];
	/* machine driver name */
	const char *drv_name;
	/* firmware file name */
	const char *fw_filename;

	/* board name */
	const char *board;
	struct snd_soc_acpi_mach * (*machine_quirk)(void *arg);
	const void *quirk_data;
	void *pdata;
};

#define SND_SOC_ACPI_MAX_CODECS 3

/**
 * struct snd_soc_acpi_codecs: Structure to hold secondary codec information
 * apart from the matched one, this data will be passed to the quirk function
 * to match with the ACPI detected devices
 *
 * @num_codecs: number of secondary codecs used in the platform
 * @codecs: holds the codec IDs
 *
 */
struct snd_soc_acpi_codecs {
	int num_codecs;
	u8 codecs[SND_SOC_ACPI_MAX_CODECS][ACPI_ID_LEN];
};

/* check all codecs */
struct snd_soc_acpi_mach *snd_soc_acpi_codec_list(void *arg);

#endif
