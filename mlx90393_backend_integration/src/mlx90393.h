/*
	Elektor project 140555 MLX90393 BoB
	Copyright (c) 2015 Elektor.  All rights reserved.
	Author: CPV, 25/02/2015

	This is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This software is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this software; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __ZEPHYR_DRIVERS_SENSOR_MLX90393_H__
#define __ZEPHYR_DRIVERS_SENSOR_MLX90393_H__

#include <device.h>				// For Zephyr's Device API
#include <drivers/gpio.h>		// For Zephyr's GPIO API
#include <drivers/sensor.h>		// For Zephyr's Sensor API

#include <zephyr/types.h>		// For Zephyr types and stdint.h
#include <sys/util.h>			// For stuff like BIT(n) etc

#if DT_INST_NODE_HAS_PROP(0, interrupt_gpios)
#error "This MLX90393 driver depends on an interrupt GPIO pin being available!"
#endif



// TODO: Split this file into driver-internals, and Zephyr API glue stuff


/*******************************************************************************
Driver internals
*******************************************************************************/


// Generic I2C <start><write><repeated start><read><stop>.
// One buffer is used for TX and RX so you better make it
// large enough for both operations.
int i2c_write_read_handler(uint8_t *p_data, uint8_t tx_size, uint8_t rx_size);

// Assemble an uint16_t from <MSB,LSB>.
uint16_t assemble_16(uint8_t *p_data);

// Assemble an uint32_t from <MSB,LSB>.
uint32_t assemble_32(uint8_t *p_data);

// Get the I2C address.
#define MLX90393_I2C_ADDRESS (DT_INST_REG_ADDR(0))



/*******************************************************************************
Key commands.
*******************************************************************************/

// TODO: Put all these commands in an enum

#define MLX90393_CMD_NOP (0x00)  /* No OPeration */

/*
	BM = Burst mode

	The ASIC will have a programmable data rate at which it will operate. This
	data rate implies auto- wakeup and sequencing of the ASIC, flagging that
	data is ready on a dedicated pin (INT/DRDY). The maximum data rate
	corresponds to continuous burst mode, and is a function of the chosen
	measurement axes. For non-continuous burst modes, the time during which the
	ASIC has a counter running but is not doing an actual conversion is called
	the Standby mode (STBY).
*/
#define MLX90393_CMD_SB (0x10)   /* Start Burst mode */

/*
	WOC = Wake-up On Change

	This mode is similar to the burst mode in the sense that the device will be
	auto-sequencing, with the difference that the measured component(s) is/are
	compared with a reference and in case the difference is bigger than a
	user-defined threshold, the DRDY signal is set on the designated pin. The
	user can select which axes and/or temperature fall under this cyclic check,
	and which thresholds are allowed.
*/
#define MLX90393_CMD_SW (0x20)   /* Start Wake-up On Change */

/*
	SM = Single Measure mode

	The master will ask for data via the corresponding protocol (I2C or SPI),
	waking up the ASIC to make a single conversion, immediately followed by an
	automatic return to sleep mode (IDLE) until the next polling of the master.
	This polling can also be done by strobing the TRG pin instead, which has the
	same effect as sending a protocol command for a single measurement.
*/
#define MLX90393_CMD_SM (0x30)   /* Start Single Measurement (polling mode) */

/*
	RM = Read Measurement

	This command differs depending on the value for zyxt. The data is returned
	in the order Status-TXYZ, where the components which are set to zero are
	skipped.
*/
#define MLX90393_CMD_RM (0x40)

/* Other commands */
#define MLX90393_CMD_RR (0x50)   /* Read from a Register */
#define MLX90393_CMD_WR (0x60)   /* Write to a Register */
#define MLX90393_CMD_EX (0x80)   /* EXit */
#define MLX90393_CMD_HR (0xD0)   /* Memory Recall */
#define MLX90393_CMD_HS (0xE0)   /* Memory Store */
#define MLX90393_CMD_RT (0xF0)   /* Reset */



/*******************************************************************************
Status byte and all the individual bits that comprise it
*******************************************************************************/

// TODO: Also put all these bits of the status byte in an enum?

/*
	These bits define in which mode the MLX90393 is currently set. Whenever a
	mode transition command is rejected, the first status byte after this
	command will have the expected mode bit cleared, which serves as an
	indication that the command has been rejected, next to the ERROR bit. The
	SM_MODE flag can be the result of an SM command or from raising the TRG pin
	when TRG mode is enabled in the volatile memory of the MLX90393.
 */
#define MLX90393_STATUS_BYTE_BURST_MODE		BIT(7)
#define MLX90393_STATUS_BYTE_WOC_MODE		BIT(6)
#define MLX90393_STATUS_BYTE_SM_MODE		BIT(5)

/*
	This bit is set in case a command has been rejected or in case an
	uncorrectable error is detected in the memory, a so called ECC_ERROR. A
	single error in the memory can be corrected (see SED bit), two errors can be
	detected and will generate the ECC_ERROR. In such a case all commands but
	the RT (Reset) command will be rejected. The error bit is equally set when
	the master is reading back data while the DRDY flag is low.
 */
