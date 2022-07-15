/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/i2c.h>
#include <logging/log.h>
//#include <sys/__assert.h>

#include "ixm42xxx.h"
#include "ixm42xxx_reg.h"

// TODO: check if format of the name is ok
LOG_MODULE_DECLARE(IXM42XXX, CONFIG_SENSOR_LOG_LEVEL);


static const struct device *i2c_dev;
static uint16_t i2c_addr;

int ixm42xxx_i2c_init(const struct device *i2c_device, uint16_t *addr)
{
	__ASSERT_NO_MSG(i2c_device);
	i2c_dev = i2c_device;
	i2c_addr = &addr;
	return 0;
}

// TODO: NEW to be tested
int i2c_reg_read_byte_ixm42xxx(struct device *dev, uint8_t reg_addr, uint8_t *value) {

	if(i2c_reg_read_byte(i2c_dev, i2c_addr, reg_addr, *value) < 0)
		return -EIO;

	return 0;

}


// this was originally from kevin code

// Generic I2C <start><write><repeated start><read><stop>.
// One buffer is used for TX and RX so you better make it
// large enough for both operations.
int i2c_write_read_handler_iim42652(struct device *dev, uint8_t *p_data, uint8_t tx_size, uint8_t rx_size) {
	struct iim42652_data *dev_data = dev->data;

	// TODO: addr now retrived as pointer... check if working
	int err = i2c_write(dev_data->i2c, p_data, tx_size, i2c_addr);
	if (err) {
		LOG_ERR("Failed to send i2c data! Error code: %i", err);
		return err;
	}

	err = i2c_read(dev_data->i2c, p_data, rx_size, i2c_addr);
	if (err) {
		LOG_ERR("Failed to read i2c data! Error code: %i", err);
		return err;
	}

	return 0;
}



// TODO: understand and implement burst read if needed, ported from elsewhere

/*static int ixm42xxx_reg_read_i2c(const union bme280_bus *bus, uint8_t start, uint8_t *buf, int size) {
	return i2c_burst_read_dt(&bus->i2c, start, buf, size);
}

static int ixm42xxx_reg_write_i2c(const union bme280_bus *bus, uint8_t reg, uint8_t val) {
	return i2c_reg_write_byte_dt(&bus->i2c, reg, val);
}*/
/*const struct bme280_bus_io bme280_bus_io_i2c = {
	.check = bme280_bus_check_i2c,
	.read = bme280_reg_read_i2c,
	.write = bme280_reg_write_i2c,
};*/
