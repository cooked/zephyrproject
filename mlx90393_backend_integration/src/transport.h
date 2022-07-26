#ifndef APP_TRANSPORT_H
#define APP_TRANSPORT_H

#include <stdbool.h>

#include <zephyr.h>

#include <net/net_ip.h>
#include <net/socket.h>
#include <net/http_client.h>

#define USE_HTTPS 1

#if (USE_HTTPS == 1)
#define HTTPS_PORT_STR "443"
#define HTTPS_PORT_INT 443
#else
#define HTTPS_PORT_STR "80"
#define HTTPS_PORT_INT 80
#endif // USE_HTTPS == 1

#define RECV_BUF_SIZE 1000u

// Options to store the response
enum RESPONSE_STORE_OPTION
{
	RESPONSE_STORE_ALL,
	RESPONSE_STORE_BODY_ONLY,
	RESPONSE_STORE_HTTP_STATUS_ONLY,
};

// Struct for passing useful information to the response_cb() function
typedef struct response_user_data
{
	struct k_sem http_done;

	char *response_buf;
	size_t response_buf_size;
	__uint32_t response_buf_index;

	enum RESPONSE_STORE_OPTION response_option;
	bool found_response;
} response_user_data_t;

/**
 * @brief Connects to the given server (hostname)
 *
 * @param hostname a null-terminated string which should hold the hostname. Can
 * be e.g. "google.com", or an IP address.
 * @param hostname_size the size of the hostname buffer string.
 * @param res output struct holding the address information for the provided
 * hostname. You must store it safely, and provide it back to
 * disconnect_server() to cleanly disconnect.
 *
 * NOTE: Ensure that the certificate and security tag(s) that you have
 * provisioned, correspond to the one used for this hostname, otherwise the
 * connecting will fail. NOTE: Ensure you have a working internet connection, so
 * that the connecting (e.g. via NB-IoT via connect_nbiot()) works!
 *
 * @returns int indicating an error if < 0, otherwise a file descriptor integer
 * which you must save, as it represents the socket connection. You must provide
 * it back to the disconnect_server() function to cleanly disconnect.
 */
int connect_server(const char *hostname, size_t hostname_size, struct addrinfo **res);

/**
 * @brief Disconnects from the given
 *
 * @param hostname a null-terminated string which should hold the hostname. Can
 * be e.g. "google.com", or an IP address.
 * @param hostname_size the size of the hostname buffer string.
 * @param res output struct holding the connection state. You must store it
 * safely, and provide it back to disconnect_server() to cleanly disconnect.
 *
 * NOTE: Ensure that the certificate and security tag(s) that you have
 * provisioned, correspond to the one used for this hostname, otherwise the
 * connecting will fail.
 * NOTE: Ensure you have a working internet connection, so
 * that the connecting (e.g. via NB-IoT via connect_nbiot()) works!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
void disconnect_server(struct addrinfo *res, int fd);

/**
 * @brief Prepares a response_user_data struct for the caller.
 *
 * The caller should only declare the struct and then pass it to this function.
 * Afterwards, fields can be overwritten according to your wishes (e.g. to store
 * the entire response, and not just the response body/content).
 *
 * @param user_data a pointer to a response_user_data struct
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int prepare_response_user_data(response_user_data_t *user_data);

/**
 * @brief Prepares a http_request struct for the caller.
 *
 * The caller should only declare the struct and then pass it to this function.
 * Afterwards, fields can be overwritten according to your wishes.
 *
 * @param request a pointer to a http_request struct
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int prepare_http_request(struct http_request *request);

/**
 * @brief Executes the given http request, blocking.
 *
 * @param request a pointer to a http_request struct
 * @param fd an int representing an open socket connection, e.g. from connect_server()
 * @param timeout_ms how many ms to wait for the request to complete
 * @param response_user_data a pointer to a response_user_data_t struct
 *
 * NOTE: Ensure you have a working and connected socket, so fd is valid!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int do_http_request(struct http_request *request, int fd, int32_t timeout_ms, response_user_data_t *response_user_data);

#endif // APP_TRANSPORT_H