/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include "ixm42xxx.h"
#include "iim42652_setup.h"


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

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
		//LOG_ERR("Failed to get device binding for IIM42652");
		return;
	}

	ret = iim42652_sensor_init(iim42652);
	//if (ret < 0)
	//	LOG_ERR("Failed to get WHO_AM_I from IIM42652 (%d)", ret);


	while(1) {
		ret = gpio_pin_toggle(led.port, led.pin);
		//ret = gpio_pin_toggle_dt(&led);
		if (ret < 0)
			return;


		//printk("Hello World! %s\n", CONFIG_BOARD);
		printk("device is %p, name is %s\n", iim42652, iim42652->name);

		k_msleep(SLEEP_TIME_MS);
	}
}
