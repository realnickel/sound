/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* Copyright(c) 2020 Intel Corporation. */

#ifndef __UAOL_INTEL_H
#define __UAOL_INTEL_H

#define UAOL_LINK_TYPE 5
#define UAOL_MAX_LINKS 2

/**
 * struct uaol_intel_ctrl_info - information for specific xHCI controller
 * @instance_number: identified, needs to be lower than "uaol-ctrl-count" property
 * @controller_identifier: value reported in xHCI Extended Capabilities
 */
struct uaol_intel_ctrl_info {
	u32 instance_number;
	u32 controller_identifier;
};

/**
 * struct uaol_intel_acpi_info - UAOL Intel information found in ACPI tables
 * @handle: ACPI controller handle
 * @count: link count found with "uaol-ctrl-count" property
 * @ctrl_info: detailed description for each xHCI connection
 *
 * this structure could be expanded to e.g. provide all the _ADR
 * information in case the link_mask is not sufficient to identify
 * platform capabilities.
 */
struct uaol_intel_acpi_info {
	acpi_handle handle;
	int count;
	struct uaol_intel_ctrl_info ctrl_info[UAOL_MAX_LINKS];
};

int uaol_intel_acpi_scan(acpi_handle *parent_handle,
			 struct uaol_intel_acpi_info *info);

#endif
