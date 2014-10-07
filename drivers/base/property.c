/*
 * property.c - Unified device property interface.
 *
 * Copyright (C) 2014, Intel Corporation
 * Authors: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/property.h>
#include <linux/export.h>
#include <linux/acpi.h>
#include <linux/of.h>

/**
 * dev_node_get_property - return a raw property from device description
 * @fdn: Device node to get the property from
 * @propname: Name of the property
 * @valptr: The raw property value is stored here
 *
 * Function reads property @propname from the device firmware description and
 * stores the raw value into @valptr if found.  Otherwise returns a negative
 * errno as specified below.
 *
 * Return: %0 if the property was found (success),
 *	   %-EINVAL if given arguments are not valid,
 *	   %-ENODATA if the property does not exist.
 */
int dev_node_get_property(struct fw_dev_node *fdn, const char *propname,
			  void **valptr)
{
	if (IS_ENABLED(CONFIG_OF) && fdn->of_node)
		return of_dev_prop_get(fdn->of_node, propname, valptr);
	else if (IS_ENABLED(CONFIG_ACPI) && fdn->acpi_node)
		return acpi_dev_prop_get(fdn->acpi_node, propname, valptr);

	return -ENODATA;
}
EXPORT_SYMBOL_GPL(dev_node_get_property);

/**
 * device_get_property - return a raw property of a device
 * @dev: Device get the property of
 * @propname: Name of the property
 * @valptr: The raw property value is stored here
 */
int device_get_property(struct device *dev, const char *propname, void **valptr)
{
	struct fw_dev_node fdn = {
		.of_node = dev->of_node,
		.acpi_node = ACPI_COMPANION(dev),
	};
	return dev_node_get_property(&fdn, propname, valptr);
}
EXPORT_SYMBOL_GPL(device_get_property);

/**
 * dev_node_read_property - return a typed property from device description
 * @fdn: Device node to get the property from
 * @propname: Name of the property
 * @proptype: Type of the property
 * @val: The value is stored here
 *
 * Function reads property @propname from the device firmware description and
 * stores the value into @val if found. The value is checked to be of type
 * @proptype.
 *
 * Return: %0 if the property was found (success),
 *	   %-EINVAL if given arguments are not valid,
 *	   %-ENODATA if the property does not exist,
 *	   %-EPROTO if the property type does not match @proptype,
 *	   %-EOVERFLOW if the property value is out of bounds of @proptype.
 */
int dev_node_read_property(struct fw_dev_node *fdn, const char *propname,
			   enum dev_prop_type proptype, void *val)
{
	if (IS_ENABLED(CONFIG_OF) && fdn->of_node)
		return of_dev_prop_read(fdn->of_node, propname, proptype, val);
	else if (IS_ENABLED(CONFIG_ACPI) && fdn->acpi_node)
		return acpi_dev_prop_read(fdn->acpi_node, propname, proptype, val);

	return -ENODATA;
}
EXPORT_SYMBOL_GPL(dev_node_read_property);

/**
 * device_read_property - return a typed property of a device
 * @dev: Device to get the property of
 * @propname: Name of the property
 * @proptype: Type of the property
 * @val: The value is stored here
 */
int device_read_property(struct device *dev, const char *propname,
			 enum dev_prop_type proptype, void *val)
{
	struct fw_dev_node fdn = {
		.of_node = dev->of_node,
		.acpi_node = ACPI_COMPANION(dev),
	};
	return dev_node_read_property(&fdn, propname, proptype, val);
}
EXPORT_SYMBOL_GPL(device_read_property);

/**
 * dev_node_read_property_array - return an array property from a device
 * @fdn: Device node to get the property from
 * @propname: Name of the property
 * @proptype: Type of the property
 * @val: The values are stored here
 * @nval: Size of the @val array
 *
 * Function reads an array of properties with @propname from the device
 * firmware description and stores them to @val if found. All the values
 * in the array must be of type @proptype.
 *
 * Return: %0 if the property was found (success),
 *	   %-EINVAL if given arguments are not valid,
 *	   %-ENODATA if the property does not exist,
 *	   %-EPROTO if the property type does not match @proptype,
 *	   %-EOVERFLOW if the property value is out of bounds of @proptype.
 */
int dev_node_read_property_array(struct fw_dev_node *fdn, const char *propname,
				 enum dev_prop_type proptype, void *val,
				 size_t nval)
{
	if (IS_ENABLED(CONFIG_OF) && fdn->of_node)
		return of_dev_prop_read_array(fdn->of_node, propname, proptype,
					      val, nval);
	else if (IS_ENABLED(CONFIG_ACPI) && fdn->acpi_node)
		return acpi_dev_prop_read_array(fdn->acpi_node, propname,
						proptype, val, nval);

	return -ENODATA;
}
EXPORT_SYMBOL_GPL(dev_node_read_property_array);

/**
 * device_read_property_array - return an array property of a device
 * @dev: Device to get the property of
 * @propname: Name of the property
 * @proptype: Type of the property
 * @val: The values are stored here
 * @nval: Size of the @val array
 */
int device_read_property_array(struct device *dev, const char *propname,
			       enum dev_prop_type proptype, void *val,
			       size_t nval)
{
	struct fw_dev_node fdn = {
		.of_node = dev->of_node,
		.acpi_node = ACPI_COMPANION(dev),
	};
	return dev_node_read_property_array(&fdn, propname, proptype, val, nval);
}
EXPORT_SYMBOL_GPL(device_read_property_array);

/**
 * device_for_each_child_node - execute function for each child node of device
 * @dev: Device to run the function for
 * @fn: Function to run
 * @data: Additional data to pass to the function
 */
int device_for_each_child_node(struct device *dev,
			       int (*fn)(struct fw_dev_node *fdn, void *data),
			       void *data)
{
	if (IS_ENABLED(CONFIG_OF) && dev->of_node)
		return of_for_each_child_node(dev->of_node, fn, data);
	else if (ACPI_COMPANION(dev))
		return acpi_for_each_child_node(ACPI_COMPANION(dev), fn, data);

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(device_for_each_child_node);

static int dev_node_count(struct fw_dev_node *fdn, void *data)
{
	*((int *)data) += 1;
	return 0;
}

/**
 * device_get_child_node_count - return number of child nodes for this device
 * @dev: Device to get the child node count from
 */
int device_get_child_node_count(struct device *dev)
{
	int count = 0;

	device_for_each_child_node(dev, dev_node_count, &count);
	return count;
}
EXPORT_SYMBOL_GPL(device_get_child_node_count);
