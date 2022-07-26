
#include <logging/log.h>
LOG_MODULE_REGISTER(transport, LOG_LEVEL_WRN);

#include <string.h>

#include <zephyr.h>

#include <net/net_ip.h>
#include <net/socket.h>
#include <net/tls_credentials.h>

#include <security.h>
#include <transport.h>


char recv_buf[RECV_BUF_SIZE] = {'\0'};



/* Setup TLS options on a given socket */
int tls_setup(int fd, const char *hostname, size_t hostname_size)
{
	int err;
	int verify;

	/* Security tag that we have provisioned the certificate with */
	const sec_tag_t tls_sec_tag[] = {
		TLS_SEC_TAG,
	};

	/* Set up TLS peer verification */
	enum
	{
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	verify = NONE;

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err < 0)
	{
		LOG_ERR("Failed to setup peer verification, err %d, errno: %d", err, -errno);
		return -errno;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag,
					 sizeof(tls_sec_tag));
	if (err < 0)
	{
		LOG_ERR("Failed to setup TLS sec tag, err %d, errno: %d", err, -errno);
		return -errno;
	}

	/* Setup SNI */
	err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, hostname, hostname_size);

	if (err)
	{
		LOG_ERR("Failed to setup TLS hostname \"%s\", err %d", log_strdup(hostname), errno);
		return err;
	}

	return 0;
}

void dump_addrinfo(const struct addrinfo *ai)
{
	LOG_DBG("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
		   "sa_family=%d, sin_port=%x",
		   ai, ai->ai_family, ai->ai_socktype, ai->ai_protocol,
		   ai->ai_addr->sa_family,
		   ((struct sockaddr_in *)ai->ai_addr)->sin_port);
}


static void response_cb(struct http_response *rsp,
						enum http_final_call final_data,
						void *user_data)
{
	size_t data_length = rsp->data_len;
	LOG_DBG("Data_length: %d, recv_buf size: %d", data_length, RECV_BUF_SIZE);

	struct response_user_data *response_info = (struct response_user_data *) user_data;

	// TODO: Ensure that the k_sem_give() call succeeds, regardless of whether
	// or not, or how much, we have copied data!

	// If no buffer given, don't store the response data
	if (!response_info->response_buf) {
		LOG_WRN("No buffer given to store the response data!");
		return;
	}

	// Ensure no buffer overflow
	if (response_info->response_buf_index >= response_info->response_buf_size
	|| response_info->response_buf_index + data_length >= response_info->response_buf_size) {
		LOG_ERR("Buffer overflow detected! Not copying anymore!");
		return;
	}

	if (data_length > RECV_BUF_SIZE) {
		LOG_ERR("Wtf http_client library! data_length(%u) >= RECV_BUF_SIZE(%u)!!! Truncating...",
			data_length, RECV_BUF_SIZE);
		data_length = RECV_BUF_SIZE;
		return;
	}

	if (data_length != 0) {
		size_t offset = 0u, length = data_length;
		char *found = recv_buf;

		// Determine how much to copy from the recv_buf response
		switch (response_info->response_option) {
			case RESPONSE_STORE_BODY_ONLY:
				if (!response_info->found_response) {
					// Look for the body response content
					found = strstr(recv_buf, "\r\n\r\n");

					if (found) {
						// Set found flag to ensure we copy everything next time
						response_info->found_response = true;

						// Adjust elements to copy, skip linebreaks
						offset = found - recv_buf + strlen("\r\n\r\n");
						length = data_length - offset;
					} else {
						// Nothing to copy (yet)
						length = 0u;
					}
				} else {
					// Else we keep the default and copy everything, as it's
					// part of the response body
				}

				break;

			case RESPONSE_STORE_HTTP_STATUS_ONLY:
				if (response_info->found_response) {
					// Already got it, don't copy anymore
					length = 0u;
				} else {
					// Look for the first linebreak
					found = strstr(recv_buf, "\r\n");

					if (found) {
						// Set found flag to ensure we copy everything next time
						response_info->found_response = true;

						// Adjust elements to copy
						length = found - recv_buf;
					} else {
						// No linebreak found yet, so we copy everything we got
						// so far... This is not likely to happen though...
						LOG_WRN("No linebreak found yet...");
					}
				}

				break;

			// Otherwise, copy everything
			case RESPONSE_STORE_ALL:
			default:
				break;
		}

		if (length != 0) {
			memcpy(response_info->response_buf + response_info->response_buf_index, recv_buf + offset, length);
			response_info->response_buf_index += length;
			LOG_DBG("Copied %d bytes, response_buf_index is now: %u\n",
				length, response_info->response_buf_index);
		}

		LOG_DBG("recv_buf:\n%s", log_strdup(recv_buf));
	}

	// Clear recv_buf for next iteration...
	memset(recv_buf, 0, RECV_BUF_SIZE);

	if (final_data == HTTP_DATA_MORE)
	{
		LOG_DBG("Partial data received (%zd bytes)", data_length);
	}
	else if (final_data == HTTP_DATA_FINAL)
	{
		LOG_DBG("All the data received (%zd bytes)", data_length);
		k_sem_give(&response_info->http_done);
	}
}


void disconnect_server(struct addrinfo *res, int fd) {
	freeaddrinfo(res);

	if (fd != -1 ) {
		int err = close(fd);

		if (err < 0) {
			LOG_ERR("Error closing fd, err: %d", err);
		}
	}

	return;
}

