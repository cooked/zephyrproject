/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr.h>
#include <logging/log.h>

#include <drivers/gpio.h>
#include <drivers/sensor.h>

#include "ixm42xxx.h"

LOG_MODULE_REGISTER(application_test, LOG_LEVEL_DBG);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   100

// The devicetree node identifier for the "led1" alias (green led)
#define LEDG_NODE DT_ALIAS(led1)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LEDG_NODE, gpios);

// from Kev code
// static K_SEM_DEFINE(sensor_data_ready, 0, 1);

void main(void)
{
	int ret;

	if (!device_is_ready(led.port))
		return;

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
		return;

	const struct device *iim42652 = device_get_binding("IIM42652");
	// poteto-potato - another way to get a hold on the sensor
	//const char *const label = DT_LABEL(DT_INST(0, invensense_iim42652));
	//const struct device *iim42652 = device_get_binding(label);

	if (!iim42652) {
		LOG_ERR("Failed to get device binding for IIM42652");
		return;
	}

	struct sensor_value temp, accel[3], gyro[3];
	float q[4] = {1.0f, 0.0, 0.0, 0.0};

	while(1) {
		//ret = gpio_pin_toggle_dt(&led);
		if( gpio_pin_toggle(led.port, led.pin) <0 )
			return;

		ret = sensor_sample_fetch(iim42652);
		ret = sensor_channel_get(iim42652, SENSOR_CHAN_DIE_TEMP, &temp);
		ret = sensor_channel_get(iim42652, SENSOR_CHAN_ACCEL_XYZ, accel);
		ret = sensor_channel_get(iim42652, SENSOR_CHAN_GYRO_XYZ, gyro);

		//ixm42xxx_quaternions(&accel, &gyro, &q, SLEEP_TIME_MS/1000);

		printk("%f, %f, %f, %f, %f, %f\n",
			//sensor_value_to_double(&temp),
			sensor_value_to_double(&accel[0]),
			sensor_value_to_double(&accel[1]),
			sensor_value_to_double(&accel[2]),
			sensor_value_to_double(&gyro[0]),
			sensor_value_to_double(&gyro[1]),
			sensor_value_to_double(&gyro[2])
		);

		k_msleep(SLEEP_TIME_MS);
	}
}
