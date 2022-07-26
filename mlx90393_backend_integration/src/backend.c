// TODO: Change these, and other (duplicate) files, to become part of a
// driver/library etc, so we can do code-reuse rather than duplicate code files.


#include <logging/log.h>
LOG_MODULE_REGISTER(backend, LOG_LEVEL_DBG);

#include <stdio.h> // for snprintf()
#include <string.h>

#include <zephyr.h>

#include <net/net_ip.h>
#include <net/socket.h>
#include <net/http_client.h>


#if (USE_JSON_ENCODING == 1)
#include <data/json.h>
#endif

#include <at.h>
#include <transport.h>
#include <backend.h>

static const char *application_headers[] = {
	"Authorization: Token " API_AUTHENTICATION_TOKEN "\r\n",
	NULL,
};

static char api_buf[API_BUF_SIZE] = { '\0' };

// TODO: Remove these hardcoded values when the JSON parsing of the GET call
// works...
const __uint32_t DN_CONFIG_ID = FIXED_DN_CONFIG_ID;
const __uint32_t DN_ASSET_ID = FIXED_DN_ASSET_ID;



int get_time_measured(char *buffer) {
	if (!buffer) {
		LOG_ERR("Null pointer exception avoided, buffer is NULL!");
		return -1;
	}

	const size_t at_buffer_size = 50;
	char at_buffer[at_buffer_size];
	memset(at_buffer, 0, at_buffer_size);

	int err = get_network_datetime(at_buffer, at_buffer_size);

	if (err < 0) {
		LOG_ERR("Failed to get network time, err %d", err);
		return err;
	}

	// Convert to time_measured format (UTC), which is:
	// YYYY-MM-DD hh:mm:ss.ususus
	// e.g.:
	// 2022-02-25 08:18:52.549211

	// Add in the 2 extra leading year digits
	buffer[0] = '2';
	buffer[1] = '0';

	// Copy over the rest of the date and time
	__uint32_t offset = strlen("+CCLK: \""); // Also ignore the first "
	__uint32_t datetime_length = strlen("YY/MM/DD,hh:mm:ss");
	memcpy(buffer + 2, at_buffer + offset, datetime_length);

	// Change / to - for the date part
	buffer[4] = '-';
	buffer[7] = '-';

	// Replace the comma between date and time with a space
	buffer[10] = ' ';

	// Add in microseconds
	buffer[19] = '.';

	// TODO: Actually get microseconds...
	for (size_t i = 0; i < 6; ++i) {
		buffer[20 + i] = '0';
	}

	return 0;
}


int hack_float_value_into_string(char *buffer, size_t buffer_size, size_t offset, float value) {
	// Figure out how many bytes are needed, excluding the null terminator
	int length = snprintf(NULL, 0, "%f", value);

	// Make a backup of the character which would be overwritten by the null
	// terminator added by snprintf()
	size_t backup_offset = offset + length;
	char backup = buffer[backup_offset];

	// Format the float into the buffer
	int err = snprintf(buffer + offset, buffer_size - offset, "%f", value);

	// Restore backup character, remove null terminator
	buffer[backup_offset] = backup;

	return err;
}

