/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IXM42XXX_I2C_H_
#define ZEPHYR_DRIVERS_SENSOR_IXM42XXX_IXM42XXX_I2C_H_

#include <device.h>

// old from spi
//int inv_i2c_single_write(uint8_t reg, uint8_t *data);
//int inv_i2c_read(uint8_t reg, uint8_t *data, size_t len);

int ixm42xxx_i2c_init(const struct device *spi_device,
		    const struct spi_config *spi_config);

int i2c_write_read_handler(uint8_t *p_data,
			uint8_t tx_size, uint8_t rx_size);

int i2c_reg_read_byte_ixm42xxx(struct device *dev,
			uint8_t reg_addr, uint8_t *value);

#endif /* __SENSOR_IXM42XXX_IXM42XXX_I2C__ */
