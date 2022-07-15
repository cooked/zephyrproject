/*
 * Copyright (c) 2022 Anything Connected
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT invensense_iim42652

#include <drivers/spi.h>
#include <init.h>
#include <sys/byteorder.h>
#include <drivers/sensor.h>
#include <logging/log.h>

// family-wide
#include "ixm42xxx.h"
#include "ixm42xxx_reg.h"
#include "ixm42xxx_i2c.h"
#include "ixm42xxx_util.h"

// sensor-specific
#include "iim42652_setup.h"

// TODO: now is fetched from DT (in the config down below), check if ok removing it here
//#define IIM42652_I2C_ADDRESS (0x68)

LOG_MODULE_REGISTER(IIM42652, CONFIG_SENSOR_LOG_LEVEL);

// TODO: move to kconfig or yaml ?
// TODO: check on datasheet if these value are ok for our sensor
/*static const uint16_t iim42652_gyro_sensitivity_x10[] = {
	1310, 655, 328, 164
};*/


static int iim42652_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	const struct iim42652_data *drv_data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		ixm42xxx_convert_accel(val, drv_data->accel_x,
				       drv_data->accel_sensitivity_shift);
		ixm42xxx_convert_accel(val + 1, drv_data->accel_y,
				       drv_data->accel_sensitivity_shift);
		ixm42xxx_convert_accel(val + 2, drv_data->accel_z,
				       drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_X:
		ixm42xxx_convert_accel(val, drv_data->accel_x,
				       drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		ixm42xxx_convert_accel(val, drv_data->accel_y,
				       drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		ixm42xxx_convert_accel(val, drv_data->accel_z,
				       drv_data->accel_sensitivity_shift);
		break;
	case SENSOR_CHAN_GYRO_XYZ:
		ixm42xxx_convert_gyro(val, drv_data->gyro_x,
				      drv_data->gyro_sensitivity_x10);
		ixm42xxx_convert_gyro(val + 1, drv_data->gyro_y,
				      drv_data->gyro_sensitivity_x10);
		ixm42xxx_convert_gyro(val + 2, drv_data->gyro_z,
				      drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_X:
		ixm42xxx_convert_gyro(val, drv_data->gyro_x,
				      drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_Y:
		ixm42xxx_convert_gyro(val, drv_data->gyro_y,
				      drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_GYRO_Z:
		ixm42xxx_convert_gyro(val, drv_data->gyro_z,
				      drv_data->gyro_sensitivity_x10);
		break;
	case SENSOR_CHAN_DIE_TEMP:
		ixm42xxx_convert_temp(val, drv_data->temp);
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

// TODO: verify FIFO data structure
static int iim42652_sample_fetch(const struct device *dev,
				 enum sensor_channel chan)
{
	int result = 0;
	uint16_t fifo_count = 0;
	struct iim42652_data *drv_data = dev->data;

	// TODO: read i2c

	// Read INT_STATUS (0x45) and FIFO_COUNTH(0x46), FIFO_COUNTL(0x47)
	//result = inv_spi_read(MPUREG_INT_STATUS, drv_data->fifo_data, 3);

	if (drv_data->fifo_data[0] & BIT_INT_STATUS_DRDY) {
		fifo_count = (drv_data->fifo_data[1] << 8)
			+ (drv_data->fifo_data[2]);

		//result = inv_spi_read(MPUREG_FIFO_DATA, drv_data->fifo_data,
		//		      fifo_count);

		/* FIFO Data structure
		 * Packet 1 : FIFO Header(1), AccelX(2), AccelY(2),
		 *            AccelZ(2), Temperature(1)
		 * Packet 2 : FIFO Header(1), GyroX(2), GyroY(2),
		 *            GyroZ(2), Temperature(1)
		 * Packet 3 : FIFO Header(1), AccelX(2), AccelY(2), AccelZ(2),
		 *            GyroX(2), GyroY(2), GyroZ(2), Temperature(1)
		 */
		if (drv_data->fifo_data[0] & BIT_FIFO_HEAD_ACCEL) {
			/* Check empty values */
			if (!(drv_data->fifo_data[1] == FIFO_ACCEL0_RESET_VALUE
			      && drv_data->fifo_data[2] ==
			      FIFO_ACCEL1_RESET_VALUE)) {
				drv_data->accel_x =
					(drv_data->fifo_data[1] << 8)
					+ (drv_data->fifo_data[2]);
				drv_data->accel_y =
					(drv_data->fifo_data[3] << 8)
					+ (drv_data->fifo_data[4]);
				drv_data->accel_z =
					(drv_data->fifo_data[5] << 8)
					+ (drv_data->fifo_data[6]);
			}
			if (!(drv_data->fifo_data[0] & BIT_FIFO_HEAD_GYRO)) {
				drv_data->temp =
					(int16_t)(drv_data->fifo_data[7]);
			} else {
				if (!(drv_data->fifo_data[7] ==
				      FIFO_GYRO0_RESET_VALUE &&
				      drv_data->fifo_data[8] ==
				      FIFO_GYRO1_RESET_VALUE)) {
					drv_data->gyro_x =
						(drv_data->fifo_data[7] << 8)
						+ (drv_data->fifo_data[8]);
					drv_data->gyro_y =
						(drv_data->fifo_data[9] << 8)
						+ (drv_data->fifo_data[10]);
					drv_data->gyro_z =
						(drv_data->fifo_data[11] << 8)
						+ (drv_data->fifo_data[12]);
				}
				drv_data->temp =
					(int16_t)(drv_data->fifo_data[13]);
			}
		} else {
			if (drv_data->fifo_data[0] & BIT_FIFO_HEAD_GYRO) {
				if (!(drv_data->fifo_data[1] ==
				      FIFO_GYRO0_RESET_VALUE &&
				      drv_data->fifo_data[2] ==
				      FIFO_GYRO1_RESET_VALUE)) {
					drv_data->gyro_x =
						(drv_data->fifo_data[1] << 8)
						+ (drv_data->fifo_data[2]);
					drv_data->gyro_y =
						(drv_data->fifo_data[3] << 8)
						+ (drv_data->fifo_data[4]);
					drv_data->gyro_z =
						(drv_data->fifo_data[5] << 8)
						+ (drv_data->fifo_data[6]);
				}
				drv_data->temp =
					(int16_t)(drv_data->fifo_data[7]);
			}
		}
	}

	return 0;
}

static int iim42652_data_init(struct iim42652_data *data,
			      const struct iim42652_config *cfg)
{
	data->accel_x = 0;
	data->accel_y = 0;
	data->accel_z = 0;
	data->temp = 0;
	data->gyro_x = 0;
	data->gyro_y = 0;
	data->gyro_z = 0;
	//data->accel_hz = cfg->accel_hz;
	//data->gyro_hz = cfg->gyro_hz;

	//data->accel_sf = cfg->accel_fs;
	//data->gyro_sf = cfg->gyro_fs;

	data->tap_en = false;
	data->sensor_started = false;

	return 0;
}

static int iim42652_init(const struct device *dev)
{
	struct iim42652_data *drv_data = dev->data;
	const struct iim42652_config *cfg = dev->config;

	// TODO: ported to i2c, check if working
	drv_data->i2c = device_get_binding(cfg->i2c_label);
	if (!drv_data->i2c) {
		LOG_ERR("I2C device not exist");
		return -ENODEV;
	}

	ixm42xxx_i2c_init(drv_data->i2c, cfg->addr);
	ixm42xxx_data_init(drv_data, cfg);
	// TODO: inside need to update to use i2c
	iim42652_sensor_init(dev);	// in _setup.h, incl who_am_I check (or check chip ID)

	//drv_data->accel_sensitivity_shift = 14 - 3;
	//drv_data->gyro_sensitivity_x10 = iim42652_gyro_sensitivity_x10[3];

	// TODO: interrupts
	/*if (icm42605_init_interrupt(dev) < 0) {
		LOG_ERR("Failed to initialize interrupts.");
		return -EIO;
	}
	LOG_DBG("Initialize interrupt done");
	*/

	return 0;
}

static const struct sensor_driver_api iim42652_driver_api = {
	//.trigger_set = iim42652_trigger_set,
	.sample_fetch = iim42652_sample_fetch,
	.channel_get = iim42652_channel_get,
	.attr_set = ixm42xxx_attr_set,
	.attr_get = ixm42xxx_attr_get,
};

// DEVICETREE driver howto
// https://docs.zephyrproject.org/latest/build/dts/howtos.html#option-1-create-devices-using-instance-numbers

// TODO: find out sensor i2c max frequency .frequency = DT_INST_PROP(index, i2c_max_frequency),
// TODO: all the config below
/*.int_label = DT_INST_GPIO_LABEL(index, int_gpios)
		.int_pin =  DT_INST_GPIO_PIN(index, int_gpios)
		.int_flags = DT_INST_GPIO_FLAGS(index, int_gpios)
		.gpio_label = DT_INST_SPI_DEV_CS_GPIOS_LABEL(index)
		.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(index)
		.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(index)*/
#define IIM42652_DEFINE_CONFIG(index)					\
	static const struct iim42652_config iim42652_cfg_##index = {	\
		.i2c_label = DT_INST_BUS_LABEL(index),			\
		.addr = DT_INST_REG_ADDR(index),			\
		.accel_hz = DT_INST_PROP(index, accel_hz),		\
		.gyro_hz = DT_INST_PROP(index, gyro_hz),		\
		.accel_fs = DT_ENUM_IDX(DT_DRV_INST(index), accel_fs),	\
		.gyro_fs = DT_ENUM_IDX(DT_DRV_INST(index), gyro_fs),	\
	}

// TODO: find out about the first NULL below .... pm_control_fn (Power management) is it useful for our sensor?
#define IIM42652_INIT(index)						\
	IIM42652_DEFINE_CONFIG(index);					\
	static struct iim42652_data iim42652_driver_##index;		\
	DEVICE_DT_INST_DEFINE(index, iim42652_init,			\
			    NULL,					\
			    &iim42652_driver_##index,			\
			    &iim42652_cfg_##index, \
				POST_KERNEL,		\
			    CONFIG_SENSOR_INIT_PRIORITY,		\
			    &iim42652_driver_api);


// ENTRY POINT: calls the above macros

DT_INST_FOREACH_STATUS_OKAY(IIM42652_INIT)
