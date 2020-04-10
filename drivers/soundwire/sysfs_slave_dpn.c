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
 * DP-N properties
 */

#define to_dpn_prop(a, b) (a).b

#define sdw_dpn_attr(N, dir, field, format_string)			\
static ssize_t								\
dp##N##_##dir##_##field##_show(struct device *dev,			\
			       struct device_attribute *attr,		\
			       char *buf)				\
{									\
	struct sdw_slave *slave = dev_to_sdw_dev(dev);			\
	struct sdw_dpn_prop *dpn =					\
			to_dpn_prop(slave->prop, dir##_dpn_prop);	\
	unsigned long mask;						\
	int bit;							\
	int i;								\
									\
	/* convert src to source */					\
	if (dpn == slave->prop.src_dpn_prop)				\
		mask = slave->prop.source_ports;			\
	else								\
		mask = slave->prop.sink_ports;				\
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
static DEVICE_ATTR_RO(dp##N##_##dir##_##field);				\

#define	DPN_ATTR(N, dir)						\
	sdw_dpn_attr(N, dir, max_word, "%d\n")				\
	sdw_dpn_attr(N, dir, min_word, "%d\n")				\
	sdw_dpn_attr(N, dir, max_grouping, "%d\n")			\
	sdw_dpn_attr(N, dir, imp_def_interrupts, "%d\n")		\
	sdw_dpn_attr(N, dir, max_ch, "%d\n")				\
	sdw_dpn_attr(N, dir, min_ch, "%d\n")				\
	sdw_dpn_attr(N, dir, modes, "%d\n")				\
	sdw_dpn_attr(N, dir, max_async_buffer, "%d\n")			\
	sdw_dpn_attr(N, dir, block_pack_mode, "%d\n")			\
	sdw_dpn_attr(N, dir, port_encoding, "%d\n")			\
	sdw_dpn_attr(N, dir, simple_ch_prep_sm, "%d\n")			\
	sdw_dpn_attr(N, dir, ch_prep_timeout, "%d\n")			\
									\
static struct attribute							\
* dp##N##_##dir##_attrs[] = {						\
	&dev_attr_dp##N##_##dir##_max_word.attr,			\
	&dev_attr_dp##N##_##dir##_min_word.attr,			\
	&dev_attr_dp##N##_##dir##_max_grouping.attr,			\
	&dev_attr_dp##N##_##dir##_imp_def_interrupts.attr,		\
	&dev_attr_dp##N##_##dir##_max_ch.attr,				\
	&dev_attr_dp##N##_##dir##_min_ch.attr,				\
	&dev_attr_dp##N##_##dir##_modes.attr,				\
	&dev_attr_dp##N##_##dir##_max_async_buffer.attr,		\
	&dev_attr_dp##N##_##dir##_block_pack_mode.attr,			\
	&dev_attr_dp##N##_##dir##_port_encoding.attr,			\
	&dev_attr_dp##N##_##dir##_simple_ch_prep_sm.attr,		\
	&dev_attr_dp##N##_##dir##_ch_prep_timeout.attr,			\
	NULL,								\
};									\
									\
static const struct attribute_group					\
dp##N##_##dir##_group = {						\
	.attrs = dp##N##_##dir##_attrs,					\
	.name = "dp" #N "_" #dir,					\
};									\

DPN_ATTR(1, src)
DPN_ATTR(2, src)
DPN_ATTR(3, src)
DPN_ATTR(4, src)
DPN_ATTR(5, src)
DPN_ATTR(6, src)
DPN_ATTR(7, src)
DPN_ATTR(8, src)
DPN_ATTR(9, src)
DPN_ATTR(10, src)
DPN_ATTR(11, src)
DPN_ATTR(12, src)
DPN_ATTR(13, src)
DPN_ATTR(14, src)

DPN_ATTR(1, sink)
DPN_ATTR(2, sink)
DPN_ATTR(3, sink)
DPN_ATTR(4, sink)
DPN_ATTR(5, sink)
DPN_ATTR(6, sink)
DPN_ATTR(7, sink)
DPN_ATTR(8, sink)
DPN_ATTR(9, sink)
DPN_ATTR(10, sink)
DPN_ATTR(11, sink)
DPN_ATTR(12, sink)
DPN_ATTR(13, sink)
DPN_ATTR(14, sink)

static const struct attribute_group *dp_sink_group_array[] = {
	&dp1_sink_group,
	&dp2_sink_group,
	&dp3_sink_group,
	&dp4_sink_group,
	&dp5_sink_group,
	&dp6_sink_group,
	&dp7_sink_group,
	&dp8_sink_group,
	&dp9_sink_group,
	&dp10_sink_group,
	&dp11_sink_group,
	&dp12_sink_group,
	&dp13_sink_group,
	&dp14_sink_group,
};

static const struct attribute_group *dp_src_group_array[] = {
	&dp1_src_group,
	&dp2_src_group,
	&dp3_src_group,
	&dp4_src_group,
	&dp5_src_group,
	&dp6_src_group,
	&dp7_src_group,
	&dp8_src_group,
	&dp9_src_group,
	&dp10_src_group,
	&dp11_src_group,
	&dp12_src_group,
	&dp13_src_group,
	&dp14_src_group,
};

int sdw_slave_sysfs_dpn_init(struct sdw_slave *slave,
			     int i,
			     bool src)
{
	int ret;

	if (!SDW_VALID_PORT_RANGE(i))
		return -EINVAL;

	if (src)
		ret = devm_device_add_group(&slave->dev,
					    dp_src_group_array[i - 1]);
	else
		ret = devm_device_add_group(&slave->dev,
					    dp_sink_group_array[i - 1]);

	return ret;
}
