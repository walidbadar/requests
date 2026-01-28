/*
 * Copyright (c) 2025 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file requests.h
 * @brief HTTP requests library for embedded systems
 *
 * This library provides a simple interface for making HTTP/HTTPS requests
 * on embedded systems, similar to the libcurl API design.
 *
 * @note API design inspired by libcurl by Daniel Stenberg
 *       https://github.com/curl/curl
 */

#ifndef LIB_REQUESTS_H_
#define LIB_REQUESTS_H_

#include <stdbool.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/netinet/in.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

/**
 * @enum requests_options
 * @brief Configuration options for HTTP requests
 *
 * These options can be set using requests_setopt() to customize
 * the behavior of HTTP requests.
 */
enum requests_options {
	REQUESTS_HTTPHEADERS,     /**< Set custom HTTP headers (uint8_t*) */
	REQUESTS_POSTFIELDS,      /**< Set POST data payload (uint8_t*) */
	REQUESTS_POSTFIELDS_SIZE, /**< Set POST data payload size (uint16_t) */
	REQUESTS_PROTOCOL,        /**< Set HTTP protocol version (uint8_t*) */
	REQUESTS_SSL_VERIFYHOST,  /**< Enable/disable SSL host verification (bool*) */
	REQUESTS_SSL_VERIFYPEER,  /**< Enable/disable SSL peer verification (bool*) */
	REQUESTS_USERPWD,         /**< Set username:password for authentication (uint8_t*) */
	REQUESTS_WRITEFUNCTION,   /**< Set callback function for response data (http_response_cb_t*)
				  */
};

/**
 * @struct requests_url_fields
 * @brief Parsed components of a URL
 *
 * This structure holds the individual components of a URL after parsing,
 * including schema, hostname, URI path, port, and SSL flag.
 */
struct requests_url_fields {
	uint8_t schema[8];                    /**< URL schema (e.g., "http", "https") */
	uint8_t hostname[NI_MAXHOST];         /**< Hostname or IP address */
	uint8_t uri[CONFIG_REQUESTS_URL_LEN]; /**< URI path component */
	uint16_t port; /**< Port number (default: 80 for HTTP, 443 for HTTPS) */
	bool is_ssl;   /**< True if HTTPS, false if HTTP */
};

/**
 * @struct requests_ctx
 * @brief Context structure for HTTP request operations
 *
 * This structure maintains the state and configuration for an HTTP request.
 * It should be initialized with requests_init() before use.
 */
struct requests_ctx {
	int sockfd;                            /**< Socket file descriptor */
	struct sockaddr sa;                    /**< Socket address structure */
	struct requests_url_fields url_fields; /**< Parsed URL components */
	enum http_method method;               /**< HTTP method (GET, POST, etc.) */
	http_response_cb_t cb;                 /**< Callback function for response data */
	uint8_t recv_buf[NET_IPV4_MTU];        /**< Buffer for receiving data */
	uint8_t payload[NET_IPV4_MTU];         /**< Buffer for request payload */
	uint16_t payload_size;                 /**< Size for request payload */
	uint8_t protocol[16];                  /**< HTTP protocol version string */
	const char **headers;                  /**< Custom HTTP headers */
	bool is_ssl_verifyhost;                /**< Enable/disable SSL host name verification */
	bool is_ssl_verifypeer;                /**< Enable/disable SSL peer certificate verification */
	int status_code;                       /**< HTTP response status code */
	int err;                               /**< Error code from last operation */
#ifdef CONFIG_REQUESTS_SHELL
	struct shell *sh; /**< Shell instance for debug output */
#endif
};

/**
 * @brief Initialize a requests context
 *
 * Parses the provided URL and initializes the context structure for making
 * HTTP requests. This function must be called before using the context.
 *
 * @param ctx Pointer to the requests context to initialize
 * @param url URL string to parse (e.g., "http://example.com/api")
 *
 * @return 0 on success, negative error code on failure
 *
 * @note The URL must include the schema (http:// or https://)
 *
 * Example usage:
 * @code
 * struct requests_ctx ctx;
 * int ret = requests_init(&ctx, "https://api.example.com/data");
 * if (ret < 0) {
 *     // Handle error
 * }
 * @endcode
 */
int requests_init(struct requests_ctx *ctx, const uint8_t *url);

/**
 * @brief Set an option for the request context
 *
 * Configures various options for the HTTP request, such as headers,
 * POST data, SSL verification, and callback functions.
 *
 * @param ctx Pointer to the requests context
 * @param option The option to set (from enum requests_options)
 * @param parameter Pointer to the parameter value
 *
 * @note The type of parameter depends on the option being set.
 *       Refer to the requests_options enum documentation for details.
 *
 * Example usage:
 * @code
 * struct requests_ctx ctx;
 * requests_init(&ctx, "https://api.example.com/data");
 *
 * // Set custom headers
 * uint8_t headers[] = "Content-Type: application/json";
 * requests_setopt(&ctx, REQUESTS_HTTPHEADER, headers);
 *
 * // Set POST data
 * uint8_t data[] = "{\"key\":\"value\"}";
 * requests_setopt(&ctx, REQUESTS_POSTFIELDS, data);
 *
 * @endcode
 */
void requests_setopt(struct requests_ctx *ctx, enum requests_options option, void *parameter);

/**
 * @brief Execute an HTTP request
 *
 * Performs the actual HTTP request using the configured context.
 * The request is sent to the URL specified during initialization,
 * using the specified HTTP method.
 *
 * @param ctx Pointer to the initialized and configured requests context
 * @param method HTTP method to use (e.g., HTTP_GET, HTTP_POST)
 *
 * @return 0 on success, negative error code on failure
 *
 * @note The context must be initialized with requests_init() and
 *       optionally configured with requests_setopt() before calling this function.
 *
 * @note The response status code will be stored in ctx->status_code
 *
 * Example usage:
 * @code
 * struct requests_ctx ctx;
 * requests_init(&ctx, "https://api.example.com/data");
 *
 * // Execute GET request
 * int ret = requests(&ctx, HTTP_GET);
 * if (ret == 0) {
 *     printf("Status: %d\n", ctx.status_code);
 * }
 * @endcode
 */
int requests(struct requests_ctx *ctx, enum http_method method);

#endif /* LIB_REQUESTS_H_ */
