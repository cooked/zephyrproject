#ifndef __ZEPHYR_DRIVERS_SENSOR_IIM42652_H__
#define __ZEPHYR_DRIVERS_SENSOR_IIM42652_H__

#include <device.h>				// For Zephyr's Device API
#include <drivers/gpio.h>		// For Zephyr's GPIO API
#include <drivers/sensor.h>		// For Zephyr's Sensor API

#include <zephyr/types.h>		// For Zephyr types and stdint.h
#include <sys/util.h>			// For stuff like BIT(n) etc


#define IIM42652_I2C_ADDRESS (0x68)

struct iim42652_data_t {
	// I2C device pointer
	const struct device *i2c;

	bool ready;
};

int test_who_am_i();

#endif /* __ZEPHYR_DRIVERS_SENSOR_IIM42652_H__ */