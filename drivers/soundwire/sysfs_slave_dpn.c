// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2015-2020 Intel Corporation.

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include "bus.h"
#include "sysfs_local.h"

/*
 * dpN tree sysfs
 *
 */

struct dpn_attribute;

struct dpn_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct device *dev, int N, int dir,
			struct dpn_attribute *attr, char *buf);
};

static int get_port_N_dir(struct kobject *kobj, int *N, int *dir,
			  struct slave **slv)
{
	struct device *dev = kobj_to_dev(kobj->parent->parent);
	char *filename;
	ssize_t ret;

	/* the filename is dp:<N>:<sink/src> */
	filename = strdup(kobj->name);
	if (!filename)
		return -ENOMEM;

	token = filename;
	end = filename;

	/* skip dp */
	token = strsep(&filename, ":");
	if (!token) {
		ret = -EINVAL;
		goto err;
	}

	/* extract N */
	token = strsep(&filename, ":");
	if (!token) {
		ret = -EINVAL;
		goto err;
	}

	ret = kstrtoint(token, 10, N);
	if (ret < 0)
		goto err;

	/* extract direction */
	token = strsep(&filename, ":");
	if (!token) {
		ret = -EINVAL;
		goto err;
	}
	if (!strcmp(token, "sink"))
		*dir = 0;
	else
		*dir = 1;

	*slv = dev_to_sdw_dev(dev);

err:
	free(filename);
	return ret;
}

static ssize_t dpn_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct dpn_attribute *dpn_attr =
		container_of(attr, struct dpn_attribute, attr);
	struct slave *slave;
	int N;
	int dir;

	if (!dpn_attr->show)
		return -EIO;
	ret = get_port_N_dir(kobj, &N, &dir, &codec);
	if (ret < 0)
		return ret;
	return dpn_attr->show(slave, N, dir, dpn_attr, buf);
}

static const struct sysfs_ops dpn_sysfs_ops = {
	.show	= dpn_attr_show,
};

static void dpn_release(struct kobject *kobj)
{
	kfree(kobj);
}

static struct kobj_type dpn_ktype = {
	.release	= dpn_release,
	.sysfs_ops	= &dpn_sysfs_ops,
};

#define sdw_dpn_attr(field, format_string)				\
									\
static ssize_t								\
dpn_##field##_show(struct slave *slave,					\
			int N,						\
			int dir,					\
			struct device_attribute *attr,			\
			char *buf)					\
{									\
	struct sdw_dpn_prop *dpn;					\
	unsigned long mask;						\
	int bit;							\
	int i;								\
									\
	/* convert src to source */					\
	if (dir) {							\
		dpn = slave->prop.src_dpn_prop;				\
		mask = slave->prop.source_ports;			\
	} else {							\
		dpn = slave->prop.sink_dpn_prop;			\
		mask = slave->prop.sink_ports;				\
	}								\
	i = 0;								\
	for_each_set_bit(bit, &mask, 32) {				\
		if (bit == N) {						\
			return sprintf(buf, format_string,		\
				       dpn[i].field);			\
		}							\
		i++;							\
	}								\
	return -EINVAL;							\
}									\
									\
static struct dpn_attribute dpn_attr_##_field = __ATTR_RO(_field);

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

static struct attribute dpn_attrs[] = {
	&dpn_attr_max_word.attr,
	&dpn_attr_min_word.attr,
	&dpn_attr_max_grouping.attr,
	&dpn_attr_imp_def_interrupts.attr,
	&dpn_attr_max_ch.attr,
	&dpn_attr_min_ch.attr,
	&dpn_attr_modes.attr,
	&dpn_attr_max_async_buffer.attr,
	&dpn_attr_block_pack_mode.attr,
	&dpn_attr_port_encoding.attr,
	&dpn_attr_simple_ch_prep_sm.attr,
	&dpn_attr_ch_prep_timeout.attr,
	NULL,
};

static const struct attribute_group dpn_group = {
	.attrs = dpn_attrs,
};

int sdw_slave_sysfs_dpn_init(struct sdw_slave *slave,
			     int i,
			     bool src)
{
	int ret;

	if (!SDW_VALID_PORT_RANGE(i))
		return -EINVAL;

	return ret;
}
