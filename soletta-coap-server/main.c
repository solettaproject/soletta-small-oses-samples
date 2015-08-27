#include <stdio.h>

#include <sol-coap.h>
#include <sol-gpio.h>
#include <sol-log.h>
#include <sol-mainloop.h>
#include <sol-network.h>
#include <sol-str-slice.h>

#include <periph_cpu.h>

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
    int len = 0;

    memset(&path, 0, sizeof(path));
    sol_coap_uri_path_to_buf(resource->path, path, sizeof(path));

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

    sol_coap_header_set_code(pkt, SOL_COAP_RSPCODE_CONTENT);

    sol_coap_packet_get_payload(pkt, &payload, &len);
    len = light_resource_to_rep(ctx->resource, ctx->state, (char *)payload, len);
    sol_coap_packet_set_payload_used(pkt, len);

    sol_coap_packet_send_notification(ctx->server, ctx->resource, pkt);
}

static int
light_method_put(const struct sol_coap_resource *resource, struct sol_coap_packet *req,
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
light_method_get(const struct sol_coap_resource *resource, struct sol_coap_packet *req,
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
        .api_version = SOL_GPIO_CONFIG_API_VERSION,
        .dir = SOL_GPIO_DIR_OUT,
        .active_low = true
    };

    return sol_gpio_open(GPIO(PA, 19), &conf);
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
button_pressed(void *data, struct sol_gpio *gpio)
{
    struct light_context *ctx = data;
    if (ctx->timeout) return;
    ctx->timeout = sol_timeout_add(300, timeout_cb, ctx);
}

static struct sol_gpio *
setup_button(const struct light_context *ctx)
{
    struct sol_gpio_config conf = {
        .api_version = SOL_GPIO_CONFIG_API_VERSION,
        .dir = SOL_GPIO_DIR_IN,
        .in = {
            .trigger_mode = SOL_GPIO_EDGE_FALLING,
            .cb = button_pressed,
            .user_data = ctx
        }
    };

    return sol_gpio_open(GPIO(PA, 28), &conf);
}
static bool
setup_server(void)
{
    struct light_context *lc;
    static struct sol_coap_resource light = {
        .api_version = SOL_COAP_RESOURCE_API_VERSION,
        .get = light_method_get,
        .put = light_method_put,
        .iface = SOL_STR_SLICE_LITERAL("oc.mi.def"),
        .resource_type = SOL_STR_SLICE_LITERAL("core.light"),
        .flags = SOL_COAP_FLAGS_WELL_KNOWN | SOL_COAP_FLAGS_OC_CORE,
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

int main(void)
{
    sol_init();

    show_interfaces();

    setup_server();

    sol_run();

    sol_shutdown();

    return 0;
}
