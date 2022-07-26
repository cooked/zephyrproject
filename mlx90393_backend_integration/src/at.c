
#include <logging/log.h>
LOG_MODULE_REGISTER(AT, LOG_LEVEL_INF);

#include <string.h>

#include <zephyr.h>


#include <modem/at_cmd.h>
#include <modem/at_notif.h>

#include <at.h>


/* Initialize AT communications */
int at_comms_init(void)
{
	int err = at_cmd_init();
	if (err < 0)
	{
		LOG_ERR("Failed to initialize AT commands, err %d", err);
		return err;
	}

	// Do not remove this call, even though we don't make use of AT
	// notifications ourselves! It is required by the LTE Link Control driver,
	// otherwise the NB-IoT connection will never succeed!
	err = at_notif_init();
	if (err < 0)
	{
		LOG_ERR("Failed to initialize AT notifications, err %d", err);
		return err;
	}

	return 0;
}

int at_cmd_helper(const char *command, char *output_buffer, size_t output_buffer_size)
{
	if (!output_buffer)
	{
		LOG_ERR("Cannot execute AT command with an empty output buffer!");
		return -1;
	}

	memset(output_buffer, 0, output_buffer_size);
	int err = at_cmd_write(command, output_buffer, output_buffer_size, NULL);

	if (err < 0)
	{
		LOG_WRN("Failed to execute AT command (%s)!", log_strdup(command));
	}
	else
	{
		LOG_DBG("AT cmd (%s) (%d bytes): %s", log_strdup(command), strlen(output_buffer), log_strdup(output_buffer));
	}

	return err;
}

int at_cmd_helper_easy_mode(const char *command)
{
	const size_t buffer_size = 35;
	char buffer[buffer_size];
	return at_cmd_helper(command, buffer, buffer_size);
}

int at_cmd_helper_easy_mode_printer(const char *command, const size_t buffer_size)
{
	char buffer[buffer_size];
	int err = at_cmd_helper(command, buffer, buffer_size);

	if (err < 0) {
		LOG_WRN("Command failed, err %d!", err);
	} else {
		LOG_INF("%s", log_strdup(buffer));
	}

	return err;
}

int print_provisioned_certificates(void)
{
	// Execute AT command
	const char command[] = "AT%CMNG=1";

	// Retrieving the provisioned certificates can take a lot of space...
	const size_t buffer_size = 1000;

	LOG_INF("Provisioned certificates:");
	return at_cmd_helper_easy_mode_printer(command, buffer_size);
}

int print_modem_firmware_version(void)
{
	// Execute AT command
	const char command[] = "AT+CGMR";
	const size_t buffer_size = 25;

	LOG_INF("Modem firmware version:");
	return at_cmd_helper_easy_mode_printer(command, buffer_size);
}

int print_signal_conditions(void)
{
	const char command[] = "AT%CONEVAL";
	const size_t buffer_size = 75;

	LOG_INF("LTE signal conditions:");
	return at_cmd_helper_easy_mode_printer(command, buffer_size);
}

int print_magpio_parameters(void) {
	const char command[] = "AT%XMAGPIO?";
	const size_t buffer_size = 150;

	LOG_INF("Test command %s", log_strdup(command));
	return at_cmd_helper_easy_mode_printer(command, buffer_size);
}

int set_test_magpio_parameters(void) {
	const char command[] = "AT%XMAGPIO=1,1,1,7,1,746,803,2,698,748,2,1710,2200,3,824,894,4,880,960,5,791,849,7,1565,1586";
	const size_t buffer_size = 50;

	LOG_INF("Executing test command %s", log_strdup(command));
	return at_cmd_helper_easy_mode_printer(command, buffer_size);
}

int get_network_datetime(char *output_buffer, size_t output_buffer_size)
{
	// Execute AT command
	const char command[] = "AT%CCLK?";
	return at_cmd_helper(command, output_buffer, output_buffer_size);
}

int set_proper_network_datetime(void)
{
	// Extract modem time
	int ms_slept = 0;
	int miliseconds_to_sleep = 1;
	int err = 0;

	const size_t buffer_size = 50;
	char buffer[buffer_size];
	memset(buffer, 0, buffer_size);

	do
	{
		err = get_network_datetime(buffer, buffer_size);

		if (err < 0)
		{
			k_msleep(miliseconds_to_sleep);
			ms_slept += miliseconds_to_sleep;
		}
	} while (err < 0);

	LOG_DBG("After %d ms of sleep, network time is available", ms_slept);

	LOG_DBG("Timezone offset = %c%c%c", buffer[25], buffer[26], buffer[27]);

	// Prepare setting time back command
	const __uint32_t command_size = 33;
	char command[command_size];
	memset(command, '\0', command_size);

	// Add the AT command to set the time
	const char at_command_part[] = "AT%CCLK=";
	__uint32_t command_length = strlen(at_command_part);
	memcpy(command, at_command_part, command_length);

	// Use memcpy to copy over the datetime part of the buffer
	__uint32_t offset = strlen("+CCLK: ");
	__uint32_t datetime_length = strlen(buffer + offset) - 2; // ignore \r\n
	memcpy(command + command_length, buffer + offset, datetime_length);

	// Set it back with the proper timezone...
	command[26] = '+';
	command[27] = '0';
	command[28] = '1'; // We need +1 for NL, compared to GMT

	// Add in the daylight savings time and null terminator
	command[30] = ',';
	command[31] = '1'; // Currently we are on daylight savings time, 1 hour ahead
	command[32] = '\0';

	// And then execute the command
	return at_cmd_helper_easy_mode(command);
}