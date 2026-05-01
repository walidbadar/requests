/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <requests/net/lib/requests.h>

#define PROMPT_KEY "\"content\":\""

static int alif_json_parser(const char *json, const char *key, char *out, size_t out_size)
{
	const char *start = strstr(json, key);
	if (!start) {
		return -EINVAL;
	}

	start += strlen(key);
	const char *end = strchr(start, '"');
	if (!end) {
		return -EINVAL;
	}

	size_t len = end - start;
	if (len + 1 > out_size) {
		return -EINVAL;
	}

	memset(out, 0, out_size);
	memcpy(out, start, len);
	out[len] = '\0';
	return 0;
}

static int alif_response(struct http_response *rsp, enum http_final_call final_data,
			 void *user_data)
{
	struct requests_ctx *ctx = (struct requests_ctx *)user_data;

	if (rsp->http_status_code == 0) {
		shell_warn(ctx->sh, "HTTP Status Code: %d", rsp->http_status_code );
	}

	return 0;
}

static int alif_ask(const struct shell *sh, size_t argc, char **argv)
{
	int ret;
	int ssl_verifypeer = 0;

	const char *headers[] = {"User-Agent: curl/7.81.0\r\n", "Accept: */*\r\n",
				 "Content-Type: application/json\r\n",
				 "Authorization: Bearer " CONFIG_ALIF_TOKEN "\r\n", NULL};

	char prompt[512];
	int prompt_lenght = snprintf(prompt, sizeof(prompt),
				  "{\"model\":\"%s\","
				  "\"stream\": false,"
				  "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
				  CONFIG_ALIF_MODEL, argv[1]);

	struct requests_ctx ctx;
	ret = requests_init(&ctx, CONFIG_ALIF_ENDPOINT);
	if (ret < 0) {
		shell_error(sh, "Failed to initialize requests (%d)", ret);
		return ret;
	}

	requests_setopt(&ctx, REQUESTS_HTTPHEADERS, headers);
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_SSL_VERIFYPEER, &ssl_verifypeer);
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, alif_response);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS_SIZE, &prompt_lenght);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS, prompt);

	ret = requests(&ctx, HTTP_POST);
	if (ret < 0) {
		shell_error(sh, "POST request failed (%d)", ret);
		return ret;
	}

	alif_json_parser(ctx.recv_buf, PROMPT_KEY, prompt, sizeof(prompt));
	shell_print(sh, "%s", prompt);

	return 0;
}

SHELL_CMD_ARG_REGISTER(alif, NULL, SHELL_HELP("Ask Alif", "<prompt>"), alif_ask, 2, 0);