#define MLX90393_STATUS_BYTE_ERROR			BIT(4)

/*
	The single error detection bit simply flags that a bit error in the
	non-volatile memory has been corrected. It is purely informative and has no
	impact on the operation of the MLX90393.
*/
#define MLX90393_STATUS_BYTE_SED			BIT(3)

/*
	Whenever the MLX90393 gets out of a reset situation – both hard and soft
	reset – the RS flag is set to highlight this situation to the master in the
	first status byte that is read out. As soon as the first status byte is
	read, the flag is cleared until the next reset occurs.
*/
#define MLX90393_STATUS_BYTE_RS				BIT(2)

/*
	These bits only have a meaning after the RR and RM commands, when data is
	expected as a response from the MLX90393. The number of response bytes
	correspond to 2*D[1:0] + 2, so the expected byte counts are either 2, 4, 6
	or 8. For commands where no response is expected, the content of D[1:0]
	should be ignored.
*/
#define MLX90393_STATUS_BYTE_D1				BIT(1)
#define MLX90393_STATUS_BYTE_D0				BIT(0)



// Flags to use with "zyxt" variables.
#define MLX90393_NOTHING (0x0) /* Nothing */
#define MLX90393_T (0x01) /* Temperature */
#define MLX90393_X (0x02) /* X-axis */
#define MLX90393_Y (0x04) /* Y-axis */
#define MLX90393_Z (0x08) /* Z-axis */



/*******************************************************************************
Memory and registers
*******************************************************************************/

// Memory areas.
#define MLX90393_CUSTOMER_AREA_BEGIN		(0x00)
#define MLX90393_CUSTOMER_AREA_END			(0x1F)
#define MLX90393_CUSTOMER_AREA_FREE_BEGIN	(0x0A)
#define MLX90393_CUSTOMER_AREA_FREE_END		(0x1F)
#define MLX90393_MELEXIS_AREA_BEGIN			(0x20)
#define MLX90393_MELEXIS_AREA_END			(0x3F)

// TODO: Fill in registers and information (page 29-35)...
#define MLX90393_REG_Z_SERIES				(0x0)
#define MLX90393_REG_Z_SERIES_BITS			BIT(7)
#define MLX90393_REG_TRIG_INT				(0x01)
#define MLX90393_REG_TRIG_INT_BITS			BIT(15)

// Sample rate is defined as Burst data rate * 20 ms
#define MLX90393_REG_BURST_DATA_RATE		(0x01)
#define MLX90393_REG_BURST_DATA_RATE_BITS	(BIT_MASK(5))

#define MLX90393_REG_BURST_SEL				(0x01)
#define MLX90393_REG_BURST_SEL_BITS			(BIT(6) | BIT(7) | BIT(8) | BIT(9))
// etc...



// TODO: Add timing specifications here, for the delays later?
// TODO: Add XY-axis and Z-axis Noise over Conversion Time information?



/* Different modes for the MLX90393 */
enum mlx90393_mode {
	/*
		The ASIC will have a programmable data rate at which it will operate.
		This data rate implies auto- wakeup and sequencing of the ASIC, flagging
		that data is ready on a dedicated pin (INT/DRDY). The maximum data rate
		corresponds to continuous burst mode, and is a function of the chosen
		measurement axes. For non-continuous burst modes, the time during which
		the ASIC has a counter running but is not doing an actual conversion is
		called the Standby mode (STBY).

		When the sensor is operating in burst mode, it will make conversions at
		specific time intervals. The programmability of the user is the
		following:

			- Burst speed (T_INTERVAL) through parameter BURST_DATA_RATE
			- Conversion time (T_CONV) through parameters OSR, OSR2 and DIG_FILT
			- Axes/Temperature (MDATA) through parameter BURST_SEL or via the
				command argument (ZYXT)

		Whenever the MLX90393 has made the selected conversions (based on
		MDATA), the DRDY signal will be set (active H) on the INT and/or INT/TRG
		pin to indicate that the data is ready for readback. It will remain high
		until the master has sent the command to read out at least one of the
		converted quantities (ZYXT). Should the master have failed to read out
		any of them by the time the sensor has made a new conversion, the
		INT/DRDY pin will be strobed low for 10us, and the next rising edge will
		indicate a new set of data is ready.
	*/
	MLX90393_BURST_MODE,

	/*
		The master will ask for data via the corresponding protocol (I 2C or
		SPI), waking up the ASIC to make a single conversion, immediately
		followed by an automatic return to sleep mode (IDLE) until the next
		polling of the master. This polling can also be done by strobing the TRG
		pin instead, which has the same effect as sending a protocol command for
		a single measurement.

		Whenever the sensor is set to this mode (or after startup) the MLX90393
		goes to the IDLE state where it awaits a command from the master to
		perform a certain acquisition. The duration of the acquisition will be
		the concatenation of the T STBY, TACTIVE , m*TCONVM (with m # of axes)
		and TCONVT. The conversion time will effectively be programmable by the
		user (see burst mode), but is equally a function of the required
		axes/temperature to be measured. Upon reception of such a polling
		command from the master, the sensor will make the necessary
		acquisitions, and set the DRDY signal high to flag that the measurement
		has been performed and the master can read out the data on the bus at
		his convenience. The INT/DRDY will be cleared either when:

		- The master has issued a command to read out at least one of the
			measured components
		- The master issues an Exit (EX) command to cancel the measurement
		- The chip is reset, after POR (Power-on reset) or Reset command (RT)
	*/
	MLX90393_SINGLE_MEASUREMENT_MODE,

