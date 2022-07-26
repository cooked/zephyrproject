#ifndef __ZEPHYR_DRIVERS_SENSOR_IMU_ABSTRACTION_H__
#define __ZEPHYR_DRIVERS_SENSOR_IMU_ABSTRACTION_H__

// Include sensor.h for sensor_attribute enum
#include <drivers/sensor.h>

/**
 * This enum holds various attributes for an IMU sensor, such as modes of
 * operation for an IMU. This is to be used with the Zephyr Sensor abstraction
 * layer, specifically as a sensor_attribute.
 */
enum sensor_imu_attribute {
	// Start from SENSOR_ATTR_PRIV_START, so that we can use private sensor
	// attributes to define e.g. different sensor IMU modes as defined below.
	SENSOR_IMU_MODE_UNKNOWN = SENSOR_ATTR_PRIV_START,

	SENSOR_IMU_MODE_SINGLE_MEASUREMENT,
	SENSOR_IMU_MODE_CONTINUOUS_BURST,
	SENSOR_IMU_MODE_CONTINUOUS_THRESHOLDED,

	SENSOR_IMU_MODE_ALL,
};


#endif