/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(mlx90393_backend_integration, LOG_LEVEL_DBG);

// #include <string.h>
#include <stdbool.h>

#include <zephyr.h>

#include <net/net_ip.h>
#include <net/socket.h>

#include <at.h>
#include <nbiot.h>
#include <transport.h>
#include <backend.h>
#include <imu.h>
#include <mlx90393.h>
#include <iim42652.h>


// TODO: Clean this stuff up...
#include <zephyr/types.h>

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/sensor.h>

static K_SEM_DEFINE(sensor_data_ready, 0, 1);



/*
******************************************************************************
ISSUES list with this firmware...
********************************************************************************
1. HTTP GET does not always return entire response content. A regular curl
   script works just fine.
2. Parsing of JSON from said GET response, if complete, also does not work,
   returning a -22. The individual bits of the error value indicate which
   token/part failed to be parsed, so we should analyze that.

Workarounds for the above: hardcode the required asset and DN config IDs.

3. Encoding of the measurement sample data gives a zephyr crash, with a bus
   fault error. This is due to the encoding of strings, for some reason... This
   is a gamebreaker, because it prevents us from use a nice library to encode
   UTC timestamps etc... See MAKE_JSON_ENCODING_CRASH define.

Workaround: Send hardcoded, pre-formated and prepared JSON payload with hacky
snprintf() formatted data.

4. When using the modem shell sample, it is possible to see the mobile network
   time and date. However, it will sometimes be in the wrong timezone: "+8",
   which corresponds to any one of:
		- Russia
		- Taiwan
		- Mongolia
		- China
		- Malaysia
		- Brenei
		- The Philippines
		- Indonesia
		- Austrialia
   Obviously, this does not make sense... Sometimes it will be "+1", which seems
   about right for our country.

Workaround for now: hardcode the time_measured
Future workaround: Get proper UTC time from the modem, or maybe cut off the +8
part?
Best fix: Upgrade to SDK >= 1.7.0 and use the DateTime library to do NTP and get
proper UTC time.

	-> For now, microseconds are fixed to 0 as the network time does not report
	those, highest resolution is only limited to seconds. But, we have datetime.

5. This firmware does not yet work on our new PCB. The modem does not seem to
   get to the state where it can provide a working NB-IoT internet connection...
   Also, because it does work on the Thingy91 and our devkit! :/ This is really
   strange and frustrating, because:
   - the Thiny91 and devkit have been proven to work with NB-IoT, connecting to
     nRF Cloud via MQTT.
   - the Thingy91 uses the same nRF9160 chip (nRF9160_xxAA_REV2, SICA B0 for
     Thingy91, SICA B1 for devkit), modem firmware (1.3.0), and sim chip
     (NX3DV2567)...
   - the https_client sample works on the devkit and Thingy91, but not on the
     PCB...

Workaround: none :(
Cause: There is copper below the NB-IoT antenna, in the keepout zone.

6. MLX90393 is not integrated yet...

Workaround: Fake data for now, but this should not be a significant hurdle...

*/

////////////////////////////////////////////////////////////////////////////////
// NB-IoT stuff
////////////////////////////////////////////////////////////////////////////////

int setup_nbiot_things(void) {
	int err = setup_lte_modem_library();

	if (err < 0) {
		LOG_ERR("Failed to setup the LTE modem library (%d)...", err);
		return err;
	}

	/* Initialize AT comms in order to provision the certificate */
	err = at_comms_init();

	if (err < 0)
	{
		LOG_ERR("Failed to initialize AT comms with the LTE modem (%d)!", err);
		return err;
	}

	err = setup_nbiot();

	if (err < 0) {
		LOG_ERR("Failed to setup NB-IoT, err %d", err);
		return err;
	}

	print_modem_firmware_version();

	return 0;
}

int shutdown_nbiot_things(struct addrinfo *res, int fd) {
	disconnect_server(res, fd);
	return disconnect_nbiot();
}

