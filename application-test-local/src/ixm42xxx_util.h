/*
 * Copyright (c) 2020 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IXM42XXX_UTIL_H_
#define ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IXM42XXX_SETUP_H_

#include <drivers/sensor.h>

static void ixm42xxx_convert_accel(struct sensor_value *val, int16_t raw_val, uint16_t sensitivity_shift);
static void ixm42xxx_convert_gyro(struct sensor_value *val, int16_t raw_val, uint16_t sensitivity_x10);
static inline void ixm42xxx_convert_temp(struct sensor_value *val, int16_t raw_val);

static int ixm42xxx_attr_set(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, const struct sensor_value *val);
static int ixm42xxx_attr_get(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, struct sensor_value *val);

#endif /* __SENSOR_IXM42XXX_UTIL__ */
