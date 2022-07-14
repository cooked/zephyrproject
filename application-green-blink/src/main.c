// // /*
//  * Copyright (c) 2016 Intel Corporation
//  *
//  * SPDX-License-Identifier: Apache-2.0
//  */


/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zephyr/types.h>

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

 #define LEDG_NODE DT_ALIAS(led1)
// /* The devicetree node identifier for the "led0" alias. */
#define LED_G DT_GPIO_LABEL(LEDG_NODE, gpios)
#define PIN DT_GPIO_PIN(LEDG_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LEDG_NODE, gpios)

void main(void)
{
	const struct device *dev = device_get_binding(LED_G);
	bool led_is_on = true;
	int ret;


 	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
 	if (ret < 0) {
 		return;
 	}

 	while (1) {
		gpio_pin_set(dev, PIN, (int) led_is_on);
		led_is_on = !led_is_on;
 		k_msleep(SLEEP_TIME_MS);
 	}
}
