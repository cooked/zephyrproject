#ifndef APP_BACKEND_H
#define APP_BACKEND_H

// TODO: Change these to make use of Kconfig things...
#define DO_HTTP_GET 1
#define DO_HTTP_POST 1

#define USE_JSON_DECODING 0
#define USE_JSON_ENCODING 0
#define MAKE_JSON_ENCODING_CRASH 0


#define HTTP_GET_RESOURCE "/datanodes/"
#define HTTP_POST_RESOURCE "/measurements/"

#define DEFAULT_HTTP_REQUEST_TIMEOUT_SECONDS 10


#define API_AUTHENTICATION_TOKEN "411c88b8187de9157af17a5b251384fda9e5bcc6"

#define HTTPS_HOSTNAME "api-dev.anything-connected.com"

#define FIXED_DN_CONFIG_ID 1
#define FIXED_DN_ASSET_ID 1

#define USE_FLOAT_ARRAY_HACK 1


#if (USE_JSON_ENCODING == 1)
#include <data/json.h>
#endif



// For the /datanodes HTTP GET call, we need at least 1842 bytes!
// We can get away with using less, if we do not store the response headers...
// TODO: Don't store response headers, only the response data
#define API_BUF_SIZE 2500

typedef struct measurement_data {
	float ax;
	float ay;
	float az;

	float gx;
	float gy;
	float gz;

	float mx;
	float my;
	float mz;

	float temp;
} measurement_data_t;



#if (USE_JSON_ENCODING == 1)
// Describe JSON data structures for the Datanodes API
// TODO: Fill it in...
struct datanode_api_asset {
	__uint32_t id;
	char* name;
};

static const struct json_obj_descr json_datanode_api_asset_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api_asset, id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_asset, name, JSON_TOK_STRING),
};


struct datanode_api_measurement_location {
	__uint32_t id;
	char* name;
};

static const struct json_obj_descr json_datanode_api_measurement_location_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api_measurement_location, id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_measurement_location, name, JSON_TOK_STRING),
};


struct datanode_api_sensors {
	__uint32_t id;
	char* name;
};

static const struct json_obj_descr json_datanode_api_sensors_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api_sensors, id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_sensors, name, JSON_TOK_STRING),
};


struct datanode_api_configuration {
	__uint32_t id;
	char* name;
	char** enabled_measurements;
};

static const struct json_obj_descr json_datanode_api_configuration_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api_configuration, id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_configuration, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_configuration, enabled_measurements, JSON_TOK_LIST_START),
};


struct datanode_api_result {
	__uint32_t id;
	char* name;
	struct datanode_api_asset asset;
	struct datanode_api_measurement_location measurement_location;
	char mac_address[18];	// Fixed length for 17-char MAC address + \0
	__uint32_t connection_type;
	char* last_active;
	bool connected;
	__uint32_t uptime;
	struct datanode_api_sensors* sensors;
	struct datanode_api_configuration configuration;
	__uint32_t asset_coord_x;
	__uint32_t asset_coord_y;
};

static const struct json_obj_descr json_datanode_api_result_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, name, JSON_TOK_STRING),

	JSON_OBJ_DESCR_OBJECT(struct datanode_api_result, asset, json_datanode_api_asset_descr),
	JSON_OBJ_DESCR_OBJECT(struct datanode_api_result, measurement_location, json_datanode_api_measurement_location_descr),

	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, mac_address, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, connection_type, JSON_TOK_NUMBER),

	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, last_active, JSON_TOK_STRING),

	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, connected, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, uptime, JSON_TOK_NUMBER),

	JSON_OBJ_DESCR_OBJECT(struct datanode_api_result, sensors, json_datanode_api_sensors_descr),
	JSON_OBJ_DESCR_OBJECT(struct datanode_api_result, configuration, json_datanode_api_configuration_descr),

	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, asset_coord_x, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api_result, asset_coord_y, JSON_TOK_NUMBER),
};


struct datanode_api {
	__uint32_t count;
	__uint32_t next;
	__uint32_t previous;
	struct datanode_api_result results[];
};

static const struct json_obj_descr json_datanode_api_main_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct datanode_api, count, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api, next, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct datanode_api, previous, JSON_TOK_NUMBER),

	JSON_OBJ_DESCR_OBJECT(struct datanode_api, results, json_datanode_api_result_obj_descr),
};



// Describe JSON data structures for the Measurements API
// TODO: Fill it in...

// static const struct json_obj_descr json_measurement_api_data_obj_descr[] = {
// 	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample_data, temp, JSON_TOK_NUMBER),
// };

// static const struct json_obj_descr json_measurement_api_obj_descr[] = {
// 	JSON_OBJ_DESCR_PRIM(struct measurement, time_measured, JSON_TOK_STRING),
// 	JSON_OBJ_DESCR_PRIM(struct measurement, asset_id, JSON_TOK_NUMBER),
// 	JSON_OBJ_DESCR_PRIM(struct measurement, data_node_configuration_id, JSON_TOK_NUMBER),
// 	JSON_OBJ_DESCR_OBJECT(struct measurement, data, json_measurement_api_data_obj_descr),
// };


// Describe JSON data structures for the Measurement API
struct measurement_api_sample_data {
	int mx;
	int my;
	int mz;
	int temp;
};

static const struct json_obj_descr json_measurement_api_sample_data_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample_data, mx, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample_data, my, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample_data, mz, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample_data, temp, JSON_TOK_NUMBER),
};

#define TIMESTAMP_LENGTH 60
struct measurement_api_sample
{
	// UTC timestamp takes 26 chars (no timezone) + 1 for the NULL terminator
#if (MAKE_JSON_ENCODING_CRASH == 1)
	char time_measured[TIMESTAMP_LENGTH];
#endif

	__uint32_t asset_id;
	__uint32_t data_node_configuration_id;
	struct measurement_api_sample_data data;
};

static const struct json_obj_descr json_measurement_api_sample_obj_descr[] = {
#if (MAKE_JSON_ENCODING_CRASH == 1)
	// JSON_OBJ_DESCR_PRIM(struct measurement_api_sample, time_measured, JSON_TOK_STRING),
#endif
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample, asset_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct measurement_api_sample, data_node_configuration_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct measurement_api_sample, data, json_measurement_api_sample_data_obj_descr),
};

#endif // USE_JSON_ENCODING == 1

/**
 * @brief Gets the list of datanodes from the backend
 *
 * Currently this function only retrieves the data and logs it, the parsing into
 * JSON is controlled via the define USE_JSON_DECODING, which does not work yet.
 * TODO: Implement the decoding via the JSON library...
 *
 * @param fd an int representing a socket connection
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int get_datanodes(int fd);

/**
 * @brief Uploads some (currently fake) measurement data to the backend
 *
 * TODO: Finish being able to receive a parameter with the measurement data to
 * upload etc
 *
 * @param fd an int representing a socket connection
 * @param data point to measurement_data_t struct holding data to upload
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int upload_data(int fd, measurement_data_t *data);

#endif // APP_BACKEND_H