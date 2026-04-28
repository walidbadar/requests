/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(requests_certificate, CONFIG_REQUESTS_LOG_LEVEL);

static const uint8_t ca_certificate[] = {
#include "certs.der.inc"
};

static int requests_certificate(void)
{
	int ret;

	ret = tls_credential_add(CONFIG_REQUESTS_CERTIFICATE_TAG,
				 TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate,
				 sizeof(ca_certificate));
	if (ret < 0) {
		LOG_ERR("Failed to register public certificate (%d)", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(requests_certificate, APPLICATION, 0);
