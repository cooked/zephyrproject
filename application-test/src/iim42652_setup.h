/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IIM42652_SETUP_H_
#define ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IIM42652_SETUP_H_

#include <device.h>

int iim42652_sensor_init(const struct device *dev);
/*int icm42605_turn_on_fifo(const struct device *dev);
int icm42605_turn_off_fifo(const struct device *dev);
int icm42605_turn_off_sensor(const struct device *dev);
int icm42605_turn_on_sensor(const struct device *dev);
int icm42605_set_odr(const struct device *dev, int a_rate, int g_rate);
*/

#endif /* __SENSOR_IXM42XXX_IIM42652_SETUP__ */
