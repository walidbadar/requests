/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <requests/net/lib/requests.h>
#include "requests_private.h"
#include "requests_certs.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(requests_connect, CONFIG_REQUESTS_LOG_LEVEL);

#define NET_EVENT_L4_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

static struct net_mgmt_event_callback l4_cb;
static K_SEM_DEFINE(ipv4_wait, 0, 1);
static K_SEM_DEFINE(dns_wait, 0, 1);

static void net_event_l4_handler(struct net_mgmt_event_callback *cb, uint64_t status,
				 struct net_if *iface)
{
	switch (status) {
	case NET_EVENT_L4_CONNECTED:
		LOG_DBG("Network connection established (IPv4)");
		k_sem_give(&ipv4_wait);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		break;
	default:
		break;
	}
}

void requests_dns_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
	struct requests_ctx *ctx = (struct requests_ctx *)user_data;
	uint8_t hr_addr[NET_IPV4_ADDR_LEN];

	switch (status) {
	case DNS_EAI_CANCELED:
	case DNS_EAI_FAIL:
	case DNS_EAI_NODATA:
		LOG_ERR("DNS resolve (%d)", status);
		break;

	case DNS_EAI_ALLDONE:
		LOG_DBG("DNS resolved");
		k_sem_give(&dns_wait);
		break;

	default:
		break;
	}

	if (!info) {
		return;
	}

	if (info->ai_family == AF_INET) {
		ctx->sa = info->ai_addr;
		ctx->err = 0;
	} else {
		LOG_ERR("Invalid IP address family %d", info->ai_family);
		ctx->err = -EINVAL;
	}

	LOG_DBG("Host IPv4 address: %s",
		net_addr_ntop(info->ai_family, &net_sin(&ctx->sa)->sin_addr, hr_addr,
			      sizeof(hr_addr)));
}

int requests_dns_lookup(struct requests_ctx *ctx)
{
	int ret;
	uint16_t dns_id;

	if (!strlen(ctx->url_fields.hostname)) {
		LOG_ERR("Invalid hostname");
		return -EINVAL;
	}

	net_mgmt_init_event_callback(&l4_cb, net_event_l4_handler, NET_EVENT_L4_MASK);
	net_mgmt_add_event_callback(&l4_cb);
	conn_mgr_mon_resend_status();

	LOG_DBG("Waiting for IPv4 address assignment (DHCP/Static)");

	ret = k_sem_take(&ipv4_wait, K_MSEC(CONFIG_NET_SOCKETS_CONNECT_TIMEOUT));
	if (ret < 0) {
		LOG_ERR("Cannot resolve IPv4 address (%d)", ret);
		return ret;
	}

	ret = dns_get_addr_info(ctx->url_fields.hostname, DNS_QUERY_TYPE_A, &dns_id,
				requests_dns_cb, (void *)ctx, CONFIG_NET_SOCKETS_DNS_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("Cannot resolve DNS address (%d)", ret);
	} else {
		ret = k_sem_take(&dns_wait, K_MSEC(CONFIG_NET_SOCKETS_DNS_TIMEOUT));
		if (ret < 0) {
			return ret;
		}

		ret = ctx->err;
	}

	return ret;
}

static int requests_connect_setup(struct requests_ctx *ctx)
{
	int ret;

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		ctx->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
	} else {
		ctx->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}

	if (ctx->sockfd < 0) {
		LOG_ERR("Failed to create socket (%d)", -errno);
		return -errno;
	}

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		sec_tag_t sec_tag_list[] = {
			CA_CERTIFICATE_TAG,
		};

		if (ctx->is_ssl_verifyhost) {
			ret = setsockopt(ctx->sockfd, SOL_TLS, TLS_HOSTNAME,
						ctx->url_fields.hostname,
						sizeof(ctx->url_fields.hostname));
			if (ret < 0) {
				LOG_ERR("Failed to set TLS_HOSTNAME %s option (%d)",
					ctx->url_fields.hostname, ret);
				return ret;
			}
		}

		if (ctx->is_ssl_verifypeer) {
			ret = setsockopt(ctx->sockfd, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list,
					 sizeof(sec_tag_list));
			if (ret < 0) {
				LOG_ERR("Failed to set TLS_SEC_TAG_LIST option (%d)", ret);
				return ret;
			}
		} else {
			ret = setsockopt(ctx->sockfd, SOL_TLS, TLS_PEER_VERIFY,
					 (int *)&ctx->is_ssl_verifypeer, sizeof(int));
			if (ret < 0) {
				LOG_ERR("Failed to set TLS_PEER_VERIFY option (%d)", ret);
				return ret;
			}
		}
	}

	return 0;
}

int requests_connect(struct requests_ctx *ctx)
{
	int ret;

	ret = requests_connect_setup(ctx);
	if (ret < 0) {
		LOG_ERR("Cannot create socket (%d)", -errno);
		return -ECONNABORTED;
	}

	struct sockaddr_in *sa_in = (struct sockaddr_in *)&ctx->sa;
	sa_in->sin_port = htons(ctx->url_fields.port);
	sa_in->sin_family = AF_INET;

	ret = connect(ctx->sockfd, (struct sockaddr *)sa_in, sizeof(*sa_in));
	if (ret < 0) {
		LOG_ERR("Cannot connect to remote (%d)", -errno);
		close(ctx->sockfd);
		ctx->sockfd = -1;
		ret = -ECONNABORTED;
	}

	return ret;
}

static int requests_certs(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		ret = tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
					 ca_certificate, sizeof(ca_certificate));
		if (ret < 0) {
			LOG_ERR("Failed to register public certificate (%d)", ret);
			return ret;
		}
	}

	return 0;
}

SYS_INIT(requests_certs, APPLICATION, 95);