int connect_server(const char *hostname, size_t hostname_size, struct addrinfo **res) {
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	// err = getaddrinfo(HTTPS_HOSTNAME, HTTPS_PORT_STR, &hints, &res);
	int err = getaddrinfo(hostname, NULL, &hints, res);

	if (err < 0)
	{
		LOG_ERR("getaddrinfo() failed, err %d", err);
		return err;
	}

	// dump_addrinfo(res);

	((struct sockaddr_in *)(*res)->ai_addr)->sin_port = htons(HTTPS_PORT_INT);

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (fd < 0)
	{
		LOG_ERR("Failed to open socket! fd = %d", fd);
		return fd;
	}

#if (USE_HTTPS == 1)
	/* Setup TLS socket options */
	err = tls_setup(fd, hostname, hostname_size);
	if (err < 0)
	{
		return err;
	}
#else
	/* Set up without TLS peer verification */
	enum
	{
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	int verify = NONE;

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err < 0)
	{
		LOG_ERR("Failed to setup peer verification, err %d, errno: %d", err, -errno);
		return -errno;
	}
#endif

	LOG_INF("Connecting to %s", log_strdup(hostname));
	err = connect(fd, (*res)->ai_addr, sizeof(struct sockaddr_in));
	if (err < 0)
	{
		LOG_ERR("connect() failed, err: %d, errno: %d", err, -errno);
		return -errno;
	}

	return fd;
}

// These function are currently not used... TODO: Remove them?
#if 0

static int setup_socket(sa_family_t family, const char *server, int port,
						int *sock, struct sockaddr *addr, socklen_t addr_len)
{
	const char *family_str = family == AF_INET ? "IPv4" : "IPv6";
	int ret = 0;

	memset(addr, 0, addr_len);

	if (family == AF_INET)
	{
		net_sin(addr)->sin_family = AF_INET;
		net_sin(addr)->sin_port = htons(port);
		inet_pton(family, server, &net_sin(addr)->sin_addr);
	}
	else
	{
		net_sin6(addr)->sin6_family = AF_INET6;
		net_sin6(addr)->sin6_port = htons(port);
		inet_pton(family, server, &net_sin6(addr)->sin6_addr);
	}

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS))
	{
		sec_tag_t sec_tag_list[] = {
			TLS_SEC_TAG,
		};

		*sock = socket(family, SOCK_STREAM, IPPROTO_TLS_1_2);
		if (*sock >= 0)
		{
			ret = setsockopt(*sock, SOL_TLS, TLS_SEC_TAG_LIST,
							 sec_tag_list, sizeof(sec_tag_list));
			if (ret < 0)
			{
				LOG_ERR("Failed to set %s secure option (%d)",
						family_str, ret);
				ret = -errno;
			}

			ret = setsockopt(*sock, SOL_TLS, TLS_HOSTNAME,
							 HTTPS_HOSTNAME,
							 sizeof(HTTPS_HOSTNAME));
			if (ret < 0)
			{
				LOG_ERR("Failed to set %s TLS_HOSTNAME "
						"option (%d)",
						family_str, ret);
				ret = -errno;
			}
		}
	}
	else
	{
		*sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
	}

	if (*sock < 0)
	{
		LOG_ERR("Failed to create %s HTTP socket (%d)", family_str,
				-errno);
	}

	return ret;
}

static int connect_socket(sa_family_t family, const char *server, int port,
						  int *sock, struct sockaddr *addr, socklen_t addr_len)
{
	int ret;

	ret = setup_socket(family, server, port, sock, addr, addr_len);
	if (ret < 0 || *sock < 0)
	{
		return -1;
	}

	ret = connect(*sock, addr, addr_len);
	if (ret < 0)
	{
		LOG_ERR("Cannot connect to %s remote (err: %d, errno: %d)",
				family == AF_INET ? "IPv4" : "IPv6",
				ret, -errno);
		ret = -errno;
	}

	return ret;
}

#endif


int prepare_response_user_data(struct response_user_data *user_data) {
	if (!user_data) {
		return -1;
	}

	memset(user_data, 0, sizeof(struct response_user_data));

	// user_data->http_done = {}; // Already covered by memset above
	user_data->response_buf = NULL;
	user_data->response_buf_size = 0;
	user_data->response_buf_index = 0;
	user_data->response_option = RESPONSE_STORE_BODY_ONLY;
	user_data->found_response = false;

	return k_sem_init(&(user_data->http_done), 0, 1);
}

int prepare_http_request(struct http_request *request) {
	if (!request) {
		return -1;
	}

	memset(request, 0, sizeof(struct http_request));

	request->protocol = "HTTP/1.1";
	request->response = response_cb;
	request->recv_buf = recv_buf;
	request->recv_buf_len = RECV_BUF_SIZE;

	return 0;
}

int do_http_request(struct http_request *request, int fd, int32_t timeout_ms, struct response_user_data *response_user_data) {
	if (!request || !response_user_data) {
		LOG_ERR("Request and/or response_user_data is NULL!");
		return -1;
	}

	if (fd < 0) {
		LOG_ERR("Invalid socket/fd number given, %d!", fd);
		return fd;
	}

	int err = http_client_req(fd, request, timeout_ms, response_user_data);

	if (err < 0) {
		LOG_ERR("Error doing http request, err %d", err);
		return err;
	}

	LOG_DBG("Http client req bytes send: %d", err);

	/* Print HTTP response */
	LOG_DBG("Waiting for request semaphore...");
	k_timeout_t timeout_sem = K_MSEC(60 * MSEC_PER_SEC);

	err = k_sem_take(&(response_user_data->http_done), timeout_sem);

	LOG_DBG("Sem take err: %d", err);

	if (err == -EAGAIN) {
		LOG_ERR("Error, timed out waiting for semaphore!");
		return err;
	}

	return 0;
}