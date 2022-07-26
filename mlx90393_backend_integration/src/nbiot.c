
#include <logging/log.h>
LOG_MODULE_REGISTER(nbiot, LOG_LEVEL_DBG);


#include <stdbool.h>

#include <zephyr.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>

#include <nbiot.h>
#include <cert.h>
#include <security.h>


/* Provision certificate to modem */
int cert_provision(void)
{
	int err;
	bool exists;
	uint8_t unused;

	err = modem_key_mgmt_exists(TLS_SEC_TAG,
								MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
								&exists, &unused);

	if (err < 0)
	{
		LOG_ERR("Failed to check for certificates err %d", err);
		return err;
	}

	if (exists)
	{
		/* For the sake of simplicity we delete what is provisioned
		 * with our security tag and reprovision our certificate.
		 */
		err = modem_key_mgmt_delete(TLS_SEC_TAG,
									MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err < 0)
		{
			LOG_ERR("Failed to delete existing certificate, err %d",
				   err);
		}
	}

	LOG_DBG("Provisioning certificate");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(TLS_SEC_TAG,
							   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
							   cert, sizeof(cert) - 1);
	if (err < 0)
	{
		LOG_ERR("Failed to provision certificate, err %d", err);
		return err;
	}

	return 0;
}

int setup_lte_modem_library() {
	int err = nrf_modem_lib_init(NORMAL_MODE);
	if (err < 0)
	{
		LOG_ERR("Failed to initialize modem library (%d)!", err);
		return err;
	}

	return 0;
}

int shutdown_lte_modem_library() {
	int err = nrf_modem_lib_shutdown();

	if (err < 0)
	{
		LOG_ERR("Failed to shutdown modem library (%d)!", err);
		return err;
	}

	err = lte_lc_deinit();

	if (err < 0) {
		LOG_ERR("Failed to deinit the LTE Link Control driver, err %d", err);
	}

	return 0;
}

int setup_nbiot() {

#if (DELETE_ALL_CERTS == 1)
	LOG_DBG("Deleting all certificates stored in the modem");

	// Delete all certificates up to and including our tag
	for (size_t i = 0; i < 100; i++) {
		modem_key_mgmt_delete(i, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		modem_key_mgmt_delete(i, MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT);
	}

	modem_key_mgmt_delete(4242424, MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT);
#endif

#if (PRINT_CERTS == 1)
	print_provisioned_certificates();
#endif

	/* Provision certificates before connecting to the LTE network */
#if (USE_HTTPS == 1)
	err = cert_provision();
	if (err < 0)
	{
		return err;
	}
#endif

#if (PRINT_CERTS == 1)
	print_provisioned_certificates();
#endif

	return 0;
}

int connect_nbiot() {
	LOG_INF("Waiting for network...");
	int err = lte_lc_init_and_connect();

	if (err < 0)
	{
		LOG_ERR("Failed to connect to the LTE network, err %d", err);
		return err;
	}

	LOG_INF("NB-IoT network connection established");
	return err;
}

int disconnect_nbiot() {
	lte_lc_offline();

	int err = lte_lc_deinit();

	if (err < 0) {
		LOG_ERR("Failed to deinit the LTE Link Control driver, err %d", err);
		return err;
	}

	return 0;
}