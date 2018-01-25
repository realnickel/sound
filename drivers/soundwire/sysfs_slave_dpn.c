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
 * DP-N properties
 */

#define to_sdw_dpn(_dev) \
	container_of(_dev, struct sdw_dpn_sysfs, dev)

#define sdw_dpn_attr(field, format_string)				\
static ssize_t field##_show(struct device *dev,				\
			    struct device_attribute *attr,		\
			    char *buf)					\
{									\
	struct sdw_dpn_sysfs *sysfs = to_sdw_dpn(dev);			\
	return sprintf(buf, format_string, sysfs->dpn_prop->field);	\
}									\
static DEVICE_ATTR_RO(field)

sdw_dpn_attr(max_word, "%d\n");
sdw_dpn_attr(min_word, "%d\n");
sdw_dpn_attr(max_grouping, "%d\n");
sdw_dpn_attr(imp_def_interrupts, "%d\n");
sdw_dpn_attr(max_ch, "%d\n");
sdw_dpn_attr(min_ch, "%d\n");
sdw_dpn_attr(modes, "%d\n");
sdw_dpn_attr(max_async_buffer, "%d\n");
sdw_dpn_attr(block_pack_mode, "%d\n");
sdw_dpn_attr(port_encoding, "%d\n");
sdw_dpn_attr(simple_ch_prep_sm, "%d\n");
sdw_dpn_attr(ch_prep_timeout, "%d\n");

static ssize_t words_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct sdw_dpn_sysfs *sysfs = to_sdw_dpn(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < sysfs->dpn_prop->num_words; i++)
		size += sprintf(buf + size, "%d ", sysfs->dpn_prop->words[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(words);

static ssize_t channels_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct sdw_dpn_sysfs *sysfs = to_sdw_dpn(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < sysfs->dpn_prop->num_ch; i++)
		size += sprintf(buf + size, "%d ", sysfs->dpn_prop->ch[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(channels);

static ssize_t ch_combinations_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sdw_dpn_sysfs *sysfs = to_sdw_dpn(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < sysfs->dpn_prop->num_ch_combinations; i++)
		size += sprintf(buf + size, "%d ",
				sysfs->dpn_prop->ch_combinations[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(ch_combinations);

static struct attribute *dpn_attrs[] = {
	&dev_attr_max_word.attr,
	&dev_attr_min_word.attr,
	&dev_attr_max_grouping.attr,
	&dev_attr_imp_def_interrupts.attr,
	&dev_attr_max_ch.attr,
	&dev_attr_min_ch.attr,
	&dev_attr_modes.attr,
	&dev_attr_max_async_buffer.attr,
	&dev_attr_block_pack_mode.attr,
	&dev_attr_port_encoding.attr,
	&dev_attr_simple_ch_prep_sm.attr,
	&dev_attr_ch_prep_timeout.attr,
	&dev_attr_words.attr,
	&dev_attr_channels.attr,
	&dev_attr_ch_combinations.attr,
	NULL,
};

static const struct attribute_group dpn_group = {
	.attrs = dpn_attrs,
};

static const struct attribute_group *dpn_groups[] = {
	&dpn_group,
	NULL
};

static void sdw_dpn_release(struct device *dev)
{
	struct sdw_dpn_sysfs *sysfs = to_sdw_dpn(dev);

	kfree(sysfs);
}

struct device_type sdw_dpn_type = {
	.name =	"sdw_dpn",
	.release = sdw_dpn_release,
};

struct sdw_dpn_sysfs
*sdw_sysfs_slave_dpn_init(struct sdw_slave *slave,
			  struct sdw_dpn_prop *prop,
			  bool src)
{
	struct sdw_dpn_sysfs *dpn;
	int err;

	dpn = kzalloc(sizeof(*dpn), GFP_KERNEL);
	if (!dpn)
		return NULL;

	dpn->dev.type = &sdw_dpn_type;
	dpn->dev.parent = &slave->dev;
	dpn->dev.groups = dpn_groups;
	dpn->dpn_prop = prop;

	if (src)
		dev_set_name(&dpn->dev, "src-dp%x", prop->num);
	else
		dev_set_name(&dpn->dev, "sink-dp%x", prop->num);

	err = device_register(&dpn->dev);
	if (err) {
		put_device(&dpn->dev);
		dpn = NULL;
	}

	return dpn;
}

void sdw_sysfs_slave_dpn_exit(struct sdw_slave_sysfs *sysfs)
{
	int i;

	for (i = 0; i < sysfs->num_dpns; i++) {
		if (sysfs->dpns[i])
			put_device(&sysfs->dpns[i]->dev);
	}
}
