// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
// Copyright(c) 2015-19 Intel Corporation.

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include "bus.h"
#include "sysfs_local.h"

/*
 * DP0 sysfs
 */

#define to_sdw_dp0(_dev) \
	container_of(_dev, struct sdw_dp0_sysfs, dev)

#define sdw_dp0_attr(field, format_string)				\
static ssize_t field##_show(struct device *dev,				\
			    struct device_attribute *attr,		\
			    char *buf)					\
{									\
	struct sdw_dp0_sysfs *sysfs = to_sdw_dp0(dev);			\
	return sprintf(buf, format_string, sysfs->dp0_prop->field);	\
}									\
static DEVICE_ATTR_RO(field)

sdw_dp0_attr(min_word, "%d\n");
sdw_dp0_attr(max_word, "%d\n");
sdw_dp0_attr(BRA_flow_controlled, "%d\n");
sdw_dp0_attr(simple_ch_prep_sm, "%d\n");
sdw_dp0_attr(imp_def_interrupts, "0x%x\n");

static ssize_t word_bits_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct sdw_dp0_sysfs *sysfs = to_sdw_dp0(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < sysfs->dp0_prop->num_words; i++)
		size += sprintf(buf + size, "%d ", sysfs->dp0_prop->words[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(word_bits);

static struct attribute *dp0_attrs[] = {
	&dev_attr_min_word.attr,
	&dev_attr_max_word.attr,
	&dev_attr_BRA_flow_controlled.attr,
	&dev_attr_simple_ch_prep_sm.attr,
	&dev_attr_imp_def_interrupts.attr,
	&dev_attr_word_bits.attr,
	NULL,
};

static const struct attribute_group dp0_group = {
	.attrs = dp0_attrs,
};

static const struct attribute_group *dp0_groups[] = {
	&dp0_group,
	NULL
};

static void sdw_dp0_release(struct device *dev)
{
	struct sdw_dp0_sysfs *sysfs = to_sdw_dp0(dev);

	kfree(sysfs);
}

struct device_type sdw_dp0_type = {
	.name =	"sdw_dp0",
	.release = sdw_dp0_release,
};

struct sdw_dp0_sysfs
*sdw_sysfs_slave_dp0_init(struct sdw_slave *slave,
			  struct sdw_dp0_prop *prop)
{
	struct sdw_dp0_sysfs *dp0;
	int err;

	dp0 = kzalloc(sizeof(*dp0), GFP_KERNEL);
	if (!dp0)
		return NULL;

	dp0->dev.type = &sdw_dp0_type;
	dp0->dev.parent = &slave->dev;
	dp0->dev.groups = dp0_groups;
	dev_set_name(&dp0->dev, "dp0");
	dp0->dp0_prop = prop;

	err = device_register(&dp0->dev);
	if (err) {
		put_device(&dp0->dev);
		dp0 = NULL;
	}

	return dp0;
}

void sdw_sysfs_slave_dp0_exit(struct sdw_slave_sysfs *sysfs)
{
	if (sysfs->dp0)
		put_device(&sysfs->dp0->dev);
}
