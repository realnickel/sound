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

struct sdw_master_sysfs {
	struct device dev;
	struct sdw_bus *bus;
};

#define to_sdw_device(_dev) \
	container_of(_dev, struct sdw_master_sysfs, dev)

/*
 * The sysfs for properties reflects the MIPI description as given
 * in the MIPI DisCo spec
 *
 * Base file is:
 *	sdw-master-N
 *      |---- revision
 *      |---- clk_stop_modes
 *      |---- max_clk_freq
 *      |---- clk_freq
 *      |---- clk_gears
 *      |---- default_row
 *      |---- default_col
 *      |---- default_frame_shape
 *      |---- dynamic_shape
 *      |---- err_threshold
 */

#define sdw_master_attr(field, format_string)				\
static ssize_t field##_show(struct device *dev,				\
			    struct device_attribute *attr,		\
			    char *buf)					\
{									\
	struct sdw_master_sysfs *master = to_sdw_device(dev);		\
	return sprintf(buf, format_string, master->bus->prop.field);	\
}									\
static DEVICE_ATTR_RO(field)

sdw_master_attr(revision, "0x%x\n");
sdw_master_attr(clk_stop_modes, "0x%x\n");
sdw_master_attr(max_clk_freq, "%d\n");
sdw_master_attr(default_row, "%d\n");
sdw_master_attr(default_col, "%d\n");
sdw_master_attr(default_frame_rate, "%d\n");
sdw_master_attr(dynamic_frame, "%d\n");
sdw_master_attr(err_threshold, "%d\n");

static ssize_t clock_frequencies_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sdw_master_sysfs *master = to_sdw_device(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < master->bus->prop.num_clk_freq; i++)
		size += sprintf(buf + size, "%8d ",
				master->bus->prop.clk_freq[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(clock_frequencies);

static ssize_t clock_gears_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sdw_master_sysfs *master = to_sdw_device(dev);
	ssize_t size = 0;
	int i;

	for (i = 0; i < master->bus->prop.num_clk_gears; i++)
		size += sprintf(buf + size, "%8d ",
				master->bus->prop.clk_gears[i]);
	size += sprintf(buf + size, "\n");

	return size;
}
static DEVICE_ATTR_RO(clock_gears);

static struct attribute *master_node_attrs[] = {
	&dev_attr_revision.attr,
	&dev_attr_clk_stop_modes.attr,
	&dev_attr_max_clk_freq.attr,
	&dev_attr_default_row.attr,
	&dev_attr_default_col.attr,
	&dev_attr_default_frame_rate.attr,
	&dev_attr_dynamic_frame.attr,
	&dev_attr_err_threshold.attr,
	&dev_attr_clock_frequencies.attr,
	&dev_attr_clock_gears.attr,
	NULL,
};

static const struct attribute_group sdw_master_node_group = {
	.attrs = master_node_attrs,
};

static const struct attribute_group *sdw_master_node_groups[] = {
	&sdw_master_node_group,
	NULL
};

static void sdw_device_release(struct device *dev)
{
	struct sdw_master_sysfs *master = to_sdw_device(dev);

	kfree(master);
}

static struct device_type sdw_device_type = {
	.name =	"sdw_device",
	.release = sdw_device_release,
};

int sdw_sysfs_bus_init(struct sdw_bus *bus)
{
	struct sdw_master_sysfs *master;
	int err;

	if (bus->sysfs) {
		dev_err(bus->dev, "SDW sysfs is already initialized\n");
		return -EIO;
	}

	master = kzalloc(sizeof(*master), GFP_KERNEL);
	if (!master)
		return -ENOMEM;

	bus->sysfs = master;
	master->bus = bus;
	master->dev.type = &sdw_device_type;
	master->dev.bus = &sdw_bus_type;
	master->dev.parent = bus->dev;
	master->dev.groups = sdw_master_node_groups;
	dev_set_name(&master->dev, "sdw-master:%x", bus->link_id);

	err = device_register(&master->dev);
	if (err)
		put_device(&master->dev);

	return err;
}

void sdw_sysfs_bus_exit(struct sdw_bus *bus)
{
	struct sdw_master_sysfs *master = bus->sysfs;

	if (!master)
		return;

	master->bus = NULL;
	put_device(&master->dev);
	bus->sysfs = NULL;
}

/*
 * Slave sysfs
 */

/*
 * The sysfs for Slave reflects the MIPI description as given
 * in the MIPI DisCo spec
 *
 * Base file is device
 *	|---- mipi_revision
 *	|---- wake_capable
 *	|---- test_mode_capable
 *	|---- simple_clk_stop_capable
 *	|---- clk_stop_timeout
 *	|---- ch_prep_timeout
 *	|---- reset_behave
 *	|---- high_PHY_capable
 *	|---- paging_support
 *	|---- bank_delay_support
 *	|---- p15_behave
 *	|---- master_count
 *	|---- source_ports
 *	|---- sink_ports
 *	|---- dp0
 *		|---- max_word
 *		|---- min_word
 *		|---- words
 *		|---- flow_controlled
 *		|---- simple_ch_prep_sm
 *		|---- device_interrupts
 *	|---- dpN
 *		|---- max_word
 *		|---- min_word
 *		|---- words
 *		|---- type
 *		|---- max_grouping
 *		|---- simple_ch_prep_sm
 *		|---- ch_prep_timeout
 *		|---- device_interrupts
 *		|---- max_ch
 *		|---- min_ch
 *		|---- ch
 *		|---- ch_combinations
 *		|---- modes
 *		|---- max_async_buffer
 *		|---- block_pack_mode
 *		|---- port_encoding
 *		|---- audio_modeM
 *				|---- bus_min_freq
 *				|---- bus_max_freq
 *				|---- bus_freq
 *				|---- max_freq
 *				|---- min_freq
 *				|---- freq
 *				|---- prep_ch_behave
 *				|---- glitchless
 *
 */

#define sdw_slave_attr(field, format_string)			\
static ssize_t field##_show(struct device *dev,			\
			    struct device_attribute *attr,	\
			    char *buf)				\
{								\
	struct sdw_slave *slave = dev_to_sdw_dev(dev);		\
	return sprintf(buf, format_string, slave->prop.field);	\
}								\
static DEVICE_ATTR_RO(field)

