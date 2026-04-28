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
static K_SEM_DEFINE(l4_wait, 0, 1);
static K_SEM_DEFINE(dns_wait, 0, 1);

static void net_event_l4_handler(struct net_mgmt_event_callback *cb, uint64_t status,
				 struct net_if *iface)
{
	switch (status) {
	case NET_EVENT_L4_CONNECTED:
		LOG_DBG("Network connection established (IPv4)");
		k_sem_give(&l4_wait);
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

	switch (status) {
	case DNS_EAI_CANCELED:
	case DNS_EAI_FAIL:
	case DNS_EAI_NODATA:
		break;

	case DNS_EAI_ALLDONE:
		k_sem_give(&dns_wait);
		break;

	default:
		break;
	}

	if (info == NULL) {
		return;
	}

	if (info->ai_family != NET_AF_INET && info->ai_family != NET_AF_INET6) {
		LOG_ERR("Invalid IP address family (%d)", info->ai_family);
		ctx->err = -EINVAL;
		return;
	}

	ctx->sa = *(net_sas(&info->ai_addr));
	ctx->sa.ss_family = info->ai_family;
	ctx->err = 0;
}

static int requests_dns_get_info(struct requests_ctx *ctx, enum dns_query_type type)
{
	int ret;
	uint16_t dns_id;

	ret = dns_get_addr_info(ctx->url_fields.hostname, type, &dns_id, requests_dns_cb,
				(void *)ctx, CONFIG_NET_SOCKETS_DNS_TIMEOUT);
	if (ret < 0) {
		return ret;
	}

	ret = k_sem_take(&dns_wait, K_MSEC(CONFIG_NET_SOCKETS_DNS_TIMEOUT));
	if (ret < 0) {
		return ret;
	}

	return ctx->err;
}

int requests_dns_lookup(struct requests_ctx *ctx)
{
	int ret;

	if (!ctx->url_fields.hostname[0]) {
		LOG_ERR("Invalid hostname");
		return -EINVAL;
	}

	net_mgmt_init_event_callback(&l4_cb, net_event_l4_handler,
				     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&l4_cb);
	conn_mgr_mon_resend_status();

	ret = k_sem_take(&l4_wait, K_MSEC(CONFIG_NET_SOCKETS_CONNECT_TIMEOUT));
	if (ret < 0) {
		LOG_ERR("Failed to resolve IP address (%d)", ret);
		return ret;
	}

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		ret = requests_dns_get_info(ctx, DNS_QUERY_TYPE_A);
	}

	if (IS_ENABLED(CONFIG_NET_IPV6) && ctx->sa.ss_family == NET_AF_UNSPEC) {
		ret = requests_dns_get_info(ctx, DNS_QUERY_TYPE_AAAA);
	}

	return ret;
}

static int requests_connect_setup(struct requests_ctx *ctx)
{
	bool is_ssl = ctx->url_fields.is_ssl;

	ctx->sockfd = zsock_socket(ctx->sa.ss_family, NET_SOCK_STREAM,
				   is_ssl ? NET_IPPROTO_TLS_1_2 : NET_IPPROTO_TCP);
	if (ctx->sockfd < 0) {
		LOG_ERR("Failed to create socket (%d)", -ECONNABORTED);
		return -ECONNABORTED;
	}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	int ret;

	if (ctx->is_ssl_verifyhost && is_ssl) {
		ret = zsock_setsockopt(ctx->sockfd, ZSOCK_SOL_TLS, ZSOCK_TLS_HOSTNAME,
				       ctx->url_fields.hostname, strlen(ctx->url_fields.hostname));
		if (ret < 0) {
			LOG_ERR("Failed to set TLS_HOSTNAME %s option (%d)",
				 ctx->url_fields.hostname, ret);
			return ret;
		}
	}

	if (ctx->is_ssl_verifyhost == 0) {
		LOG_WRN("Hostname verification is disabled");

		ret = zsock_setsockopt(ctx->sockfd, ZSOCK_SOL_TLS, ZSOCK_TLS_HOSTNAME, NULL, 0);
		if (ret < 0) {
			LOG_ERR("Failed to set TLS_HOSTNAME %s option (%d)",
				 ctx->url_fields.hostname, ret);
			return ret;
		}
	}

	if (ctx->is_ssl_verifypeer && is_ssl) {
		ret = zsock_setsockopt(ctx->sockfd, ZSOCK_SOL_TLS, ZSOCK_TLS_SEC_TAG_LIST,
				       &ctx->sec_tag, sizeof(int));
		if (ret < 0) {
			LOG_ERR("Failed to set TLS_SEC_TAG_LIST option (%d)", ret);
			return ret;
		}
	}

	if (ctx->is_ssl_verifypeer == 0) {
		LOG_WRN("TLS verification is disabled");

		ret = zsock_setsockopt(ctx->sockfd, ZSOCK_SOL_TLS, ZSOCK_TLS_PEER_VERIFY,
				       &ctx->is_ssl_verifypeer, sizeof(int));
		if (ret < 0) {
			LOG_ERR("Failed to set TLS_PEER_VERIFY option (%d)", ret);
			return ret;
		}
	}
#endif

	return 0;
}

int requests_connect(struct requests_ctx *ctx)
{
	int ret;
	struct net_sockaddr_in *sa_in;
	struct net_sockaddr_in6 *sa_in6;

	ret = requests_connect_setup(ctx);
	if (ret < 0) {
		LOG_ERR("Cannot create socket (%d)", -errno);
		return -ECONNABORTED;
	}

	if (ctx->sa.ss_family == NET_AF_INET) {
		sa_in = (struct net_sockaddr_in *)(&ctx->sa);
		sa_in->sin_port = net_htons(ctx->url_fields.port);
	} else if (ctx->sa.ss_family == NET_AF_INET6) {
		sa_in6 = (struct net_sockaddr_in6 *)(&ctx->sa);
		sa_in6->sin6_port = net_htons(ctx->url_fields.port);
	} else {
		LOG_ERR("Invalid IP address family (%d)", ctx->sa.ss_family);
		return -EINVAL;
	}

	ret = zsock_connect(ctx->sockfd, net_sad(&ctx->sa), net_family2size(ctx->sa.ss_family));
	if (ret < 0) {
		LOG_ERR("Failed to connect to remote (%d)", -ECONNABORTED);
		zsock_close(ctx->sockfd);
		ctx->sockfd = -1;
		return -ECONNABORTED;
	}

	return 0;
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
