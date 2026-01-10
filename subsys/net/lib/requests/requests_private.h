/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __REQUESTS_PRIVATE_H_
#define __REQUESTS_PRIVATE_H_

#include <stdint.h>

int requests_url_parser(struct requests_ctx *ctx, const uint8_t *url);
int requests_dns_lookup(struct requests_ctx *ctx);
int requests_connect(struct requests_ctx *ctx);

#endif /* __REQUESTS_PRIVATE_H_ */
