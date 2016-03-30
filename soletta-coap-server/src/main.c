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

#include <soletta.h>
#include <sol-coap.h>
#include <sol-gpio.h>
#include <sol-log.h>
#include <sol-mainloop.h>
#include <sol-network.h>

#define DEFAULT_UDP_PORT 5683

#ifdef SOL_PLATFORM_RIOT
#define GPIO_BTN 0x4100441c
#define GPIO_LED 0x41004413
#elif SOL_PLATFORM_ZEPHYR
#define GPIO_BTN 15
#define GPIO_LED 25
#endif

#define OC_CORE_JSON_SEPARATOR ","
#define OC_CORE_ELEM_JSON_START "{\"oc\":[{\"href\":\"%s\",\"rep\":{"
#define OC_CORE_PROP_JSON_NUMBER "\"%s\":%d"
#define OC_CORE_PROP_JSON_STRING "\"%s\":\"%s\""
#define OC_CORE_PROP_JSON_BOOLEAN "\"%s\":%s"
#define OC_CORE_ELEM_JSON_END "}}]}"

struct light_context {
    struct sol_coap_server *server;
    struct sol_coap_resource *resource;
    struct sol_timeout *timeout;
    struct sol_gpio *led;
    struct sol_gpio *btn;
    bool state;
};

static int
light_resource_to_rep(const struct sol_coap_resource *resource,
    bool state, char *buf, int buflen)
{
    uint8_t path[64];
    size_t pathlen;
    int len = 0;

    memset(&path, 0, sizeof(path));
    sol_coap_uri_path_to_buf(resource->path, path, sizeof(path), &pathlen);

    len += snprintf(buf + len, buflen - len, OC_CORE_ELEM_JSON_START, path);
    len += snprintf(buf + len, buflen - len, OC_CORE_PROP_JSON_NUMBER, "power", 100);
    len += snprintf(buf + len, buflen - len, OC_CORE_JSON_SEPARATOR);
    len += snprintf(buf + len, buflen - len, OC_CORE_PROP_JSON_STRING, "name", "Soletta LAMP!");
    len += snprintf(buf + len, buflen - len, OC_CORE_JSON_SEPARATOR);
    len += snprintf(buf + len, buflen - len, OC_CORE_PROP_JSON_BOOLEAN, "state",
        state ? "true" : "false");
    len += snprintf(buf + len, buflen - len, OC_CORE_ELEM_JSON_END);

    return len;
}

static void
set_light_state(struct light_context *ctx)
{
    struct sol_coap_packet *pkt;
    uint8_t *payload;
    uint16_t len;

    sol_gpio_write(ctx->led, ctx->state);

    pkt = sol_coap_packet_notification_new(ctx->server, ctx->resource);
    if (!pkt) {
        SOL_WRN("Oops! No memory?");
        return;
    }

    sol_coap_header_set_code(pkt, SOL_COAP_RSPCODE_CONTENT);

    sol_coap_packet_get_payload(pkt, &payload, &len);
    len = light_resource_to_rep(ctx->resource, ctx->state, (char *)payload, len);
    sol_coap_packet_set_payload_used(pkt, len);

    sol_coap_packet_send_notification(ctx->server, ctx->resource, pkt);
}

static int
light_method_put(struct sol_coap_server *server, const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr, void *data)
{
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    char *sub = NULL;
    uint8_t *p;
    uint16_t len;
    sol_coap_responsecode_t code = SOL_COAP_RSPCODE_CONTENT;

    sol_coap_packet_get_payload(req, &p, &len);

    if (p)
        sub = strstr((char *)p, "state\":");
    if (!sub) {
        code = SOL_COAP_RSPCODE_BAD_REQUEST;
        goto done;
    }

    lc->state = !memcmp(sub + strlen("state\":"), "true", sizeof("true") - 1);

    set_light_state(lc);

done:
    resp = sol_coap_packet_new(req);
    if (!resp) {
        SOL_WRN("resp failed");
        return -1;
    }
    sol_coap_header_set_type(resp, SOL_COAP_TYPE_ACK);
    sol_coap_header_set_code(resp, code);

    return sol_coap_send_packet(lc->server, resp, cliaddr);
}

static int
light_method_get(struct sol_coap_server *s, const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr, void *data)
{
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    uint8_t *payload;
    uint16_t len;

    resp = sol_coap_packet_new(req);
    if (!resp) {
        SOL_WRN("resp failed");
        return -1;
    }
    sol_coap_header_set_type(resp, SOL_COAP_TYPE_ACK);
    sol_coap_header_set_code(resp, SOL_COAP_RSPCODE_CONTENT);

    sol_coap_packet_get_payload(resp, &payload, &len);
    len = light_resource_to_rep(resource, lc->state, (char *)payload, len);
    sol_coap_packet_set_payload_used(resp, len);

    return sol_coap_send_packet(lc->server, resp, cliaddr);
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
timeout_cb(void *data)
{
    struct light_context *ctx = data;

    ctx->state = !ctx->state;

    set_light_state(ctx);

    ctx->timeout = NULL;

    return false;
}

static void
button_pressed(void *data, struct sol_gpio *gpio, bool value)
{
    struct light_context *ctx = data;
    if (ctx->timeout) return;
    ctx->timeout = sol_timeout_add(300, timeout_cb, ctx);
}

static struct sol_gpio *
setup_button(const struct light_context *ctx)
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
static bool
setup_server(void)
{
    struct light_context *lc;
    static struct sol_coap_resource light = {
        SOL_SET_API_VERSION(.api_version = SOL_COAP_RESOURCE_API_VERSION,)
        .get = light_method_get,
        .put = light_method_put,
        .flags = SOL_COAP_FLAGS_WELL_KNOWN,
        .path = {
            SOL_STR_SLICE_LITERAL("a"),
            SOL_STR_SLICE_LITERAL("light"),
            SOL_STR_SLICE_EMPTY
        }
    };

    lc = calloc(1, sizeof(*lc));
    if (!lc) {
        SOL_WRN("lc failed");
        return false;
    }

    lc->led = setup_led();
    if (!lc->led) {
        SOL_WRN("lc->led failed");
        free(lc);
        return false;
    }

    lc->btn = setup_button(lc);

    lc->server = sol_coap_server_new(DEFAULT_UDP_PORT);
    if (!lc->server) {
        SOL_WRN("lc->server failed");
        sol_gpio_close(lc->led);
        free(lc);
        return false;
    }

    if (!sol_coap_server_register_resource(lc->server, &light, lc)) {
        SOL_WRN("register resource failed");
        sol_coap_server_unref(lc->server);
        sol_gpio_close(lc->led);
        free(lc);
        return false;
    }

    lc->resource = &light;

    return true;
}

static void
show_interfaces(void)
{
    const struct sol_vector *links;

    links = sol_network_get_available_links();
    if (links) {
        const struct sol_network_link *l;
        uint16_t i;

        printf("Found %d links\n", links->len);
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
    } else {
        printf("No interfaces?\n");
    }
}

static void
startup(void)
{
    show_interfaces();

    setup_server();
}
SOL_MAIN_DEFAULT(startup, NULL);
