#ifndef APP_NBIOT_H
#define APP_NBIOT_H



#define DELETE_ALL_CERTS 0
#define PRINT_CERTS 0

/**
 * Sets up everything needed to enable the usage of the LTE modem library.
 *
 * This function does not actually enable modem capabilities yet, for that you
 * will need the setup_nbiot() function.
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int setup_lte_modem_library();

/**
 * Shuts down the LTE modem library
 *
 * Afterwards, the LTE modem driver is powered off, and you will need to call
 * setup_lte_modem() again to reconnect to the network.
 */
int shutdown_lte_modem_library();

/**
 * Sets up everything needed to enable the usage of NB-IoT.
 *
 * This function does not actually enable NB-IoT capabilities yet, for that you
 * will need the connect_nbiot() function.
 *
 * NOTE: Ensure you have setup the LTE modem (setup_lte_modem() before calling
 * this function!!!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int setup_nbiot();


/**
 * Connects to the NB-IoT network.
 *
 * This is a blocking function, which puts the nRF9160 CPU to sleep while the
 * modem connects to the NB-IoT network. This means that this function may take
 * a long time to connect... or even fail.
 *
 * NOTE: Ensure you have called setup_nbiot() once before calling this function!
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int connect_nbiot();


/**
 * Disconnects from the NB-IoT network.
 *
 * This is a blocking function, which puts the nRF9160 CPU to sleep while the
 * modem disconnects from the NB-IoT network, but usually does not take long.
 * Afterwards, the LTE modem is powered off, and you will need to call
 * connect_nbiot() again to reconnect to the network.
 *
 * @returns int indicating an error if < 0, succes otherwise.
 */
int disconnect_nbiot();

#endif // APP_NBIOT_H