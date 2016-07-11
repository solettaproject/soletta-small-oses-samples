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
    bool state, struct sol_buffer *buf)
{
    SOL_BUFFER_DECLARE_STATIC(buffer, 64);
    int r;

    r = sol_coap_path_to_buffer(resource->path, &buffer, 0, NULL);
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf,
        OC_CORE_ELEM_JSON_START, (char *)sol_buffer_steal(&buffer, NULL));
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf,
        OC_CORE_PROP_JSON_NUMBER, "power", 100);
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf, OC_CORE_JSON_SEPARATOR);
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf,
        OC_CORE_PROP_JSON_STRING, "name", "Soletta LAMP!");
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf, OC_CORE_JSON_SEPARATOR);
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf,
        OC_CORE_PROP_JSON_BOOLEAN, "state", state ? "true" : "false" );
    SOL_INT_CHECK(r, < 0, r);

    r = sol_buffer_append_printf(buf, OC_CORE_ELEM_JSON_END);
    SOL_INT_CHECK(r, < 0, r);

    return 0;
}

static void
set_light_state(struct light_context *ctx)
{
    struct sol_coap_packet *pkt;
    struct sol_buffer *buf;
    size_t offset;
    int r;

    sol_gpio_write(ctx->led, ctx->state);

    pkt = sol_coap_packet_new_notification(ctx->server, ctx->resource);
    if (!pkt) {
        SOL_WRN("Oops! No memory?");
        return;
    }

    sol_coap_header_set_code(pkt, SOL_COAP_RESPONSE_CODE_CONTENT);

    r = sol_coap_packet_get_payload(pkt, &buf, &offset);
    SOL_INT_CHECK_GOTO(r, < 0, err);
    r = light_resource_to_rep(ctx->resource, ctx->state, buf);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    sol_coap_notify(ctx->server, ctx->resource, pkt);

    return;

err:
    sol_coap_packet_unref(pkt);
}

static int
light_method_put(void *data, struct sol_coap_server *server,
    const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr)
{
    enum sol_coap_response_code code = SOL_COAP_RESPONSE_CODE_CONTENT;
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    struct sol_buffer *buf;
    char *sub = NULL;
    size_t offset;
    int r;

    r = sol_coap_packet_get_payload(req, &buf, &offset);
    SOL_INT_CHECK(r, < 0, r);

    if (buf->used > offset)
        sub = strstr((char *)sol_buffer_at(buf, offset), "state\":");
    if (!sub) {
        code = SOL_COAP_RESPONSE_CODE_BAD_REQUEST;
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
    sol_coap_header_set_type(resp, SOL_COAP_MESSAGE_TYPE_ACK);
    sol_coap_header_set_code(resp, code);

    return sol_coap_send_packet(lc->server, resp, cliaddr);
}

static int
light_method_get(void *data, struct sol_coap_server *s,
    const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr)
{
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    struct sol_buffer *buf;
    int r;

    resp = sol_coap_packet_new(req);
    if (!resp) {
        SOL_WRN("resp failed");
        return -1;
    }
    sol_coap_header_set_type(resp, SOL_COAP_MESSAGE_TYPE_ACK);
    sol_coap_header_set_code(resp, SOL_COAP_RESPONSE_CODE_CONTENT);

    r = sol_coap_packet_get_payload(resp, &buf, NULL);
    SOL_INT_CHECK_GOTO(r, < 0, err);
    r = light_resource_to_rep(resource, lc->state, buf);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    return sol_coap_send_packet(lc->server, resp, cliaddr);

err:
    sol_coap_packet_unref(resp);
    return r;
}

static struct sol_gpio *
setup_led(void)
{
    struct sol_gpio_config conf = {
        SOL_SET_API_VERSION(.api_version = SOL_GPIO_CONFIG_API_VERSION, )
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
        SOL_SET_API_VERSION(.api_version = SOL_GPIO_CONFIG_API_VERSION, )
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
    int r;
    struct light_context *lc;
    static struct sol_coap_resource light = {
        SOL_SET_API_VERSION(.api_version = SOL_COAP_RESOURCE_API_VERSION, )
        .get = light_method_get,
        .put = light_method_put,
        .flags = SOL_COAP_FLAGS_WELL_KNOWN,
        .path = {
            SOL_STR_SLICE_LITERAL("a"),
            SOL_STR_SLICE_LITERAL("light"),
            SOL_STR_SLICE_EMPTY
        }
    };
    struct sol_network_link_addr servaddr =
    { .family = SOL_NETWORK_FAMILY_INET6,
      .port = DEFAULT_UDP_PORT };

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

    lc->server = sol_coap_server_new(&servaddr, false);
    if (!lc->server) {
        SOL_WRN("lc->server failed");
        sol_gpio_close(lc->led);
        free(lc);
        return false;
    }

    r = sol_coap_server_register_resource(lc->server, &light, lc);
    if (r < 0) {
        SOL_WRN("register resource failed: %d", r);
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
        SOL_VECTOR_FOREACH_IDX (links, l, i) {
            uint16_t j;
            const struct sol_network_link_addr *addr;

            printf("Link #%d\n", i);
            SOL_VECTOR_FOREACH_IDX (&l->addrs, addr, j) {
                SOL_BUFFER_DECLARE_STATIC(buf, SOL_NETWORK_INET_ADDR_STR_LEN);
                const char *ret;
                ret = sol_network_link_addr_to_str(addr, &buf);
                if (ret)
                    printf("\tAddr #%d: %.*s\n", j,
                        SOL_STR_SLICE_PRINT(sol_buffer_get_slice(&buf)));
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