void json_test(void) {
#if (USE_JSON_ENCODING == 1)
	// Try to parse the JSON
	char test[] = "{\"count\":2,\"next\":null,\"previous\":null,\"results\":[{\"id\":2,\"name\":\"Data Node 2.0.1\",\"asset\":{\"id\":4,\"name\":\"Anything Connected Office\"},\"measurement_location\":{\"id\":4,\"name\":\"Anything Connected Office\"},\"mac_address\":\"F7:5A:A0:88:97:38\",\"connection_type\":20,\"last_active\":\"2022-03-23T11:19:09.000000\",\"connected\":false,\"uptime\":100,\"sensors\":[],\"configuration\":{\"id\":3,\"name\":\"Data Node 2.0.1 config\",\"enabled_measurements\":[\"ax\",\"ay\",\"az\",\"gx\",\"gy\",\"gz\",\"mx\",\"my\",\"mz\",\"temp\"]},\"asset_coord_x\":null,\"asset_coord_y\":null},{\"id\":1,\"name\":\"Dev DN\",\"asset\":{\"id\":1,\"name\":\"Default Asset\"},\"measurement_location\":{\"id\":1,\"name\":\"Default Asset\"},\"mac_address\":\"AA:BB:CC:DD:EE:FF\",\"connection_type\":10,\"last_active\":\"2022-04-18T12:49:51.372061\",\"connected\":true,\"uptime\":100,\"sensors\":[],\"configuration\":{\"id\":1,\"name\":\"Default for Dev DN\",\"enabled_measurements\":[\"ax\",\"ay\",\"az\",\"gx\",\"gy\",\"gz\",\"mx\",\"my\",\"mz\",\"temp\"]},\"asset_coord_x\":null,\"asset_coord_y\":null}]}{\"count\":2,\"next\":null,\"previous\":null,\"results\":[{\"id\":2,\"name\":\"Data Node 2.0.1\",\"asset\":{\"id\":4,\"name\":\"Anything Connected Office\"},\"measurement_location\":{\"id\":4,\"name\":\"Anything Connected Office\"},\"mac_address\":\"F7:5A:A0:88:97:38\",\"connection_type\":20,\"last_active\":\"2022-03-23T11:19:09.000000\",\"connected\":false,\"uptime\":100,\"sensors\":[],\"configuration\":{\"id\":3,\"name\":\"Data Node 2.0.1 config\",\"enabled_measurements\":[\"ax\",\"ay\",\"az\",\"gx\",\"gy\",\"gz\",\"mx\",\"my\",\"mz\",\"temp\"]},\"asset_coord_x\":null,\"asset_coord_y\":null},{\"id\":1,\"name\":\"Dev DN\",\"asset\":{\"id\":1,\"name\":\"Default Asset\"},\"measurement_location\":{\"id\":1,\"name\":\"Default Asset\"},\"mac_address\":\"AA:BB:CC:DD:EE:FF\",\"connection_type\":10,\"last_active\":\"2022-04-18T12:49:51.372061\",\"connected\":true,\"uptime\":100,\"sensors\":[],\"configuration\":{\"id\":1,\"name\":\"Default for Dev DN\",\"enabled_measurements\":[\"ax\",\"ay\",\"az\",\"gx\",\"gy\",\"gz\",\"mx\",\"my\",\"mz\",\"temp\"]},\"asset_coord_x\":null,\"asset_coord_y\":null}]}";

	struct datanode_api datanode_results_test;
	int err = json_obj_parse(test, strlen(test),
		json_datanode_api_main_obj_descr,
		ARRAY_SIZE(json_datanode_api_main_obj_descr),
		&datanode_results_test);

	if (err < 0) {
		LOG_ERR("JSON parse error! %d", err);

		// Try this:
		struct aids {
			__uint32_t count;
		};

		const struct json_obj_descr aids_descr[] = {
			JSON_OBJ_DESCR_PRIM(struct aids, count, JSON_TOK_NUMBER),
		};

		char aids_test_string[] = "{\"count\":123}";

		struct aids aids_test;
		err = json_obj_parse(aids_test_string, strlen(aids_test_string),
			aids_descr,
			ARRAY_SIZE(aids_descr),
			&aids_test);

		if (err < 0) {
			LOG_ERR("JSON parse error again! %d", err);
		} else {
			LOG_INF("Json parse successful! Count: %d", aids_test.count);
		}
	}
#endif
}


