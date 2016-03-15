/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <sol-coap.h>
#include <sol-gpio.h>
#include <sol-log.h>
#include <sol-mainloop.h>
#include <sol-network.h>

#define DEFAULT_UDP_PORT 5683

#if SOL_PLATFORM_RIOT
#define GPIO_BTN 0x4100441c
#define GPIO_LED 0x41004413
#elif SOL_PLATFORM_ZEPHYR
#define GPIO_BTN 15
#define GPIO_LED 25
#endif

struct remote_light_context {
    struct sol_coap_server *server;
    struct sol_gpio *button;
    struct sol_gpio *led;
    struct sol_timeout *timeout;
    struct sol_network_link_addr addr;
    bool state;
    bool found;
};

static void
remote_light_toggle(struct remote_light_context *ctx)
{
    struct sol_coap_packet *req;
    uint8_t *payload;
    uint16_t len;

    req = sol_coap_packet_request_new(SOL_COAP_METHOD_PUT, SOL_COAP_TYPE_NONCON);
    if (!req) {
        SOL_WRN("Oops! No memory?");
        return;
    }
    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "a", sizeof("a") - 1);
    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "light", sizeof("light") - 1);

    sol_coap_packet_get_payload(req, &payload, &len);
    len = snprintf((char *)payload, len, "{\"oc\":[{\"rep\":{\"state\":\%s}}]}",
        ctx->state ? "true" : "false");
    sol_coap_packet_set_payload_used(req, len);
    sol_coap_send_packet(ctx->server, req, &ctx->addr);
}

static bool
timeout_cb(void *data)
{
    struct remote_light_context *ctx = data;

    ctx->state = !ctx->state;

    remote_light_toggle(ctx);

    ctx->timeout = NULL;

    return false;
}

static void
button_pressed(void *data, struct sol_gpio *gpio, bool value)
{
    struct remote_light_context *ctx = data;
    if (!ctx->found || ctx->timeout) return;
    ctx->timeout = sol_timeout_add(300, timeout_cb, ctx);
}

static struct sol_gpio *
setup_button(const struct remote_light_context *ctx)
{
    struct sol_gpio_config conf = {
        SOL_SET_API_VERSION(.api_version = SOL_GPIO_CONFIG_API_VERSION,)
        .dir = SOL_GPIO_DIR_IN,
        .in = {
            .trigger_mode = SOL_GPIO_EDGE_FALLING,
            .cb = button_pressed,
            .user_data = ctx
        }
    };

    return sol_gpio_open(GPIO_BTN, &conf);
}

static struct sol_gpio *
setup_led(void)
{
    struct sol_gpio_config conf = {
        SOL_SET_API_VERSION(.api_version = SOL_GPIO_CONFIG_API_VERSION,)
        .dir = SOL_GPIO_DIR_OUT,
        .active_low = true
    };

    return sol_gpio_open(GPIO_LED, &conf);
}

static bool
get_state(struct sol_coap_packet *pkt)
{
    char *sub = NULL;
    uint8_t *payload;
    uint16_t len;

    if (!sol_coap_packet_has_payload(pkt)) {
        printf("No payload\n");
        return false;
    }

    sol_coap_packet_get_payload(pkt, &payload, &len);
    if (!payload)
        return false;
    printf("Payload: %.*s\n", len, payload);
    sub = strstr((char *)payload, "state\":");
    if (!sub)
        return false;
    return !memcmp(sub + strlen("state\":"), "true", sizeof("true") - 1);
}

static bool
notification_reply_cb(struct sol_coap_server *s, struct sol_coap_packet *req, const struct sol_network_link_addr *cliaddr, void *data)
{
    struct remote_light_context *ctx = data;

    ctx->state = get_state(req);
    sol_gpio_write(ctx->led, ctx->state);

    return true;
}

