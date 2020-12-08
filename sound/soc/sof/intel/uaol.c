// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
// Copyright(c) 2020 Intel Corporation.

/*
 * UAOL Routines
 *
 * Initializes and creates UAOL (USB Sideband Audio) devices based on ACPI and Hardware values
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/module.h>
#include "uaol.h"

static bool read_link_properties(struct fwnode_handle *fw_node,
				 struct uaol_intel_acpi_info *acpi_info,
				 int i)
{
	struct fwnode_handle *link;
	char name[32];
	u32 instance_number = 0;
	u32 controller_identifier = 0;

	/* Find descriptor subproperties */
	snprintf(name, sizeof(name),
		 "uaol-descriptor-%d", i);

	link = fwnode_get_named_child_node(fw_node, name);
	if (!link)
		return false;

	fwnode_property_read_u32(link,
				 "uaol-instance-number",
				 &instance_number);

	if (instance_number > acpi_info->count)
		return false;

	acpi_info->ctrl_info[i].instance_number = instance_number;

	fwnode_property_read_u32(link,
				 "peer-integrated-controller-identifier",
				 &controller_identifier);

	if (!controller_identifier)
		return false;

	/*
	 * we don't have a means to check the value at this stage, it will be verified
	 * later in the actual connection with the xHCI driver
	 */
	acpi_info->ctrl_info[i].controller_identifier = controller_identifier;

	return true;
}

static int
uaol_intel_scan_controller(struct uaol_intel_acpi_info *info)
{
	struct acpi_device *adev;
	int ret, i;
	u8 count;

	if (acpi_bus_get_device(info->handle, &adev))
		return -EINVAL;

	/* Found controller, find links supported */
	count = 0;
	ret = fwnode_property_read_u8_array(acpi_fwnode_handle(adev),
					    "uaol-ctrl-count", &count, 1);

	if (ret) {
		dev_err(&adev->dev,
			"Failed to read uaol-ctrl-count: %d\n", ret);
		return -EINVAL;
	}

	/*
	 * In theory we could check the number of links supported in
	 * hardware, but that information is available to the DSP
	 * firmware only, so this will be verified in a later
	 * step. For now only do basic sanity check.
	 */

	/* Check count is within bounds */
	if (count > UAOL_MAX_LINKS) {
		dev_err(&adev->dev, "Link count %d exceeds max %d\n",
			count, UAOL_MAX_LINKS);
		return -EINVAL;
	}

	if (!count) {
		dev_warn(&adev->dev, "No UAOL links detected\n");
		return -EINVAL;
	}
	dev_dbg(&adev->dev, "ACPI reports %d Audio Sideband links with xHCI controllers\n", count);

	info->count = count;

	for (i = 0; i < count; i++) {

		if (!read_link_properties(acpi_fwnode_handle(adev),
					  info,
					  i))
			continue;

		dev_dbg(&adev->dev, "UAOL instance %d identifier 0x%x\n",
			info->ctrl_info[i].instance_number,
			info->ctrl_info[i].controller_identifier);
	}
	return 0;
}

static acpi_status uaol_intel_acpi_cb(acpi_handle handle, u32 level,
				      void *cdata, void **return_value)
{
	struct uaol_intel_acpi_info *info = cdata;
	struct acpi_device *adev;
	acpi_status status;
	u64 adr;

	status = acpi_evaluate_integer(handle, METHOD_NAME__ADR, NULL, &adr);
	if (ACPI_FAILURE(status))
		return AE_OK; /* keep going */

	if (acpi_bus_get_device(handle, &adev)) {
		pr_err("%s: Couldn't find ACPI handle\n", __func__);
		return AE_NOT_FOUND;
	}

	info->handle = handle;

	/*
	 * On some Intel platforms, multiple children of the HDAS
	 * device can be found, but only one of them is the UAOL
	 * controller. The UAOL device is always exposed with
	 * Name(_ADR, 0x50000000), with bits 31..28 representing the
	 * UAOL link so filter accordingly
	 */
	if (FIELD_GET(GENMASK(31, 28), adr) != UAOL_LINK_TYPE)
		return AE_OK; /* keep going */

	/* device found, stop namespace walk */
	return AE_CTRL_TERMINATE;
}

/**
 * uaol_intel_acpi_scan() - USB offload capabilities detection
 * @parent_handle: ACPI parent handle
 * @info: description of what firmware/DSDT tables expose
 *
 * This scans the namespace and queries firmware to figure out if any
 * xHCI connections are possible with the Audio Sideband capability.
 */
int uaol_intel_acpi_scan(acpi_handle *parent_handle,
			 struct uaol_intel_acpi_info *info)
{
	acpi_status status;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE,
				     parent_handle, 1,
				     uaol_intel_acpi_cb,
				     NULL, info, NULL);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	return uaol_intel_scan_controller(info);
}
EXPORT_SYMBOL_NS(uaol_intel_acpi_scan, UAOL_INTEL);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel USB Sideband Audio");