int get_datanodes(int fd) {
	json_test();

	response_user_data_t http_get_call;
	int err = prepare_response_user_data(&http_get_call);

	if (err < 0) {
		LOG_ERR("Failed to prepare user data struct, err %d", err);
		return err;
	}

	memset(api_buf, '\0', API_BUF_SIZE);

	http_get_call.response_option = RESPONSE_STORE_BODY_ONLY;
	http_get_call.response_buf = api_buf;
	http_get_call.response_buf_size = API_BUF_SIZE;

	struct http_request get_req;
	err = prepare_http_request(&get_req);

	if (err < 0) {
		LOG_ERR("Failed to prepare HTTP request, err %d", err);
		return err;
	}

	get_req.method = HTTP_GET;
	get_req.url = HTTP_GET_RESOURCE;
	get_req.host = HTTPS_HOSTNAME;
	get_req.header_fields = application_headers;

	int32_t get_timeout = DEFAULT_HTTP_REQUEST_TIMEOUT_SECONDS * MSEC_PER_SEC;

	err = do_http_request(&get_req, fd, get_timeout, &http_get_call);

	if (err < 0) {
		LOG_ERR("Failed to do HTTP GET request to retrieve datanodes, err %d", err);
		return err;
	}

	LOG_INF("Murt, GET response:");
	LOG_INF("%s", log_strdup(api_buf));

#if (USE_JSON_DECODING == 1)
	// Try to parse the JSON
	struct datanode_api datanode_results;
	err = json_obj_parse(api_buf, http_get_call.response_buf_index - 1,
		json_datanode_api_main_obj_descr,
		ARRAY_SIZE(json_datanode_api_main_obj_descr),
		&datanode_results);

	if (err < 0) {
		LOG_ERR("JSON parse error! %d, but continuing anyway", err);
	}
#endif

	return 0;
}