int connect_to_backend(struct addrinfo **res) {
	// Upload via NB-IoT
	int err = connect_nbiot();

	if (err < 0) {
		LOG_ERR("Failed to connect to NB-IoT network, err %d", err);
		return err;
	}

	err = set_proper_network_datetime();

	if (err < 0) {
		LOG_ERR("Failed to set the proper network datetime, err %d", err);
		return err;
	}

	print_signal_conditions();
	print_magpio_parameters();

	// TODO: When testing with the new PCB, uncomment this to see if that maybe
	// influences the signal conditions next run?
	// set_test_magpio_parameters();

	const size_t network_datetime_size = 50;
	char network_datetime[network_datetime_size];
	memset(network_datetime, 0, network_datetime_size);
	err = get_network_datetime(network_datetime, network_datetime_size);

	if (err) {
		LOG_ERR("Failed to get the current network datetime, err %d!", err);
	} else {
		LOG_INF("Current network datetime (GMT): %s", log_strdup(network_datetime));
	}

	LOG_DBG("Connecting to the backend server...");
	int fd = connect_server(HTTPS_HOSTNAME, strlen(HTTPS_HOSTNAME), res);

	if (fd < 0) {
		LOG_ERR("Failed to connect to the server, err: %d", fd);
		return fd;
	}

	// Connection should now be established and working...
	return fd;
}

////////////////////////////////////////////////////////////////////////////////
// Sensor stuff
////////////////////////////////////////////////////////////////////////////////

static void trigger_handler(const struct device *dev, struct sensor_trigger *trigger) {
	ARG_UNUSED(dev);

	switch (trigger->type) {
		case SENSOR_TRIG_TIMER:
			LOG_INF("Timer trigger!");
			break;

		case SENSOR_TRIG_DATA_READY:
			LOG_INF("Data ready trigger!");
			break;

		default:
			LOG_INF("Unknown trigger type %d!", trigger->type);
			break;
	}

	k_sem_give(&sensor_data_ready);
}

