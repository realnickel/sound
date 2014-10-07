/*
 * property.h - Unified device property interface.
 *
 * Copyright (C) 2014, Intel Corporation
 * Authors: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_PROPERTY_H_
#define _LINUX_PROPERTY_H_

#include <linux/device.h>

enum dev_prop_type {
	DEV_PROP_U8,
	DEV_PROP_U16,
	DEV_PROP_U32,
	DEV_PROP_U64,
	DEV_PROP_STRING,
	DEV_PROP_MAX,
};

struct fw_dev_node {
	struct device_node *of_node;
	struct acpi_device *acpi_node;
};

int dev_node_get_property(struct fw_dev_node *fdn, const char *propname,
			  void **valptr);
int dev_node_read_property(struct fw_dev_node *fdn, const char *propname,
			   enum dev_prop_type proptype, void *val);
int dev_node_read_property_array(struct fw_dev_node *fdn, const char *propname,
				 enum dev_prop_type proptype, void *val,
				 size_t nval);
int device_get_property(struct device *dev, const char *propname,
			void **valptr);
int device_read_property(struct device *dev, const char *propname,
			 enum dev_prop_type proptype, void *val);
int device_read_property_array(struct device *dev, const char *propname,
			       enum dev_prop_type proptype, void *val,
			       size_t nval);
int device_for_each_child_node(struct device *dev,
			       int (*fn)(struct fw_dev_node *fdn, void *data),
			       void *data);
int device_get_child_node_count(struct device *dev);

static inline int dev_node_property_read_u8(struct fw_dev_node *fdn,
					    const char *propname, u8 *out_value)
{
	return dev_node_read_property(fdn, propname, DEV_PROP_U8, out_value);
}

static inline int dev_node_property_read_u16(struct fw_dev_node *fdn,
					     const char *propname,
					     u16 *out_value)
{
	return dev_node_read_property(fdn, propname, DEV_PROP_U16, out_value);
}

static inline int dev_node_property_read_u32(struct fw_dev_node *fdn,
					     const char *propname,
					     u32 *out_value)
{
	return dev_node_read_property(fdn, propname, DEV_PROP_U32, out_value);
}

static inline int dev_node_property_read_u64(struct fw_dev_node *fdn,
					     const char *propname,
					     u64 *out_value)
{
	return dev_node_read_property(fdn, propname, DEV_PROP_U64, out_value);
}

static inline int dev_node_property_read_u8_array(struct fw_dev_node *fdn,
						  const char *propname,
						  u8 *val, size_t nval)
{
	return dev_node_read_property_array(fdn, propname, DEV_PROP_U8, val, nval);
}

static inline int dev_node_property_read_u16_array(struct fw_dev_node *fdn,
						   const char *propname,
						   u16 *val, size_t nval)
{
	return dev_node_read_property_array(fdn, propname, DEV_PROP_U16, val, nval);
}

static inline int dev_node_property_read_u32_array(struct fw_dev_node *fdn,
						   const char *propname,
						   u32 *val, size_t nval)
{
	return dev_node_read_property_array(fdn, propname, DEV_PROP_U32, val, nval);
}

static inline int dev_node_property_read_u64_array(struct fw_dev_node *fdn,
						   const char *propname,
						   u64 *val, size_t nval)
{
	return dev_node_read_property_array(fdn, propname, DEV_PROP_U64, val, nval);
}

static inline int dev_node_property_read_string(struct fw_dev_node *fdn,
						const char *propname,
						const char **out_string)
{
	return dev_node_read_property(fdn, propname, DEV_PROP_STRING, out_string);
}

static inline int dev_node_property_read_string_array(struct fw_dev_node *fdn,
						      const char *propname,
						      const char **out_strings,
						      size_t nstrings)
{
	return dev_node_read_property_array(fdn, propname, DEV_PROP_STRING,
					    out_strings, nstrings);
}

static inline int device_property_read_u8(struct device *dev,
					  const char *propname, u8 *out_value)
{
	return device_read_property(dev, propname, DEV_PROP_U8, out_value);
}

static inline int device_property_read_u16(struct device *dev,
					  const char *propname, u16 *out_value)
{
	return device_read_property(dev, propname, DEV_PROP_U16, out_value);
}

static inline int device_property_read_u32(struct device *dev,
					  const char *propname, u32 *out_value)
{
	return device_read_property(dev, propname, DEV_PROP_U32, out_value);
}

static inline int device_property_read_u64(struct device *dev,
					  const char *propname, u64 *out_value)
{
	return device_read_property(dev, propname, DEV_PROP_U64, out_value);
}

static inline int device_property_read_u8_array(struct device *dev,
						const char *propname,
						u8 *val, size_t nval)
{
	return device_read_property_array(dev, propname, DEV_PROP_U8, val,
					  nval);
}

static inline int device_property_read_u16_array(struct device *dev,
						 const char *propname,
						 u16 *val, size_t nval)
{
	return device_read_property_array(dev, propname, DEV_PROP_U16, val,
					  nval);
}

static inline int device_property_read_u32_array(struct device *dev,
						 const char *propname,
						 u32 *val, size_t nval)
{
	return device_read_property_array(dev, propname, DEV_PROP_U32, val,
					  nval);
}

static inline int device_property_read_u64_array(struct device *dev,
						 const char *propname,
						 u64 *val, size_t nval)
{
	return device_read_property_array(dev, propname, DEV_PROP_U64, val,
					  nval);
}

static inline int device_property_read_string(struct device *dev,
					      const char *propname,
					      const char **out_string)
{
	return device_read_property(dev, propname, DEV_PROP_STRING, out_string);
}

static inline int device_property_read_string_array(struct device *dev,
						    const char *propname,
						    const char **out_strings,
						    size_t nstrings)
{
	return device_read_property_array(dev, propname, DEV_PROP_STRING,
					  out_strings, nstrings);
}
#endif /* _LINUX_PROPERTY_H_ */
