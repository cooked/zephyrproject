
#include <zephyr/types.h>		// For Zephyr types and stdint.h
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <sys/__assert.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <init.h>
#include <kernel.h>
#include <string.h>
#include <logging/log.h>

#include <imu.h>
#include <iim42652.h>

static const struct device *iim42652_device = NULL;

LOG_MODULE_REGISTER(IIM42652, LOG_LEVEL_DBG);

#define DT_DRV_COMPAT invensense_iim42652



////////////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////////////



// Generic I2C <start><write><repeated start><read><stop>.
// One buffer is used for TX and RX so you better make it
// large enough for both operations.
int i2c_write_read_handler_iim42652(uint8_t *p_data, uint8_t tx_size, uint8_t rx_size) {
	struct iim42652_data_t *device_data = iim42652_device->data;

	// LOG_INF("Doing i2c write shit...");
	int err = i2c_write(device_data->i2c, p_data, tx_size, IIM42652_I2C_ADDRESS);

	if (err) {
		LOG_ERR("Failed to send i2c data! Error code: %i", err);
		return err;
	}

	// LOG_INF("Doing i2c read shit...");
	err = i2c_read(device_data->i2c, p_data, rx_size, IIM42652_I2C_ADDRESS);

	if (err) {
		LOG_ERR("Failed to read i2c data! Error code: %i", err);
		return err;
	}

	// int err = i2c_write_read(device_data->i2c, IIM42652_I2C_ADDRESS,
	// 	p_data, tx_size, p_data, rx_size);

	// if (err) {
	// 	LOG_ERR("Failed to do i2c_write_read! Error code: %i", err);
	// }
	// LOG_INF("Doing i2c shit transfer success");
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Main driver implementation
////////////////////////////////////////////////////////////////////////////////

// Up to 9 bytes may be returned.
uint8_t iim42652_data_buffer[10] = {0U};
uint8_t iim42652_status = 0;
uint16_t iim42652_t     = 0;
uint16_t iim42652_x     = 0;
uint16_t iim42652_y     = 0;
uint16_t iim42652_z     = 0;
uint8_t iim42652_memory[64 * 2]; // 64 16-bit words.

int test_who_am_i() {
	memset(iim42652_data_buffer, 0, 10);

	uint8_t rx_size = 1;
	uint8_t tx_size = 1;

	const uint8_t WHO_AM_I = 0x75;
	iim42652_data_buffer[0] = WHO_AM_I;

	int err = i2c_write_read_handler_iim42652((uint8_t *) iim42652_data_buffer, tx_size, rx_size);

	if (err < 0) {
		LOG_ERR("Failed to read WHO_AM_I register, err %d", err);
		return err;
	}

	// Check expected who am i data...
	const uint8_t EXPECTED_WHO_AM_I = 0x6F;

	if (EXPECTED_WHO_AM_I != iim42652_data_buffer[0]) {
		LOG_ERR("Unexpected WHO_AM_I value for IIM42652! Got %u, expected %u",
			iim42652_data_buffer[0], EXPECTED_WHO_AM_I);
		return -1;
	}

	LOG_INF("Got expected WHO_AM_I value for IIM42652!");
	return 0;
}



////////////////////////////////////////////////////////////////////////////////
// Zephyr Sensor API implementation
////////////////////////////////////////////////////////////////////////////////



int iim42652_init(const struct device *dev) {
	LOG_INF("Starting IIM42652 initialization...");
	iim42652_device = dev;
	struct iim42652_data_t *drv_data = dev->data;
	drv_data->ready = false;

	// Setup i2c
	drv_data->i2c = device_get_binding(DT_INST_BUS_LABEL(0));

	if (drv_data->i2c == NULL) {
		LOG_ERR("Failed to get pointer to %s device!", DT_INST_BUS_LABEL(0));
		return -EINVAL;
	}

	// Chip should now be in the idle state
	LOG_INF("Succesfully initialized IIM42652");
	drv_data->ready = true;
	return 0;
}

static struct iim42652_data_t iim42652_data;

// TODO: Find out what pm_control_fn is and does. Power management?
// TODO: Find out if we need to put something into the 2nd NULL placeholder
DEVICE_DT_INST_DEFINE(
	0,
	iim42652_init,
	NULL,				// pm_control_fn - Pointer to pm_control function. Can be NULL if not implemented.
	&iim42652_data,
	NULL,				// The address to the structure containing the configuration information for this instance of the driver.
	POST_KERNEL,
	CONFIG_SENSOR_INIT_PRIORITY,
	NULL);