static void
observe(struct remote_light_context *ctx)
{
    struct sol_coap_packet *req;
    uint8_t observe = 0;
    uint8_t token[] = { 0x36, 0x36, 0x36, 0x21 };

    req = sol_coap_packet_request_new(SOL_COAP_METHOD_GET, SOL_COAP_TYPE_CON);
    if (!req) {
        SOL_WRN("Looks like we have no space");
        return;
    }

    sol_coap_header_set_token(req, token, sizeof(token));

    sol_coap_add_option(req, SOL_COAP_OPTION_OBSERVE, &observe, sizeof(observe));

    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "a", sizeof("a") - 1);
    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "light", sizeof("light") - 1);

    sol_coap_send_packet_with_reply(ctx->server, req, &ctx->addr, notification_reply_cb, ctx);
}

static bool
discover_reply_cb(struct sol_coap_server *s, struct sol_coap_packet *req, const struct sol_network_link_addr *cliaddr, void *data)
{
    struct remote_light_context *ctx = data;
    char buf[SOL_INET_ADDR_STRLEN];

    sol_network_addr_to_str(cliaddr, buf, sizeof(buf));
    printf("Found resource in: %s\n", buf);

    if (ctx->found) {
        sol_network_addr_to_str(&ctx->addr, buf, sizeof(buf));
        printf("Ignoring, as we already had resource from: %s\n", buf);
        return 0;
    }

    ctx->found = true;

    memcpy(&ctx->addr, cliaddr, sizeof(ctx->addr));

    ctx->state = get_state(req);

    observe(ctx);

    return false;
}

static bool
discover_resource(struct remote_light_context *ctx)
{
    struct sol_coap_packet *req;
    struct sol_network_link_addr cliaddr = { };

    req = sol_coap_packet_request_new(SOL_COAP_METHOD_GET, SOL_COAP_TYPE_CON);
    if (!req) {
        SOL_WRN("Looks like we have no space");
        return false;
    }

    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "a", sizeof("a") - 1);
    sol_coap_add_option(req, SOL_COAP_OPTION_URI_PATH, "light", sizeof("light") - 1);

    cliaddr.family = SOL_NETWORK_FAMILY_INET6;
    sol_network_addr_from_str(&cliaddr, "ff02::fd");
    //sol_network_addr_from_str(&cliaddr, "ff80::863a:4bff:fe8c:f665");
    cliaddr.port = DEFAULT_UDP_PORT;

    sol_coap_send_packet_with_reply(ctx->server, req, &cliaddr, discover_reply_cb, ctx);

    return true;
}

static bool
setup_server(void)
{
    struct remote_light_context *ctx;

    ctx = calloc(1, sizeof(*ctx));
    SOL_NULL_CHECK(ctx, false);

    ctx->server = sol_coap_server_new(0);
    SOL_NULL_CHECK_GOTO(ctx->server, server_failed);

    ctx->button = setup_button(ctx);
    SOL_NULL_CHECK_GOTO(ctx->button, button_failed);

    ctx->led = setup_led();
    SOL_NULL_CHECK_GOTO(ctx->led, led_failed);

    discover_resource(ctx);

    return true;
led_failed:
    sol_gpio_close(ctx->button);
button_failed:
    sol_coap_server_unref(ctx->server);
server_failed:
    free(ctx);
    return false;
}

static void
show_interfaces(void)
{
    const struct sol_vector *links;

    links = sol_network_get_available_links();
    if (links) {
        const struct sol_network_link *l;
        uint16_t i;

        SOL_VECTOR_FOREACH_IDX(links, l, i) {
            uint16_t j;
            const struct sol_network_link_addr *addr;

            printf("Link #%d\n", i);
            SOL_VECTOR_FOREACH_IDX(&l->addrs, addr, j) {
                char buf[100];
                const char *ret;
                ret = sol_network_addr_to_str(addr, buf, sizeof(buf));
                if (ret)
                    printf("\tAddr #%d: %s\n", j, ret);
            }
        }
    }
}

static void
startup(void)
{
    show_interfaces();

    setup_server();
}
SOL_MAIN_DEFAULT(startup, NULL);