	/*
		This mode is similar to the burst mode in the sense that the device will
		be auto-sequencing, with the difference that the measured component(s)
		is/are compared with a reference and in case the difference is bigger
		than a user-defined threshold, the DRDY signal is set on the designated
		pin. The user can select which axes and/or temperature fall under this
		cyclic check, and which thresholds are allowed.

		The Wake-Up on Change (WOC) functionality can be set by the master with
		as main purpose to only receive an interrupt when a certain threshold is
		crossed. The WOC mode will always compare a new burst value with a
		reference value to assess if the difference between both exceeds a
		user-defined threshold. The reference value is defined as one of the
		following:
		- The first measurement of WOC mode is stored as reference value once.
			This measurement at “t=0” is then the basis for comparison or,
		- The reference for acquisition(t) is always acquisition(t-1), in such a
			way that the INT signal will only be set if the derivative of any
			component exceeds a threshold.

		The in-application programmability is the same as for burst mode, but
		now the thresholds for setting the interrupt are also programmable by
		the user, as well as the reference, if the latter is data(t=0) or
		data(t-1).
	*/
	MLX90393_WAKE_ON_CHANGE_MODE,

	/* In case no mode has been set yet, or it is invalid, chip is idle, etc */
	MLX90393_IDLE_MODE,
};


enum async_init_state {
	ASYNC_INIT_STATE_IDLE,
	ASYNC_INIT_STATE_MEASURING,

	ASYNC_INIT_STATE_COUNT,
};


// TODO: Add delays based upon state changes from above?


/* This struct definition is used for the handling of commands */
typedef struct mlx90393_command {
	/*  */
	uint8_t command;
	uint8_t zyxt;
	uint8_t address;
	uint16_t data;

	uint8_t data_buffer[10];

	uint8_t status_byte;
	bool error;

	uint16_t temp;
	uint16_t x;
	uint16_t y;
	uint16_t z;
} mlx90393_command_t;



/*******************************************************************************
Driver internals
*******************************************************************************/


// Up to 9 bytes may be returned.
extern uint8_t mlx90393_data_buffer[10];
extern uint8_t mlx90393_status;
extern uint16_t mlx90393_t;
extern uint16_t mlx90393_x;
extern uint16_t mlx90393_y;
extern uint16_t mlx90393_z;
extern uint8_t mlx90393_memory[64 * 2]; // 64 16-bit words.

int mlx90393_command(uint8_t cmd, uint8_t zyxt, uint8_t address, uint16_t data);

// Returns true/false if the command actually succeeded
bool mlx90393_command_succeeded();

// Returns current mode of the MLX90393
enum mlx90393_mode mlx90393_get_current_mode();

// Returns true/false if the current mode is the same as the expected one
bool mlx90393_in_correct_mode(enum mlx90393_mode desired_mode);


void mlx90393_decode(uint8_t zyxt);
uint16_t mlx90393_read_memory_word(uint8_t address);
void mlx90393_write_memory_word(uint8_t address, uint8_t data);
void mlx90393_read_memory(uint8_t *p_dst, uint8_t address, uint8_t size);
int mlx90393_try_command(uint8_t command, uint8_t zyxt, uint8_t address,
	uint16_t data, enum mlx90393_mode expected_mode, bool *success,
	bool *in_correct_mode, uint8_t max_attempts);



extern bool use_interrupt;
extern bool use_low_level;



/*******************************************************************************
Zephyr Sensor API implementation
*******************************************************************************/


struct mlx90393_data_t {
	// I2C device pointer
	const struct device *i2c;

	// GPIO stuff required for the interrupt pin handling
	const struct device *gpio;
	struct gpio_callback gpio_callback;

	// K_SEM_DEFINE(data_ready_sem, 0, 1);
	struct k_sem data_ready_sem;


	// For Zephyr Sensor abstraction layer handling of interrupts/triggers
	sensor_trigger_handler_t trigger_handler;
	struct sensor_trigger trigger;

	// Current mode of the MLX90393
	enum mlx90393_mode current_mode;


	// TODO: Remove this stuff?
	uint8_t data_buffer[10];
	bool ready;
};

int mlx90393_attr_set(
	const struct device *dev,
	enum sensor_channel chan,
	enum sensor_attribute attr,
	const struct sensor_value *value);

int mlx90393_trigger_set(
	const struct device *dev,
	const struct sensor_trigger *trigger,
	sensor_trigger_handler_t handler);


#endif /* __ZEPHYR_DRIVERS_SENSOR_MLX90393_H__ */
