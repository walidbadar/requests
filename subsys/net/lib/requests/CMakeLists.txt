# Copyright (c) 2025 Muhammad Waleed Badar
# SPDX-License-Identifier: Apache-2.0

zephyr_library_named(requests)

zephyr_library_sources(requests_connect.c)
zephyr_library_sources(requests_parser.c)
zephyr_library_sources(requests.c)
zephyr_library_sources_ifdef(CONFIG_TLS_CREDENTIALS requests_certificate.c)
zephyr_library_sources_ifdef(CONFIG_REQUESTS_SHELL requests_shell.c)

if(CONFIG_REQUESTS_HAS_CERTIFICATE)
  set(CERTIFICATE_PATH
    ${APPLICATION_SOURCE_DIR}/certs/${CONFIG_REQUESTS_CERTIFICATE}
  )

  if(NOT EXISTS ${CERTIFICATE_PATH})
    message(FATAL_ERROR
      "Requests SSL certificate not found: ${CERTIFICATE_PATH}"
    )
  endif()

  generate_inc_file_for_target(
    app
    ${CERTIFICATE_PATH}
    ${ZEPHYR_BINARY_DIR}/include/generated/certs.der.inc
  )
endif()
