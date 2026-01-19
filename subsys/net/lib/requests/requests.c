/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <requests/net/lib/requests.h>
#include "requests_private.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(requests, CONFIG_REQUESTS_LOG_LEVEL);

int requests_init(struct requests_ctx *ctx, const uint8_t *url)
{
	int ret;
	memset(ctx, 0, sizeof(struct requests_ctx));

	if (IS_ENABLED(CONFIG_REQUESTS_SSL_VERIFYHOST)) {
		ctx->is_ssl_verifyhost = true;
	}

	if (IS_ENABLED(CONFIG_REQUESTS_SSL_VERIFYPEER)) {
		ctx->is_ssl_verifypeer = true;
	}

	ret = requests_url_parser(ctx, url);
	if (ret < 0) {
		LOG_ERR("Failed to parse URL (%d)", ret);
		return ret;
	}

	ret = requests_dns_lookup(ctx);
	if (ret < 0) {
		LOG_ERR("DNS Failed to resolve (%d)", ret);
		return ret;
	}

	return 0;
}

void requests_setopt(struct requests_ctx *ctx, enum requests_options option, void *parameter)
{
	switch (option) {
	case REQUESTS_HTTPHEADERS:
		ctx->headers = (const char **)parameter;
		break;
	case REQUESTS_POSTFIELDS:
		strncpy(ctx->payload, (uint8_t *)parameter, sizeof(ctx->payload) - 1);
		break;
	case REQUESTS_PROTOCOL:
		strncpy(ctx->protocol, (uint8_t *)parameter, sizeof(ctx->protocol) - 1);
		break;
	case REQUESTS_SSL_VERIFYHOST:
		ctx->is_ssl_verifyhost = *((bool *)parameter);
		break;
	case REQUESTS_SSL_VERIFYPEER:
		ctx->is_ssl_verifypeer = *((bool *)parameter);
		break;
	case REQUESTS_WRITEFUNCTION:
		ctx->cb = (http_response_cb_t)parameter;
		break;
	default:
		break;
	}
}

int requests(struct requests_ctx *ctx, enum http_method method)
{
	int ret;
	struct http_request req;
	memset(&req, 0, sizeof(struct http_request));

	if (ctx == NULL) {
		LOG_ERR("Invalid argument");
		return -EINVAL;
	}

	ret = requests_connect(ctx);
	if (ret < 0) {
		LOG_ERR("Cannot connect to remote (%d)", -errno);
		return ret;
	}

	req.method = method;
	req.url = ctx->url_fields.uri;
	req.host = ctx->url_fields.hostname;
	req.header_fields = ctx->headers;
	req.protocol = ctx->protocol;
	req.response = ctx->cb;
	req.recv_buf = ctx->recv_buf;
	req.recv_buf_len = sizeof(ctx->recv_buf);

	if (method == HTTP_POST || method == HTTP_PUT) {
		req.payload = ctx->payload;
		req.payload_len = sizeof(ctx->payload);
	}

	ret = http_client_req(ctx->sockfd, &req, CONFIG_NET_SOCKETS_DNS_TIMEOUT, ctx);
	close(ctx->sockfd);

	return ret;
}
