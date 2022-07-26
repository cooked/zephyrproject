#ifndef APP_AT_H
#define APP_AT_H


/**
 * @brief Sets up everything needed to enable the usage of AT modem commands.
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int at_comms_init(void);

/**
 * @brief Executes a given AT command to the modem.
 *
 * @param command a null-terminated string holding an AT command, e.g. AT+CMD
 * @param output_buffer a buffer to store the AT response in
 * @param output_buffer_size the size of the output buffer
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int at_cmd_helper(const char *command, char *output_buffer, size_t output_buffer_size);

/**
 * @brief Helper function to easily execute an AT command to the modem.
 *
 * This function internally calls the at_cmd_helper(), but the output of the AT
 * command is lost after the function returns. You can set this module's logging
 * level to DBG to view it, or use the at_cmd_helper() function to preserve the
 * output.
 *
 * @param command a null-terminated string holding an AT command, e.g. AT+CMD
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int at_cmd_helper_easy_mode(const char *command);

/**
 * @brief Helper function to easily execute an AT command to the modem.
 *
 * The same as at_cmd_helper_easy_mode(), except you now have the ability to
 * specify a buffer size before hand, if the default of the aforementioned
 * function is not enough.
 *
 * @param command a null-terminated string holding an AT command, e.g. AT+CMD
 * @param buffer_size output buffer size for holding and printing the result
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int at_cmd_helper_easy_mode_printer(const char *command, const size_t buffer_size);

/**
 * @brief Prints the currently provisioned modem certificates.
 *
 * Set this module's logging level to INF to view the certificates
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int print_provisioned_certificates(void);

/**
 * @brief Prints the current modem firmware version.
 *
 * Set this module's logging level to INF to view the output
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int print_modem_firmware_version(void);

/**
 * @brief Prints the current signal conditions.
 *
 * Set this module's logging level to INF to view the output
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int print_signal_conditions(void);

/**
 * @brief Prints the current AT%XMAGPIO parameters.
 *
 * Set this module's logging level to INF to view the output
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int print_magpio_parameters(void);

/**
 * @brief Sets the current AT%XMAGPIO parameters to some fixed test values.
 *
 * Set this module's logging level to INF to view the output
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int set_test_magpio_parameters(void);

/**
 * @brief Retrieves the current network datetime as known by the LTE
 * modem/network.
 *
 * Note that this function will fail if the network date and time are not yet
 * available to the LTE modem! It can take a few miliseconds for it to be
 * available...
 *
 * @param output_buffer a buffer to store the network datetime in
 * @param output_buffer_size the size of the output buffer
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 * NOTE: You may want to call set_proper_network_datetime() first, to ensure
 * that the datetime returned from this function makes (more) sense.
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int get_network_datetime(char *output_buffer, size_t output_buffer_size);

/**
 * @brief Sets the proper network date and time back to the LTE modem.
 *
 * NOTE: Right now, this done a bit hacky. It assumes that the datetime from
 * the network is considered GMT (supposedly defined by 3GPP spec), and it is
 * accurate regarding the date and time values. Then, it hardcodes the correct
 * timezone and daylight savings time for the Netherlands, and writes the
 * datetime back to the LTE modem...
 *
 * NOTE: Ensure you have setup AT communications (with the modem) before calling
 * this function, via at_comms_init()!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int set_proper_network_datetime(void);

#endif // APP_AT_H