/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <requests/net/lib/requests.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(requests_shell, CONFIG_REQUESTS_LOG_LEVEL);

static int http_response_handler(struct http_response *rsp, enum http_final_call final_data,
				 void *user_data)
{
	struct requests_ctx *ctx = (struct requests_ctx *)user_data;
	ctx->status_code = rsp->http_status_code;

	if (!ctx->status_code) {
		shell_warn(ctx->sh, "HTTP Status Code: %d", ctx->status_code);
		return 0;
	}

	if (rsp->body_frag_len) {
		shell_print(ctx->sh, "%.*s", rsp->body_frag_len, rsp->body_frag_start);
	}

	return 0;
}

static int cmd_requests_get(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (argc < 2) {
		shell_error(sh, "Usage: requests get <url>");
		return -EINVAL;
	}

	struct requests_ctx ctx;
	ret = requests_init(&ctx, argv[1]);
	if (ret < 0) {
		shell_error(sh, "Failed to initialize requests (%d)", ret);
		return ret;
	}

	ctx.sh = (struct shell *)sh;
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);

	ret = requests(&ctx, HTTP_GET);
	if (ret < 0) {
		shell_error(sh, "GET request failed (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_requests_post(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (argc < 3) {
		shell_error(sh, "Usage: requests post <url> <body>");
		return -EINVAL;
	}

	struct requests_ctx ctx;
	ret = requests_init(&ctx, argv[1]);
	if (ret < 0) {
		shell_error(sh, "Failed to initialize requests (%d)", ret);
		return ret;
	}

	ctx.sh = (struct shell *)sh;
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS, argv[2]);

	ret = requests(&ctx, HTTP_POST);
	if (ret < 0) {
		shell_error(sh, "POST request failed (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_requests_put(const struct shell *sh, size_t argc, char **argv)
{
	int ret;
	if (argc < 3) {
		shell_error(sh, "Usage: requests put <url> <body>");
		return -EINVAL;
	}
	struct requests_ctx ctx;
	ret = requests_init(&ctx, argv[1]);
	if (ret < 0) {
		shell_error(sh, "Failed to initialize requests (%d)", ret);
		return ret;
	}

	ctx.sh = (struct shell *)sh;
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS, argv[2]);

	ret = requests(&ctx, HTTP_PUT);
	if (ret < 0) {
		shell_error(sh, "PUT request failed (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_requests_delete(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (argc < 2) {
		shell_error(sh, "Usage: requests delete <url>");
		return -EINVAL;
	}

	struct requests_ctx ctx;
	ret = requests_init(&ctx, argv[1]);
	if (ret < 0) {
		shell_error(sh, "Failed to initialize requests (%d)", ret);
		return ret;
	}

	ctx.sh = (struct shell *)sh;
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);
	ret = requests(&ctx, HTTP_DELETE);

	if (ret < 0) {
		shell_error(sh, "DELETE request failed (%d)", ret);
		return ret;
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_requests,
	SHELL_CMD_ARG(get, NULL, "Perform HTTP GET request: requests get <url>", cmd_requests_get,
		      2, 0),
	SHELL_CMD_ARG(post, NULL, "Perform HTTP POST request: requests post <url> <body>",
		      cmd_requests_post, 3, 0),
	SHELL_CMD_ARG(put, NULL, "Perform HTTP PUT request: requests put <url> <body>",
		      cmd_requests_put, 3, 0),
	SHELL_CMD_ARG(delete, NULL, "Perform HTTP DELETE request: requests delete <url>",
		      cmd_requests_delete, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(requests, &sub_requests, "HTTP requests commands", NULL);
