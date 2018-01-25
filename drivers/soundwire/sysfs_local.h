/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* Copyright(c) 2015-19 Intel Corporation. */

#ifndef __SDW_SYSFS_LOCAL_H
#define __SDW_SYSFS_LOCAL_H

struct sdw_dp0_sysfs {
	struct device dev;
	struct sdw_dp0_prop *dp0_prop;
};

extern struct device_type sdw_dp0_type;

struct sdw_dp0_sysfs
*sdw_sysfs_slave_dp0_init(struct sdw_slave *slave,
			  struct sdw_dp0_prop *prop);

void sdw_sysfs_slave_dp0_exit(struct sdw_slave_sysfs *sysfs);

struct sdw_dpn_sysfs {
	struct device dev;
	struct sdw_dpn_prop *dpn_prop;
};

extern struct device_type sdw_dpn_type;

struct sdw_dpn_sysfs
*sdw_sysfs_slave_dpn_init(struct sdw_slave *slave,
			  struct sdw_dpn_prop *prop,
			  bool src);

void sdw_sysfs_slave_dpn_exit(struct sdw_slave_sysfs *sysfs);

struct sdw_slave_sysfs {
	struct sdw_slave *slave;
	struct sdw_dp0_sysfs *dp0;
	unsigned int num_dpns;
	struct sdw_dpn_sysfs **dpns;

};

#endif /* __SDW_SYSFS_LOCAL_H */
