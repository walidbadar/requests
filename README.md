![REQUESTS](doc/img/requests.png)

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![Zephyr RTOS](https://img.shields.io/badge/RTOS-Zephyr-blue)](https://zephyrproject.org/)

## Overview

The project provides an easy-to-use interface for performing common HTTP(S) operations
such as GET, POST, PUT and DELETE using Zephyr's networking stack.
It also includes built-in shell commands to interact with HTTP(S) endpoints directly
from the Zephyr shell.

```shell
uart:~$ requests
requests - HTTP requests commands
Subcommands:
  get     : Perform HTTP GET request
            Usage: get <url>
  post    : Perform HTTP POST request
            Usage: post <url> <body>
  put     : Perform HTTP PUT request
            Usage: put <url> <body>
  delete  : Perform HTTP DELETE request
            Usage: delete <url>
uart:~$

```

### HTTP_GET

```c
#include <zephyr/kernel.h>
#include <requests/net/lib/requests.h>

static int http_response_handler(struct http_response *rsp, enum http_final_call final_data,
                    void *user_data)
{
	struct requests_ctx *ctx = (struct requests_ctx *)user_data;
	ctx->status_code = rsp->http_status_code;

	if (rsp->body_frag_len) {
		printk("%.*s", rsp->body_frag_len, rsp->body_frag_start);
	}

	return 0;
}

int main(void)
{
	int ret;
	struct requests_ctx ctx;

	printk("HTTP GET example started\n");

	ret = requests_init(&ctx, "http://example.com");
	if (ret < 0) {
		printk("requests_init failed (%d)\n", ret);
		return 0;
	}

	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);

	ret = requests(&ctx, HTTP_GET);
	if (ret < 0) {
		printk("HTTP GET failed (%d)\n", ret);
		return 0;
	}

	printk("\nHTTP GET finished, status=%d\n", ctx.status_code);
	return 0;
}
```

### HTTP_POST

```c
#include <zephyr/kernel.h>
#include <requests/net/lib/requests.h>

static int http_response_handler(struct http_response *rsp, enum http_final_call final_data,
                    void *user_data)
{
	struct requests_ctx *ctx = (struct requests_ctx *)user_data;
	ctx->status_code = rsp->http_status_code;

	if (rsp->body_frag_len) {
		printk("%.*s", rsp->body_frag_len, rsp->body_frag_start);
	}

	return 0;
}

int main(void)
{
	int ret;
	struct requests_ctx ctx;

	const char post_body[] = "{\"msg\":\"hello from zephyr\"}";
	uint16_t post_size = sizeof(post_body);

	const char *headers[] = {
		"Content-Type: application/json\r\n",
		NULL
	};

	printk("HTTP POST example started\n");

	ret = requests_init(&ctx, "http://example.com/post");
	if (ret < 0) {
		printk("requests_init failed (%d)\n", ret);
		return 0;
	}

	requests_setopt(&ctx, REQUESTS_HTTPHEADERS, headers);
	requests_setopt(&ctx, REQUESTS_PROTOCOL, "HTTP/1.1");
	requests_setopt(&ctx, REQUESTS_WRITEFUNCTION, http_response_handler);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS, post_body);
	requests_setopt(&ctx, REQUESTS_POSTFIELDS_SIZE, &post_size);

	ret = requests(&ctx, HTTP_POST);
	if (ret < 0) {
		printk("HTTP POST failed (%d)\n", ret);
		return 0;
	}

	printk("\nHTTP POST finished, status=%d\n", ctx.status_code);
	return 0;
}
```

### Prerequisites

#### On the Linux Host, find the Zephyr net-tools project, which can either be found in a Zephyr standard installation under the tools/net-tools directory or installed stand alone from its own git repository:

```shell
git clone https://github.com/zephyrproject-rtos/net-tools
cd net-tools
make
```

#### To run with native_sim board first setup network interface with NAT

```shell
./net-setup.sh start --config nat.conf
```

## Getting Started

Before getting started, make sure you have a proper Zephyr development
environment. Follow the official
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

### Initialization

The first step is to initialize the workspace folder (``workspace``) where
the ``requests`` and all Zephyr modules will be cloned. Run the following
command:

```shell
# initialize workspace for the requests (main branch)
west init -m https://github.com/walidbadar/requests --mr main workspace
# update Zephyr modules
cd workspace
west update
```

### Building and running

To build the application, run the following command:

```shell
cd requests
west build -b $BOARD app
```

where `$BOARD` is the target board (i.e native_sim).
Some other build configurations are also provided:

- `overlay-tls.conf`: Enable HTTPS
- `overlay-wifi.conf`: Enable [Wi-Fi shell](https://docs.zephyrproject.org/latest/samples/net/wifi/shell/README.html)

They can be enabled by setting `OVERLAY_CONFIG`, e.g.

```shell
west build -b $BOARD app -- -DOVERLAY_CONFIG=overlay-wifi.conf
```

> **Note (TLS certificates)**  
> When using `overlay-tls.conf`, a TLS certificate **must be provided** in DER format.  
> Place the certificate file (any name, ending with `.der`) in the `certs` directory.  
> During the build, the certificate is automatically converted into `certs.der.inc` and made available to the application via `CONFIG_REQUESTS_SSL_CERTS`.

Once you have built the application, run the following command:

```shell
west flash
```

### Sample Output
```shell
*** Booting Zephyr OS build v4.3.0 ***
[00:00:00.000,000] <inf> net_config: Initializing network
[00:00:00.000,000] <inf> net_config: IPv4 address: 192.0.2.1
uart:~$ requests get http://google.com
[00:03:20.200,000] <dbg> requests_parser: Hostname: google.com, Port: 80, URI: /
[00:03:20.200,000] <dbg> requests_connect: Network connection established (IPv4)
[00:03:20.200,000] <dbg> requests_connect: Waiting for IPv4 address assignment (DHCP/Static)
[00:03:20.280,000] <dbg> requests_connect: Host IPv4 address: 142.250.200.174
[00:03:20.280,000] <dbg> requests_connect: DNS resolved
<HTML><HEAD><meta http-equiv="content-type" content="text/html;charset=utf-8">
<TITLE>301 Moved</TITLE></HEAD><BODY>
<H1>301 Moved</H1>
The document has moved
<A HREF="http://www.google.com/">here</A>.
</BODY></HTML>
```

## requests as a Zephyr Module

This repository is a [Zephyr module](https://docs.zephyrproject.org/latest/develop/modules.html) 
which allows for reuse of its components.

To pull in requests as a Zephyr module, either add it as a West project in the `west.yaml` file
or pull it in by adding a submanifest file with the following content and run `west update`:

```yaml
manifest:
  projects:
    - name: requests
      url: https://github.com/walidbadar/requests.git
      revision: main
      path: modules/lib/requests # adjust the path as needed
```