int setup_mlx90393_things(const struct device *mlx90393_dev) {
	// Setup MLX90393
	// Maybe wait for the init to complete?
	mlx90393_dev = device_get_binding("MLX90393");

	if (!mlx90393_dev) {
		LOG_ERR("Failed to get device binding for MLX90393");
		return -1;
	}

	LOG_INF("MLX90393 device is %p, name is %s", mlx90393_dev, mlx90393_dev->name);

	if (use_interrupt) {
		if (!use_low_level) {
			struct sensor_trigger sensor_trigger_config = {
				.type = SENSOR_TRIG_DATA_READY,
				.chan = SENSOR_CHAN_ALL,
			};

			if (sensor_trigger_set(mlx90393_dev, &sensor_trigger_config, trigger_handler)) {
				LOG_ERR("Could not setup sensor trigger!");
				k_sleep(K_MSEC(100));
				return -2;
			}
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Main function
////////////////////////////////////////////////////////////////////////////////

void main(void)
{
	int err = -1;
	LOG_INF("New backend API firmware started - compiled at %s", __TIMESTAMP__);

	// Setup NB-IoT
	err = setup_nbiot_things();

	if (err < 0) {
		LOG_ERR("Failed to setup NB-IoT things (%d)! Shutting down...", err);
		k_msleep(10);
		return;
	}

	// Test IIM42652
	const struct device *iim42652_dev = device_get_binding("IIM42652");

	if (!iim42652_dev) {
		LOG_ERR("Failed to get device binding for IIM42652");
		return;
	}

	err = test_who_am_i();

	if (err < 0) {
		LOG_ERR("Failed to get WHO_AM_I from IIM42652 (%d)", err);
	}

	// Setup MLX90393
	struct device *mlx90393_dev = NULL;
	err = setup_mlx90393_things(mlx90393_dev);

	if (err < 0) {
		LOG_ERR("Failed to setup MLX90393 things (%d)! Shutting down...", err);
		k_msleep(10);
		return;
	}

	uint32_t iterations = 0u;
	// uint8_t burst_sample_rate = 50; // Multiply by 20 ms to get actual sample
	// rate
	#if 1
	enum mlx90393_mode mode = MLX90393_SINGLE_MEASUREMENT_MODE;
	#else
	enum mlx90393_mode mode = MLX90393_BURST_MODE;
	#endif

	LOG_DBG("Current mode: %d", mode);

	bool mode_set = false;
	uint32_t num_samples = 0u;
	uint32_t num_uploads = 0u;

	while (true) {
		// Disable/enable reading of MLX90393
		#if (1)

		// Get actual measurement data
		if (use_low_level) {
			// Read all temperature + xyz axes of MLX90393 magnetometer
			uint8_t zyxt = MLX90393_T | MLX90393_X | MLX90393_Y | MLX90393_Z;
			const uint8_t max_attempts = 10u;

			// Keep doing Melexis reading until we succeed...
			while (1) {
				bool success = false;
				bool in_correct_mode = false;

				uint8_t command = MLX90393_CMD_NOP;

				switch (mode) {
					case MLX90393_SINGLE_MEASUREMENT_MODE:
						command = MLX90393_CMD_SM;
						break;

					case MLX90393_BURST_MODE:
						// Only set the burst mode if we haven't done so yet
						if (!mode_set) {
							// TODO: First, update the registers with the burst
							// parameters that we want, such as sample frequency
							err = mlx90393_try_command(MLX90393_CMD_WR, zyxt,
								MLX90393_REG_BURST_DATA_RATE, 50, MLX90393_IDLE_MODE,
								&success, &in_correct_mode, 10u);

							if (err || !success) {
								LOG_ERR("Failed to setup burst sample rate, err: %d", err);
								continue;
							}

							// Then do the SB command
							LOG_DBG("Successfully set burst sample rate!");
							command = MLX90393_CMD_SB;
						}

						break;

					default:
						break;
				}

				// Set the mode of the MLX90393, execute command
				if (command != MLX90393_CMD_NOP) {
					err = mlx90393_try_command(command, zyxt, 0, 0, mode, &success, &in_correct_mode, max_attempts);

					if (err) {
						LOG_ERR("Error trying to execute command 0x%x: %d", command, err);
					}
				}

				if (!success) {
					LOG_DBG("No success yet, even after %d attempts, trying again...", max_attempts);
					continue;
				}

				LOG_DBG("Command 0x%x succeeded: %d, in correct mode: %d, err: %d",
					command, success, in_correct_mode, err);
				mode_set = true;

				// Wait either 1 second, or until data is ready
				if (use_interrupt) {
					LOG_DBG("Waiting for semaphore from trigger...");
					// k_usleep(50);
					k_sem_take(&((struct mlx90393_data_t *) mlx90393_dev->data)->data_ready_sem, K_FOREVER);
					LOG_DBG("Got semaphore from trigger!");
				} else {
					LOG_DBG("Sleeping for a bit because we aren't triggered");
					k_msleep(100);
					LOG_DBG("Still here...");
				}

				// Do RM aka Read Measurement command to read magnetometer data
				command = MLX90393_CMD_RM;

				if (mode == MLX90393_SINGLE_MEASUREMENT_MODE || mode == MLX90393_BURST_MODE) {
					err = mlx90393_try_command(command, zyxt, 0, 0, MLX90393_IDLE_MODE, &success, &in_correct_mode, max_attempts);

					if (err) {
						LOG_ERR("Error trying to execute command RM (0x%x): %d", command, err);
					}

					if (!success) {
						continue;
					}

					LOG_DBG("RM command succeeded: %d, in correct mode: %d, err: %d",
						success, in_correct_mode, err);

					mlx90393_decode(zyxt);
					++num_samples;
					int64_t elapsed_milliseconds = k_uptime_get();
					uint32_t sample_rate = 100 * num_samples / (elapsed_milliseconds / 1000);

					LOG_INF("\tTemp: %u, x: %u, y: %u, z: %u, sample rate: %u Hz",
						mlx90393_t, mlx90393_x, mlx90393_y, mlx90393_z, sample_rate);

					// LOG_INF("Got %u samples, sample rate * 100: %u Hz",
					// 	num_samples, sample_rate);

					if (!use_interrupt) {
						k_sleep(K_MSEC(100));
					}

					LOG_INF("Got %u samples so far, sample rate * 100: %lld",
						num_samples, 100 * num_samples / (elapsed_milliseconds / 1000));
					break;
				}
			}
		} else {
			// Read all temperature + xyz axes of MLX90393 magnetometer
			uint32_t num_samples = 0u;

			while (1) {
				// Prepare/fetch sensor data
				err = sensor_sample_fetch_chan(mlx90393_dev, SENSOR_CHAN_ALL);

				if (err) {
					LOG_ERR("Failed to fetch sensor sample! Error code: %d", err);
					continue;
				}

				// Actually get sensor data
				struct sensor_value sensor_values_xyzt[4] = { 0 };

				err = sensor_channel_get(mlx90393_dev, SENSOR_CHAN_ALL, sensor_values_xyzt);

				if (err) {
					LOG_ERR("Failed to read sensor sample! Error code: %d", err);
					continue;
				}

				++num_samples;
				int64_t elapsed_milliseconds = k_uptime_get();

				LOG_INF("\tTemp: %i.%i, x: %i.%i, y: %i.%i, z: %i.%i, sample rate * 100: %lld Hz",
					sensor_values_xyzt[3].val1, sensor_values_xyzt[3].val2,
					sensor_values_xyzt[0].val1, sensor_values_xyzt[0].val2,
					sensor_values_xyzt[1].val1, sensor_values_xyzt[1].val2,
					sensor_values_xyzt[2].val1, sensor_values_xyzt[2].val2,
					100 * num_samples / (elapsed_milliseconds / 1000));


				LOG_INF("Got %u samples so far, sample rate: %lld Hz",
					num_samples, num_samples / (elapsed_milliseconds / 1000));
				break;
			}
		}
		#endif

		// Setup data struct
		measurement_data_t data = {
			.ax = 12345.12345678,
			.ay = 12345.12345678,
			.az = 12345.12345678,

			.gx = 12345.12345678,
			.gy = 12345.12345678,
			.gz = 12345.12345678,

			.mx = mlx90393_x,
			.my = mlx90393_y,
			.mz = mlx90393_z,

			.temp = mlx90393_t,
		};

		// Upload to backend
		if (true) {
			print_magpio_parameters();

			struct addrinfo *res = NULL;
			int fd = connect_to_backend(&res);

			if (fd < 0) {
				LOG_ERR("Failed to connect to the backend, err: %d", fd);
				goto clean_up;
			}

			#if (DO_HTTP_GET == 1)
			err = get_datanodes(fd);

			if (err < 0) {
				LOG_ERR("Failed to get datanodes, err %d", err);
				goto clean_up;
			}
			#endif

			#if (DO_HTTP_POST == 1)
			err = upload_data(fd, &data);

			if (err < 0) {
				LOG_ERR("Failed to upload data, err %d", err);
				goto clean_up;
			}

			++num_uploads;
			#endif

			LOG_INF("Finished, closing socket.\n");

		clean_up:
			err = shutdown_nbiot_things(res, fd);

			if (err < 0) {
				LOG_WRN("Failed to shutdown NB-IoT things, but continuing anyway, err %d", err);
			}
		}

		k_msleep(1000);
		++iterations;
	} // End of main while loop...

	shutdown_lte_modem_library();

	k_msleep(100);
	return;
}