int upload_data(int fd, measurement_data_t *data) {
	if (!data) {
		LOG_ERR("Given data pointer is NULL, nothing to upload!");
		return -1;
	}

	response_user_data_t http_post_call;
	int err = prepare_response_user_data(&http_post_call);

	if (err < 0) {
		LOG_ERR("Failed to prepare user data struct, err %d", err);
		return err;
	}

	memset(api_buf, '\0', API_BUF_SIZE);

	http_post_call.response_option = RESPONSE_STORE_HTTP_STATUS_ONLY;
	http_post_call.response_buf = api_buf;
	http_post_call.response_buf_size = API_BUF_SIZE;

	struct http_request post_req;
	err = prepare_http_request(&post_req);

	if (err < 0) {
		LOG_ERR("Failed to prepare HTTP request, err %d", err);
		return err;
	}

	const char *headers[] = {
		"Content-Type: application/json\r\n",
		NULL,
	};

	post_req.method = HTTP_POST;
	post_req.url = HTTP_POST_RESOURCE;
	post_req.host = HTTPS_HOSTNAME;
	post_req.header_fields = application_headers;
	post_req.optional_headers = headers;

	// Add payload

#if (USE_JSON_ENCODING == 1)
	struct measurement_api_sample measurement_data = {
#if (MAKE_JSON_ENCODING_CRASH == 1)
		// .time_measured = "123",
#endif
		.asset_id = DN_ASSET_ID,
		.data_node_configuration_id = DN_CONFIG_ID,
		.data = {
			.mx = 1,
			.my = 2,
			.mz = 3,
			.temp = 4,
		},
	};

	err = json_obj_encode_buf(
		&json_measurement_api_sample_obj_descr,
		ARRAY_SIZE(json_measurement_api_sample_obj_descr),
		&measurement_data,
		api_buf,
		API_BUF_SIZE);

	if (err < 0) {
		LOG_ERR("Failed to encode json! %d", err);
	}

	post_req.payload = api_buf;
#else
	// Fake data payload:
	// char payload[] = "{\"time_measured\":\"2022-04-26 13:37:42.123456\", \"asset_id\":1, \"data_node_configuration\":1, \"data\":{\"ax\":12345.12345678,\"ay\":12345.12345678,\"az\":12345.12345678,\"gx\":12345.12345678,\"gy\":12345.12345678,\"gz\":12345.12345678,\"mx\":12345.12345678,\"my\":12345.12345678,\"mz\":12345.12345678,\"temp\":12345.12345678}}";

	// Pre-JSON encoded data payload
	// NOTE: This assumes that floats are always in the range -xxxxx.xxxxxxxx to
	// xxxxx.xxxxxxxx (5 decimals for the integer, and 8 decimals for the
	// fraction), plus one character for a potential minus sign. This assumption
	// may prove invalid...
	char payload[] = "{\"time_measured\":\"2022-04-26 13:37:42.123456\", \"asset_id\":1, \"data_node_configuration\":1, \"data\":{\"ax\":               ,\"ay\":               ,\"az\":               ,\"gx\":               ,\"gy\":               ,\"gz\":               ,\"mx\":               ,\"my\":               ,\"mz\":               ,\"temp\":               }}";
	size_t payload_size = strlen(payload);

	// Hack in the time_measured by overwriting the value above (same length)
	// This is a terrible hack, but yes, it works...
	// TODO: Fix it by using the JSON encoding library?
	err = get_time_measured(payload + 18); // remember, \" is 1 character...

	if (err < 0) {
		LOG_ERR("Error, failed to hack in the time_measured, err %d", err);
	}

#if (USE_FLOAT_ARRAY_HACK == 1)
	// Cast the struct to a float*, so we can use pointer arithmetic
	float *data_float = (float *) data;

	// Do all 3-axis sensors first: accel, gyro, mag
	for (size_t i = 0; i < 9; i++)
	{
		err = hack_float_value_into_string(payload, payload_size, 103 + i * 21, data_float[i]);

		if (err < 0) {
			LOG_ERR("Failed to encode value %f at index %u", data_float[i], i);
		}
	}

	// Then add in the temp
	err = hack_float_value_into_string(payload, payload_size, 294, data->temp);

	if (err < 0) {
		LOG_ERR("Failed to encode temp value %f at index %u", data->temp, 294);
	}
#else
	// Hack in the measurements... This is a terrible hack, but it works...
	// TODO: Fix it by using the JSON encoding library?
	float mx = 123.456, my = 456.789, mz = 789.123;

	// Accelerometer xyz
	err = hack_float_value_into_string(payload, payload_size, 103, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode ax, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 124, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode ay, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 145, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode az, err %d", err);
	}

	// Gyrometer xyz
	err = hack_float_value_into_string(payload, payload_size, 166, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode gx, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 187, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode gy, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 208, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode gz, err %d", err);
	}

	// Magnetometer xyz
	err = hack_float_value_into_string(payload, payload_size, 229, mx);

	if (err < 0) {
		LOG_ERR("Error, failed to encode mx, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 250, my);

	if (err < 0) {
		LOG_ERR("Error, failed to encode my, err %d", err);
	}

	err = hack_float_value_into_string(payload, payload_size, 271, mz);

	if (err < 0) {
		LOG_ERR("Error, failed to encode mz, err %d", err);
	}

	// Temperature
	err = hack_float_value_into_string(payload, payload_size, 294, 12345.12345678);

	if (err < 0) {
		LOG_ERR("Error, failed to encode temp, err %d", err);
	}
#endif

	post_req.payload = payload;

#endif

	post_req.payload_len = strlen(post_req.payload);
	LOG_DBG("Payload (%d bytes): %s", post_req.payload_len, log_strdup(post_req.payload));

	int32_t timeout_post_req = DEFAULT_HTTP_REQUEST_TIMEOUT_SECONDS * MSEC_PER_SEC;

	err = do_http_request(&post_req, fd, timeout_post_req, &http_post_call);

	if (err < 0) {
		LOG_ERR("Failed to do HTTP POST request to upload data, err %d", err);
		return err;
	}

	LOG_DBG("Http client bytes sent: %d", err);

	LOG_INF("Murt, POST response:");
	LOG_INF("%s\n", log_strdup(api_buf));

	return 0;
}