# Copyright (c) 2025 Muhammad Waleed Badar
# SPDX-License-Identifier: Apache-2.0

zephyr_library_named(requests)

zephyr_library_sources(requests_connect.c)
zephyr_library_sources(requests_parser.c)
zephyr_library_sources(requests.c)
zephyr_library_sources_ifdef(CONFIG_REQUESTS_SHELL requests_shell.c)

if(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
  file(GLOB DER_CERTS "${CMAKE_SOURCE_DIR}/certs/*.der")

  if(NOT DER_CERTS)
    message(FATAL_ERROR "No .der certificates found in ${CMAKE_SOURCE_DIR}/certs")
  endif()

  # Use the first .der file found
  list(GET DER_CERTS 0 CERT_PATH)
  message(STATUS "Using TLS certificate: ${CERT_PATH}")

  generate_inc_file_for_target(
    app
    ${CERT_PATH}
    ${ZEPHYR_BINARY_DIR}/include/generated/${CONFIG_REQUESTS_SSL_CERTS}
  )

endif()
