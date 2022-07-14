/*
 * Copyright (c) 2020 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IIM42652_H_
#define ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IIM42652_H_

#include <device.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/spi.h>
#include <sys/util.h>
#include <zephyr/types.h>

#include "ixm42xxx_reg.h"

// TODO add support for triggers

// TODO check if valid for any device of the family
enum {
	GYRO_FS_2000DPS = 0,
	GYRO_FS_1000DPS,
	GYRO_FS_500DPS,
	GYRO_FS_250DPS,
	GYRO_FS_125DPS,
	GYRO_FS_62DPS,
	GYRO_FS_32DPS,
	GYRO_FS_15DPS,
};

enum {
	ACCEL_FS_16G = 0,
	ACCEL_FS_8G,
	ACCEL_FS_4G,
	ACCEL_FS_2G,
};

struct ixm42xxx_data {
	// TODO keep one, call it *bus, and generalize all code
	const struct device *spi;
	const struct device *i2c;

	uint8_t fifo_data[HARDWARE_FIFO_SIZE];

	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	uint16_t accel_sensitivity_shift;
	uint16_t accel_hz;
	uint16_t accel_sf;

	int16_t temp;

	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
	uint16_t gyro_sensitivity_x10;
	uint16_t gyro_hz;
	uint16_t gyro_sf;

	bool accel_en;
	bool gyro_en;
	bool tap_en;

	bool sensor_started;

	const struct device *dev;
	const struct device *gpio;
	struct gpio_callback gpio_cb;

	// TODO: study and understand implications
	//K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_IXM42XXX_THREAD_STACK_SIZE);
	struct k_thread thread;
	struct k_sem gpio_sem;

	//struct spi_cs_control spi_cs;
	struct spi_config spi_cfg;
};

struct ixm42xxx_config {

	// TODO keep one, call it *bus_label, and generalize all code
	// NEW: extended to support spi and i2c
	const char *i2c_label;	// one label or the other will be used
	const char *spi_label;
	uint16_t addr;			// this is i2c or spi address, depending on sensor selection

	uint32_t frequency;
	uint32_t slave;
	uint8_t int_pin;
	uint8_t int_flags;
	const char *int_label;
	const char *gpio_label;
	gpio_pin_t gpio_pin;
	gpio_dt_flags_t gpio_dt_flags;
	uint16_t accel_hz;
	uint16_t gyro_hz;
	uint16_t accel_fs;
	uint16_t gyro_fs;
};

#endif /* __SENSOR_IIM42652__ */
