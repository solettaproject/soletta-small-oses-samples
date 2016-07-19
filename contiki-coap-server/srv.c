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
#include <sol-buffer.h>
#include <sol-coap.h>
#include <sol-network.h>
#include <sol-vector.h>

#define DEFAULT_UDP_PORT 5683

#define OC_CORE_JSON_SEPARATOR ","
#define OC_CORE_ELEM_JSON_START "{\"oc\":[{\"href\":\"%s\",\"rep\":{"
#define OC_CORE_PROP_JSON_NUMBER "\"%s\":%d"
#define OC_CORE_PROP_JSON_STRING "\"%s\":\"%s\""
#define OC_CORE_PROP_JSON_BOOLEAN "\"%s\":%s"
#define OC_CORE_ELEM_JSON_END "}}]}"

struct light_context {
    struct sol_coap_server *server;
    struct sol_coap_resource *resource;
    bool state;
};

static int
light_resource_to_rep(const struct sol_coap_resource *resource,
    bool state, struct sol_buffer *buf)
{
    SOL_BUFFER_DECLARE_STATIC(path, 64);
    int ret;

    memset(&path, 0, sizeof(path));
    sol_coap_path_to_buffer(resource->path, &path, 0, NULL);

    ret = sol_buffer_append_printf(buf, OC_CORE_ELEM_JSON_START, (char *)sol_buffer_steal(&path, NULL));
    ret = sol_buffer_append_printf(buf, OC_CORE_PROP_JSON_NUMBER, "power", 100);
    ret = sol_buffer_append_printf(buf, OC_CORE_JSON_SEPARATOR);
    ret = sol_buffer_append_printf(buf, OC_CORE_PROP_JSON_STRING, "name", "Soletta LAMP!");
    ret = sol_buffer_append_printf(buf, OC_CORE_JSON_SEPARATOR);
    ret = sol_buffer_append_printf(buf, OC_CORE_PROP_JSON_BOOLEAN, "state", state ? "true" : "false");
    ret = sol_buffer_append_printf(buf, OC_CORE_ELEM_JSON_END);

    return ret;
}

static void
set_light_state(struct light_context *ctx)
{
    struct sol_coap_packet *pkt;
    struct sol_buffer *payload;

    printf(" *********************** %s\n", ctx->state ? "ON" : "OFF");

    pkt = sol_coap_packet_new_notification(ctx->server, ctx->resource);
    if (!pkt) {
        SOL_WRN("Oops! No memory?");
        return;
    }

    sol_coap_header_set_code(pkt, SOL_COAP_RESPONSE_CODE_CONTENT);

    sol_coap_packet_get_payload(pkt, &payload, NULL);
    light_resource_to_rep(ctx->resource, ctx->state, payload);

    sol_coap_notify(ctx->server, ctx->resource, pkt);
}

static int
light_method_put(void *data, struct sol_coap_server *server, const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr)
{
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    char *sub = NULL;
    struct sol_buffer *p;
    size_t len;
    enum sol_coap_response_code code = SOL_COAP_RESPONSE_CODE_CONTENT;

    sol_coap_packet_get_payload(req, &p, &len);

    if (p)
        sub = strstr((char *)sol_buffer_at(p, len), "state\":");
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
light_method_get(void *data, struct sol_coap_server *s, const struct sol_coap_resource *resource, struct sol_coap_packet *req,
    const struct sol_network_link_addr *cliaddr)
{
    struct light_context *lc = data;
    struct sol_coap_packet *resp;
    struct sol_buffer *payload;
    SOL_BUFFER_DECLARE_STATIC(buf, 100);
    const char *ret;

    ret = sol_network_link_addr_to_str(cliaddr, &buf);
    printf("Got GET from [%s]:%d\n", ret, cliaddr->port);

    resp = sol_coap_packet_new(req);
    if (!resp) {
        SOL_WRN("resp failed");
        return -1;
    }
    sol_coap_header_set_type(resp, SOL_COAP_MESSAGE_TYPE_ACK);
    sol_coap_header_set_code(resp, SOL_COAP_RESPONSE_CODE_CONTENT);

    sol_coap_packet_get_payload(resp, &payload, NULL);
    light_resource_to_rep(resource, lc->state, payload);

    return sol_coap_send_packet(lc->server, resp, cliaddr);
}
static bool
setup_server(void)
{
    struct light_context *lc;
    struct sol_network_link_addr saddr = {
        .family = SOL_NETWORK_FAMILY_INET6,
        .addr.in6 = { 0 },
        .port = DEFAULT_UDP_PORT
    };
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
    int ret;

    lc = calloc(1, sizeof(*lc));
    if (!lc) {
        SOL_WRN("lc failed");
        return false;
    }

    lc->server = sol_coap_server_new(&saddr, false);
    if (!lc->server) {
        SOL_WRN("lc->server failed");
        free(lc);
        return false;
    }

    if ((ret = sol_coap_server_register_resource(lc->server, &light, lc)) < 0) {
        SOL_WRN("register resource failed: %s", strerror(-ret));
        sol_coap_server_unref(lc->server);
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

        SOL_VECTOR_FOREACH_IDX(links, l, i) {
            uint16_t j;
            const struct sol_network_link_addr *addr;

            printf("Link #%d\n", i);
            SOL_VECTOR_FOREACH_IDX(&l->addrs, addr, j) {
                SOL_BUFFER_DECLARE_STATIC(buf, 100);
                const char *ret;
                ret = sol_network_link_addr_to_str(addr, &buf);
                if (ret)
                    printf("\tAddr #%d: %s\n", j, ret);
            }
        }
    }
}

static void
startup(void)
{
    SOL_INF("Init");

    show_interfaces();

    setup_server();
}

static void
shutdown(void)
{
    SOL_INF("Shutdown");
}
SOL_MAIN_DEFAULT(startup, shutdown);
