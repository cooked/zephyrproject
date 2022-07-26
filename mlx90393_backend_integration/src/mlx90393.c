
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
#include <mlx90393.h>

static const struct device *mlx90393_device = NULL;

LOG_MODULE_REGISTER(MLX90393, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT melexis_mlx90393

#define INT_DRDY_PIN			(DT_INST_GPIO_PIN(0, interrupt_gpios))
#define INT_DRDY_PIN_FLAGS		(DT_INST_GPIO_FLAGS(0, interrupt_gpios))



// TODO: Remove this use_interrupt and use_low_level stuff
bool use_interrupt = false;
bool use_low_level = true;


// TODO: Split this file into driver-internals, and Zephyr API glue stuff

////////////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////////////

const char *bit_rep[16] = {
	[ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
	[ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
	[ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
	[12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
};

void print_byte(uint8_t byte) {
	LOG_DBG("%s%s", bit_rep[byte >> 4], bit_rep[byte & 0x0F]);
}

// Generic I2C <start><write><repeated start><read><stop>.
// One buffer is used for TX and RX so you better make it
// large enough for both operations.
int i2c_write_read_handler(uint8_t *p_data, uint8_t tx_size, uint8_t rx_size) {
	struct mlx90393_data_t *device_data = mlx90393_device->data;

	// LOG_INF("Doing i2c write shit...");
	int err = i2c_write(device_data->i2c, p_data, tx_size, MLX90393_I2C_ADDRESS);

	if (err) {
		LOG_ERR("Failed to send i2c data! Error code: %i", err);
		return err;
	}

	// LOG_INF("Doing i2c read shit...");
	err = i2c_read(device_data->i2c, p_data, rx_size, MLX90393_I2C_ADDRESS);

	if (err) {
		LOG_ERR("Failed to read i2c data! Error code: %i", err);
		return err;
	}

	// int err = i2c_write_read(device_data->i2c, MLX90393_I2C_ADDRESS,
	// 	p_data, tx_size, p_data, rx_size);

	// if (err) {
	// 	LOG_ERR("Failed to do i2c_write_read! Error code: %i", err);
	// }
	// LOG_INF("Doing i2c shit transfer success");
	return 0;
}

uint16_t assemble_16(uint8_t *p_data) {
	uint16_t result = p_data[0];
	result          = (result << 8) + p_data[1];
	return result;
}

uint32_t assemble_32(uint8_t *p_data) {
	int i;
	uint32_t result = p_data[0];
	for (i = 1; i < 4; i++) {
		result = (result << 8) + p_data[i];
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// Main driver implementation
////////////////////////////////////////////////////////////////////////////////

// Up to 9 bytes may be returned.
uint8_t mlx90393_data_buffer[10] = {0U};
uint8_t mlx90393_status = 0;
uint16_t mlx90393_t     = 0;
uint16_t mlx90393_x     = 0;
uint16_t mlx90393_y     = 0;
uint16_t mlx90393_z     = 0;
uint8_t mlx90393_memory[64 * 2]; // 64 16-bit words.


int mlx90393_command(uint8_t cmd, uint8_t zyxt, uint8_t address, uint16_t data) {
	// rx and tx sizes are sizes in bytes
	uint8_t rx_size = 1;
	uint8_t tx_size = 1;

	// Clear the last few bits of the command to be sure, because that is where
	// the zyxt bits will go eventually. Similar for the zyxt and address bytes.
	cmd &= 0xf0;
	zyxt &= 0x0f;
	address &= 0x3f;

	switch (cmd) {
		// Start <MODE> commands only need the command, and the axes to measure
		case MLX90393_CMD_SB:
		case MLX90393_CMD_SW:
		case MLX90393_CMD_SM:
			cmd |= zyxt;
			break;

		// Read Measurement command also needs to adjust the rx_size for the
		// configured axes to measure.
		case MLX90393_CMD_RM:
			cmd |= zyxt;

			if ((zyxt & MLX90393_T) != 0) {
				rx_size += 2;
			}

			if ((zyxt & MLX90393_X) != 0) {
				rx_size += 2;
			}

			if ((zyxt & MLX90393_Y) != 0) {
				rx_size += 2;
			}

			if ((zyxt & MLX90393_Z) != 0) {
				rx_size += 2;
			}

			break;

		// Read Register command operates on MLX90393 RAM data
		case MLX90393_CMD_RR:
			mlx90393_data_buffer[1] = address << 2;
			rx_size += 2;
			tx_size = 2;
			break;

		// Write Register command operates on MLX90393 RAM data
		case MLX90393_CMD_WR:
			mlx90393_data_buffer[1] = (data & 0xff00) >> 8;
			mlx90393_data_buffer[2] = data & 0x00ff;
			mlx90393_data_buffer[3] = address << 2;
			tx_size                 = 4;
			break;

		// These commands currently do not need any special treatment
		case MLX90393_CMD_NOP:
		case MLX90393_CMD_EX:
		case MLX90393_CMD_RT:
		case MLX90393_CMD_HR:
		case MLX90393_CMD_HS:
			break;

		default:
			LOG_ERR("Error, invalid command '%x'!", cmd);
	}

	mlx90393_data_buffer[0] = cmd;
	// LOG_INF("Doing i2c shit...");
	return i2c_write_read_handler((uint8_t *) mlx90393_data_buffer, tx_size, rx_size);
}

bool mlx90393_command_succeeded() {
	// Check if an error happened or not, aka if the error bit is set or not
	uint8_t status = mlx90393_data_buffer[0];
	return !(status & MLX90393_STATUS_BYTE_ERROR);
}

enum mlx90393_mode mlx90393_get_current_mode() {
	// Check the mode bits to see which mode is currently active, if any
	uint8_t status = mlx90393_data_buffer[0];

	if (status & MLX90393_STATUS_BYTE_BURST_MODE) {
		return MLX90393_BURST_MODE;
	}

	if (status & MLX90393_STATUS_BYTE_SM_MODE) {
		return MLX90393_SINGLE_MEASUREMENT_MODE;
	}

	if (status & MLX90393_STATUS_BYTE_WOC_MODE) {
		return MLX90393_WAKE_ON_CHANGE_MODE;
	}

	return MLX90393_IDLE_MODE;
}

bool mlx90393_in_correct_mode(enum mlx90393_mode expected_mode) {
	enum mlx90393_mode current_mode = mlx90393_get_current_mode();
	return current_mode == expected_mode;
}


void mlx90393_decode(uint8_t zyxt) {
	uint8_t *p = (uint8_t *) mlx90393_data_buffer;

	mlx90393_status = *p;
	p += 1;

	if ((zyxt & MLX90393_T) != 0) {
		mlx90393_t = assemble_16(p);
		p += 2;
	}

	if ((zyxt & MLX90393_X) != 0) {
		mlx90393_x = assemble_16(p);
		p += 2;
	}

	if ((zyxt & MLX90393_Y) != 0) {
		mlx90393_y = assemble_16(p);
		p += 2;
	}

	if ((zyxt & MLX90393_Z) != 0) {
		mlx90393_z = assemble_16(p);
		p += 2;
	}
}

uint16_t mlx90393_read_memory_word(uint8_t address) {
	mlx90393_command(MLX90393_CMD_RR, 0, address, 0);
	// mlx90393_data_buffer[0] contains status code.
	return assemble_16((uint8_t *) &mlx90393_data_buffer[1]);
}

void mlx90393_write_memory_word(uint8_t address, uint8_t data) {
	mlx90393_command(MLX90393_CMD_WR, 0, address, data);
}

void mlx90393_read_memory(uint8_t *p_dst, uint8_t address, uint8_t size) {
	uint16_t val;
	while (size != 0) {
		val    = mlx90393_read_memory_word(address);
		LOG_INF("Reading memory word at address %u, got %u", address, val);
		k_sleep(K_MSEC(1));
		*p_dst = (val >> 8) & 0xff;
		p_dst += 1;
		*p_dst = val & 0xff;
		p_dst += 1;
		address += 1;
		size -= 1;
	}
}


int mlx90393_try_command(uint8_t command, uint8_t zyxt, uint8_t address, uint16_t data, enum mlx90393_mode expected_mode, bool *success, bool *in_correct_mode, uint8_t max_attempts) {
	uint8_t attempts = 0u;
	int err = 0;
	*success = false;
	*in_correct_mode = false;

	// TODO: Add option to ignore the "in_correct_mode" for the while expression
	while (!(*success && *in_correct_mode)) {
		// Send command
		err = mlx90393_command(command, zyxt, address, data);

		if (err) {
			LOG_ERR("Failed to do command %x, err %d!", command, err);
		}

		*in_correct_mode = mlx90393_in_correct_mode(expected_mode);

		// LOG_INF("Success: %d, err: %d, correct mode: %d",
		// 	mlx90393_command_succeeded(), err, *in_correct_mode);

		*success = mlx90393_command_succeeded() && err == 0;

		// LOG_INF("");
		// LOG_INF("v%u: SM command, success: %d, mode correct: %d, binary: %s%s",
		// 	attempts,
		// 	success,
		// 	in_correct_mode,
		// 	bit_rep[mlx90393_data_buffer[0] >> 4],
		// 	bit_rep[mlx90393_data_buffer[0] & 0x0F]);

		++attempts;
		// k_sleep(K_MSEC(10));

		if (attempts > 0u && !use_interrupt) {
			k_sleep(K_MSEC(10));
		}

		if (attempts >= max_attempts && !(*success)) {
			LOG_ERR("Tried %u attempts for command %x, giving up...",
				attempts, command);
			// return;
			break;
		}
	}

	return err;
}



////////////////////////////////////////////////////////////////////////////////
// Zephyr Sensor API implementation
////////////////////////////////////////////////////////////////////////////////

static inline uint8_t get_zyxt_from_sensor_channel(enum sensor_channel chan) {
	uint8_t zyxt = MLX90393_NOTHING;

	switch (chan) {
		case SENSOR_CHAN_MAGN_X:
			zyxt = MLX90393_X;
			break;

		case SENSOR_CHAN_MAGN_Y:
			zyxt = MLX90393_Y;
			break;

		case SENSOR_CHAN_MAGN_Z:
			zyxt = MLX90393_Z;
			break;

		case SENSOR_CHAN_MAGN_XYZ:
			zyxt = MLX90393_X | MLX90393_Y | MLX90393_Z;
			break;

		case SENSOR_CHAN_DIE_TEMP:
			zyxt = MLX90393_T;
			break;

		case SENSOR_CHAN_ALL:
			zyxt = MLX90393_X | MLX90393_Y | MLX90393_Z | MLX90393_T;
			break;

		default: {
			zyxt = MLX90393_NOTHING;
			break;
		}
	}

	return zyxt;
}

static int mlx90393_sample_fetch(const struct device *dev, enum sensor_channel chan) {
	struct mlx90393_data_t *drv_data = dev->data;

	// Double check to see that we have a valid channel to fetch and set zyxt
	uint8_t zyxt = get_zyxt_from_sensor_channel(chan);

	if (zyxt == MLX90393_NOTHING) {
		LOG_ERR("Unsupported sensor channel %d for MLX90393.", chan);
		return -ENOTSUP;
	}

	// Perform a start measurement command
	bool success = false;
	bool in_correct_mode = false;
	const size_t max_attempts = 5;

	for (size_t i = 0; i < max_attempts; i++) {
		int err = mlx90393_command(MLX90393_CMD_SM, zyxt, 0, 0);

		if (err) {
			LOG_ERR("Failed to execute SM command! Err code: %d", err);
			continue;
		}

		// Check success and if MLX90393 is in the correct mode
		success = mlx90393_command_succeeded();
		in_correct_mode = mlx90393_in_correct_mode(MLX90393_SINGLE_MEASUREMENT_MODE);

		if (!success) {
			LOG_ERR("Failed to put MLX90393 in Single Measurement mode!");
			continue;
			// return -EIO;
		}

		if (!in_correct_mode) {
			LOG_ERR("SM command succeeded but MLX903933 is not in the correct mode!");
			continue;
			// return -EIO;
		}

		// LOG_INF("Success and in correct mode!");
		success = in_correct_mode = true;
		drv_data->current_mode = MLX90393_SINGLE_MEASUREMENT_MODE;
		break;
	}

	// LOG_INF("Success: %d, in_correct_mode: %d", success, in_correct_mode);
	return (success && in_correct_mode) ? 0 : -EIO;
}

// static void mlx90393_convert(struct sensor_value *val, int16_t sample, uint8_t adjustment) {
static void mlx90393_convert(struct sensor_value *val, uint16_t sample) {
	int32_t conv_val = sample;

	// conv_val  = sample * MLX90393_MICRO_GAUSS_PER_BIT * ((uint16_t) adjustment + 128) / 256;
	// val->val1 = conv_val / 1000000;
	// val->val2 = conv_val % 1000000;

	// TODO: Calculate proper MLX90393 magnetometer values. For now, use ints.
	val->val1 = conv_val;
	val->val2 = 0;
}

static int mlx90393_channel_get(const struct device *dev, enum sensor_channel chan,
	struct sensor_value *val) {

	// We can do an assert without an explicit message, as this function should
	// not even be called if the sample_fetch failed due to an invalid channel.
	__ASSERT_NO_MSG(
		chan == SENSOR_CHAN_MAGN_X ||
		chan == SENSOR_CHAN_MAGN_Y ||
		chan == SENSOR_CHAN_MAGN_Z ||
		chan == SENSOR_CHAN_MAGN_XYZ ||
		chan == SENSOR_CHAN_DIE_TEMP ||
		chan == SENSOR_CHAN_ALL);

	// Wait for interrupt INT/DRDY to be ready...
	struct mlx90393_data_t *drv_data = dev->data;

	// int64_t elapsed_milliseconds = k_uptime_get();
	// LOG_INF("Waiting for semaphore from trigger...");

	k_sem_take(&((struct mlx90393_data_t *) dev->data)->data_ready_sem, K_FOREVER);
	// k_sem_take(&drv_data->data_ready_sem, K_FOREVER);

	// int64_t duration = k_uptime_get() - elapsed_milliseconds;
	// LOG_INF("Waiting took %lld miliseconds", duration);



	// Get zyxt from sensor channel configuration for RM command
	uint8_t zyxt = get_zyxt_from_sensor_channel(chan);

	if (zyxt == MLX90393_NOTHING) {
		LOG_ERR("Unsupported sensor channel %d for MLX90393.", chan);
		return -ENOTSUP;
	}

	// Perform actual RM aka Read Measurement command
	int err = mlx90393_command(MLX90393_CMD_RM, zyxt, 0, 0);

	// After an RM command, the MLX90393 always goes back to IDLE
	if (drv_data->current_mode == MLX90393_SINGLE_MEASUREMENT_MODE) {
		drv_data->current_mode = MLX90393_IDLE_MODE;
	}

	if (err) {
		LOG_ERR("Got error %d from executing RM command!", err);
		return err;
	}

	if (!mlx90393_command_succeeded()) {
		LOG_ERR("Failed to perform Read Measurement command!");

		// TODO: Give it X attempts before calling it quits?
		return -EIO;
	}

	// Decode and convert raw value into weird "sensor_value" struct format
	mlx90393_decode(zyxt);
	bool add_temp = (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_DIE_TEMP);

	if (chan == SENSOR_CHAN_MAGN_XYZ || chan == SENSOR_CHAN_ALL) {
		mlx90393_convert(val++, mlx90393_x);
		mlx90393_convert(val++, mlx90393_y);
		mlx90393_convert(val++, mlx90393_z);
	} else if (chan == SENSOR_CHAN_MAGN_X) {
		mlx90393_convert(val, mlx90393_x);
	} else if (chan == SENSOR_CHAN_MAGN_Y) {
		mlx90393_convert(val, mlx90393_y);
	} else if (chan == SENSOR_CHAN_MAGN_Z) {
		mlx90393_convert(val, mlx90393_z);
	}

	if (add_temp) {
		mlx90393_convert(val, mlx90393_t);
	}

	return 0;
}

int mlx90393_trigger_set(const struct device *dev, const struct sensor_trigger *trigger, sensor_trigger_handler_t handler) {
	struct mlx90393_data_t *drv_data = dev->data;

	// Disable interrupt so we can set it up again
	gpio_pin_interrupt_configure(drv_data->gpio,
		INT_DRDY_PIN, GPIO_INT_DISABLE);

	switch (trigger->type) {
		case SENSOR_TRIG_DATA_READY:
			/* code */
			break;

		default:
			LOG_ERR("Unsupported sensor trigger %d", trigger->type);
			return -ENOTSUP;
	}

	// Setup trigger and handler
	drv_data->trigger = *trigger;
	drv_data->trigger_handler = handler;

	// Configure interrupt
	int err = gpio_pin_interrupt_configure(drv_data->gpio,
		INT_DRDY_PIN,
		// GPIO_ACTIVE_HIGH |
		GPIO_INT_EDGE_TO_ACTIVE);	// This one seems to work...
		// GPIO_INT_EDGE_RISING);			// This one seems to work...
		// GPIO_INT_LEVEL_ACTIVE);			// Doesn't work...
		// GPIO_INT_LEVEL_HIGH);	// This one even breaks SEGGER RTT...
		// GPIO_INT_TRIG_HIGH | GPIO_INT_ENABLE);	// Doesn't work...

	if (err) {
		LOG_ERR("Could not configure interrupt!");
		return err;
	}

	return 0;
}

int mlx90393_attr_get(const struct device *dev, enum sensor_channel chan,
	enum sensor_attribute attr, struct sensor_value *val) {

	// TODO: Main shit
	LOG_ERR("mlx90393_attr_get: Not implemented yet!");

	return 0;
}

int mlx90393_attr_set(const struct device *dev, enum sensor_channel chan,
	enum sensor_attribute attr, const struct sensor_value *val) {

	// TODO: Main shit
	LOG_ERR("mlx90393_attr_set: Not implemented yet!");

	return 0;
}

// static int mlx90393_set_attribute(const struct device *dev, enum sensor_channel chan,
// 	enum sensor_attribute attr, int value) {

// 	int ret; struct sensor_value sensor_val;

// 	sensor_val.val1 = (value);

// 	ret = sensor_attr_set(dev, chan, attr, &sensor_val);

// 	if (ret) {
// 		printk("sensor_attr_set failed ret %d\n", ret);
// 	}

// 	return ret;
// }


static const struct sensor_driver_api mlx90393_driver_api = {
	.sample_fetch = mlx90393_sample_fetch,
	.channel_get  = mlx90393_channel_get,
	.trigger_set = mlx90393_trigger_set,
	.attr_get = mlx90393_attr_get,
	.attr_set = mlx90393_attr_set,
};


// TODO: Handle interrupt pin strobing low for about 10 microseconds. Because
// that means that we have missed a measurement...
static void mlx90393_gpio_callback(const struct device *dev, struct gpio_callback *callback, uint32_t pins) {
	LOG_DBG("GPIO callback handler, got INT/DRDY!");

	struct mlx90393_data_t *drv_data = CONTAINER_OF(callback, struct mlx90393_data_t,
		gpio_callback);

	// Signal that data is ready
	// LOG_INF("GPIO callback handler called: INT/DRDY!");
	k_sem_give(&drv_data->data_ready_sem);
}

int mlx90393_gpio_interrupt_init(struct mlx90393_data_t *drv_data) {
	if (use_interrupt) {
		// TODO: Read TRIG_INT_SEL register, double check that it's 1...
		// TODO: Set TRIG_INT_SEL to 1, so it's configured as an interrupt pin
		// mlx90393_command(MLX90393_CMD_WR, 0u, MLX90393_I2C_ADDRESS, 1u);

		k_sem_init(&drv_data->data_ready_sem, 0, K_SEM_MAX_LIMIT);

		drv_data->gpio = device_get_binding(DT_INST_GPIO_LABEL(0, interrupt_gpios));

		if (drv_data->gpio == NULL) {
			LOG_ERR("Failed to get pointer/binding to %s device!",
				DT_INST_GPIO_LABEL(0, interrupt_gpios));
			return -EINVAL;
		}

		// Configure GPIO
		int err = gpio_pin_configure(drv_data->gpio,
			INT_DRDY_PIN,
			GPIO_INPUT | GPIO_ACTIVE_HIGH | INT_DRDY_PIN_FLAGS);
			// GPIO_INPUT | GPIO_INT_LEVEL_ACTIVE | INT_DRDY_PIN_FLAGS);
			//  GPIO_INT_ENABLE | GPIO_INT_TRIG_HIGH);
			// GPIO_INT_LEVEL_HIGH
			// GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH );

		if (err) {
			LOG_ERR("Failed to configure GPIO pin");
			return err;
		}

		// Setup and add callback for GPIO
		gpio_init_callback(&drv_data->gpio_callback, mlx90393_gpio_callback,
			BIT(INT_DRDY_PIN));

		err = gpio_add_callback(drv_data->gpio, &drv_data->gpio_callback);

		if (err) {
			LOG_ERR("Failed to add GPIO callback!");
			return err;
		}

		if (use_low_level) {
			// Configure interrupt
			err = gpio_pin_interrupt_configure(drv_data->gpio,
				INT_DRDY_PIN,
				// GPIO_ACTIVE_HIGH |
				GPIO_INT_EDGE_TO_ACTIVE);	// This one seems to work...
				// GPIO_INT_EDGE_RISING);			// This one seems to work...
				// GPIO_INT_LEVEL_ACTIVE);			// Doesn't work...
				// GPIO_INT_LEVEL_HIGH);	// This one even breaks SEGGER RTT...
				// GPIO_INT_TRIG_HIGH | GPIO_INT_ENABLE);	// Doesn't work...

			if (err) {
				LOG_ERR("Could not configure interrupt!");
				return err;
			}
		}
	}

	return 0;
}

int mlx90393_init(const struct device *dev) {
	LOG_INF("Starting MLX90393 initialization...");
	mlx90393_device = dev;
	struct mlx90393_data_t *drv_data = dev->data;
	drv_data->ready = false;

	// Setup i2c
	drv_data->i2c = device_get_binding(DT_INST_BUS_LABEL(0));

	if (drv_data->i2c == NULL) {
		LOG_ERR("Failed to get pointer to %s device!", DT_INST_BUS_LABEL(0));
		return -EINVAL;
	}

	// Setup GPIO interrupts
	int err = mlx90393_gpio_interrupt_init(drv_data);

	if (err) {
		return err;
	}

	// Chip should now be in the idle state
	LOG_INF("Succesfully initialized MLX90393");
	drv_data->current_mode = MLX90393_IDLE_MODE;
	drv_data->ready = true;
	return 0;
}

static struct mlx90393_data_t mlx90393_data;

// TODO: Find out what pm_control_fn is and does. Power management?
// TODO: Find out if we need to put something into the 2nd NULL placeholder
DEVICE_DT_INST_DEFINE(
	0,
	mlx90393_init,
	NULL,				// pm_control_fn - Pointer to pm_control function. Can be NULL if not implemented.
	&mlx90393_data,
	NULL,				// The address to the structure containing the configuration information for this instance of the driver.
	POST_KERNEL,
	CONFIG_SENSOR_INIT_PRIORITY,
	&mlx90393_driver_api);