sdw_slave_attr(mipi_revision, "0x%x\n");
sdw_slave_attr(wake_capable, "%d\n");
sdw_slave_attr(test_mode_capable, "%d\n");
sdw_slave_attr(clk_stop_mode1, "%d\n");
sdw_slave_attr(simple_clk_stop_capable, "%d\n");
sdw_slave_attr(clk_stop_timeout, "%d\n");
sdw_slave_attr(ch_prep_timeout, "%d\n");
sdw_slave_attr(reset_behave, "%d\n");
sdw_slave_attr(high_PHY_capable, "%d\n");
sdw_slave_attr(paging_support, "%d\n");
sdw_slave_attr(bank_delay_support, "%d\n");
sdw_slave_attr(p15_behave, "%d\n");
sdw_slave_attr(master_count, "%d\n");
sdw_slave_attr(source_ports, "%d\n");
sdw_slave_attr(sink_ports, "%d\n");

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct sdw_slave *slave = dev_to_sdw_dev(dev);

	return sdw_slave_modalias(slave, buf, 256);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *slave_dev_attrs[] = {
	&dev_attr_mipi_revision.attr,
	&dev_attr_wake_capable.attr,
	&dev_attr_test_mode_capable.attr,
	&dev_attr_clk_stop_mode1.attr,
	&dev_attr_simple_clk_stop_capable.attr,
	&dev_attr_clk_stop_timeout.attr,
	&dev_attr_ch_prep_timeout.attr,
	&dev_attr_reset_behave.attr,
	&dev_attr_high_PHY_capable.attr,
	&dev_attr_paging_support.attr,
	&dev_attr_bank_delay_support.attr,
	&dev_attr_p15_behave.attr,
	&dev_attr_master_count.attr,
	&dev_attr_source_ports.attr,
	&dev_attr_sink_ports.attr,
	&dev_attr_modalias.attr,
	NULL,
};

static struct attribute_group sdw_slave_dev_attr_group = {
	.attrs	= slave_dev_attrs,
};

const struct attribute_group *sdw_slave_dev_attr_groups[] = {
	&sdw_slave_dev_attr_group,
	NULL
};

int sdw_sysfs_slave_init(struct sdw_slave *slave)
{
	struct sdw_slave_sysfs *sysfs;
	unsigned int src_dpns, sink_dpns, i, j;
	int err;

	if (slave->sysfs) {
		dev_err(&slave->dev, "SDW Slave sysfs is already initialized\n");
		err = -EIO;
		goto err_ret;
	}

	sysfs = kzalloc(sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs) {
		err = -ENOMEM;
		goto err_ret;
	}

	slave->sysfs = sysfs;
	sysfs->slave = slave;

	if (slave->prop.dp0_prop) {
		sysfs->dp0 = sdw_sysfs_slave_dp0_init(slave,
						      slave->prop.dp0_prop);
		if (!sysfs->dp0) {
			err = -ENOMEM;
			goto err_free_sysfs;
		}
	}

	src_dpns = hweight32(slave->prop.source_ports);
	sink_dpns = hweight32(slave->prop.sink_ports);
	sysfs->num_dpns = src_dpns + sink_dpns;

	sysfs->dpns = kcalloc(sysfs->num_dpns,
			      sizeof(**sysfs->dpns), GFP_KERNEL);
	if (!sysfs->dpns) {
		err = -ENOMEM;
		goto err_dp0;
	}

	for (i = 0; i < src_dpns; i++) {
		sysfs->dpns[i] =
			sdw_sysfs_slave_dpn_init(slave,
						 &slave->prop.src_dpn_prop[i],
						 true);
		if (!sysfs->dpns[i]) {
			err = -ENOMEM;
			goto err_dpn;
		}
	}

	for (j = 0; j < sink_dpns; j++) {
		sysfs->dpns[i + j] =
			sdw_sysfs_slave_dpn_init(slave,
						 &slave->prop.sink_dpn_prop[j],
						 false);
		if (!sysfs->dpns[i + j]) {
			err = -ENOMEM;
			goto err_dpn;
		}
	}
	return 0;

err_dpn:
	sdw_sysfs_slave_dpn_exit(sysfs);
err_dp0:
	sdw_sysfs_slave_dp0_exit(sysfs);
err_free_sysfs:
	kfree(sysfs);
	sysfs->slave = NULL;
err_ret:
	return err;
}

void sdw_sysfs_slave_exit(struct sdw_slave *slave)
{
	struct sdw_slave_sysfs *sysfs = slave->sysfs;

	if (!sysfs)
		return;

	sdw_sysfs_slave_dpn_exit(sysfs);
	kfree(sysfs->dpns);
	sdw_sysfs_slave_dp0_exit(sysfs);
	kfree(sysfs);
	slave->sysfs = NULL;
}
