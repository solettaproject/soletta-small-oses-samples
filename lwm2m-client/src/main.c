#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

#include <soletta.h>
#include <sol-log.h>
#include <sol-lwm2m.h>
#include <sol-util.h>
#include <sol-vector.h>

#define LOCATION_OBJ_ID (6)
#define LOCATION_OBJ_LATITUDE_RES_ID (0)
#define LOCATION_OBJ_LONGITUDE_RES_ID (1)
#define LOCATION_OBJ_TIMESTAMP_RES_ID (5)

#define ONE_SECOND (1000)
#define LIFETIME (60)

#define SERVER_OBJ_ID (1)
#define SERVER_OBJ_SHORT_RES_ID (0)
#define SERVER_OBJ_LIFETIME_RES_ID (1)
#define SERVER_OBJ_BINDING_RES_ID (7)
#define SERVER_OBJ_REGISTRATION_UPDATE_RES_ID (8)

#define SECURITY_SERVER_OBJ_ID (0)
#define SECURITY_SERVER_SERVER_URI_RES_ID (0)
#define SECURITY_SERVER_IS_BOOTSTRAP_RES_ID (1)
#define SECURITY_SERVER_SERVER_ID_RES_ID (10)

struct location_obj_instance_ctx {
    struct sol_timeout *timeout;
    struct sol_lwm2m_client *client;
    char *latitude;
    char *longitude;
    int64_t timestamp;
};

static char *
generate_new_coord(void)
{
    char *p;
    int r;
    double v = ((double)rand() / (double)RAND_MAX);

    r = asprintf(&p, "%g", v);
    if (r < 0)
        return NULL;
    return p;
}

static bool
change_location(void *data)
{
    struct location_obj_instance_ctx *instance_ctx = data;
    char *latitude, *longitude;
    int r = 0;
    static const char *paths[] = { "/6/0/0",
                                   "/6/0/1", "/6/0/5", NULL };

    latitude = longitude = NULL;

    latitude = generate_new_coord();

    if (!latitude) {
        SOL_WRN("Could not generate a new latitude");
        return true;
    }

    longitude = generate_new_coord();
    if (!longitude) {
        SOL_WRN("Could not generate a new longitude");
        free(latitude);
        return true;
    }

    free(instance_ctx->latitude);
    free(instance_ctx->longitude);
    instance_ctx->latitude = latitude;
    instance_ctx->longitude = longitude;

    instance_ctx->timestamp = (int64_t)time(NULL);

    SOL_DBG("New latitude: %s - New longitude: %s", instance_ctx->latitude,
        instance_ctx->longitude);

    r = sol_lwm2m_notify_observers(instance_ctx->client, paths);

    if (r < 0) {
        SOL_WRN("Could not notify the observers");
    } else
        SOL_DBG("Sending new location coordinates to the observers");

    return true;
}

static int
create_location_obj(void *user_data, struct sol_lwm2m_client *client,
    uint16_t instance_id, void **instance_data,
    enum sol_lwm2m_content_type content_type,
    const struct sol_str_slice content)
{
    struct location_obj_instance_ctx *instance_ctx;
    bool *has_location_instance = user_data;
    struct sol_vector tlvs;
    int r;
    uint16_t i;
    struct sol_lwm2m_tlv *tlv;

    //Only one location object is allowed
    if (*has_location_instance) {
        SOL_WRN("Only one location object instance is allowed");
        return -EINVAL;
    }

    if (content_type != SOL_LWM2M_CONTENT_TYPE_TLV) {
        SOL_WRN("Content type is not in TLV format");
        return -EINVAL;
    }

    instance_ctx = calloc(1, sizeof(struct location_obj_instance_ctx));
    if (!instance_ctx) {
        SOL_WRN("Could not alloc memory for location object context");
        return -ENOMEM;
    }

    instance_ctx->timeout = sol_timeout_add(ONE_SECOND, change_location,
        instance_ctx);
    if (!instance_ctx->timeout) {
        SOL_WRN("Could not create the client timer");
        r = -ENOMEM;
        goto err_free_instance;
    }

    r = sol_lwm2m_parse_tlv(content, &tlvs);
    if (r < 0) {
        SOL_WRN("Could not parse the TLV content");
        goto err_free_timeout;
    }

    if (tlvs.len != 3) {
        r = -EINVAL;
        SOL_WRN("Missing mandatory fields.");
        goto err_free_tlvs;
    }

    SOL_VECTOR_FOREACH_IDX (&tlvs, tlv, i) {
        uint8_t *bytes;
        uint16_t bytes_len;
        char **prop = NULL;

        bytes_len = 0;

        if (tlv->id == LOCATION_OBJ_LATITUDE_RES_ID) {
            r = sol_lwm2m_tlv_get_bytes(tlv, &bytes, &bytes_len);
            prop = &instance_ctx->latitude;
        } else if (tlv->id == LOCATION_OBJ_LONGITUDE_RES_ID) {
            r = sol_lwm2m_tlv_get_bytes(tlv, &bytes, &bytes_len);
            prop = &instance_ctx->longitude;
        } else
            r = sol_lwm2m_tlv_to_int(tlv, &instance_ctx->timestamp);

        if (r < 0) {
            SOL_WRN("Could not get the tlv value for resource %"
                PRIu16, tlv->id);
            goto err_free_tlvs;
        }

        if (bytes_len) {
            *prop = strndup((const char *)bytes, bytes_len);
            if (!*prop) {
                r = -ENOMEM;
                SOL_WRN("Could not copy the longitude/latitude"
                    " property");
                goto err_free_tlvs;
            }
        }
    }

    instance_ctx->client = client;
    *instance_data = instance_ctx;
    *has_location_instance = true;
    sol_lwm2m_tlv_array_clear(&tlvs);
    SOL_DBG("Location object created");

    return 0;

err_free_tlvs:
    sol_lwm2m_tlv_array_clear(&tlvs);
err_free_timeout:
    sol_timeout_del(instance_ctx->timeout);
    free(instance_ctx->longitude);
    free(instance_ctx->latitude);
err_free_instance:
    free(instance_ctx);
    return r;
}

static int
read_location_obj(void *instance_data, void *user_data,
    struct sol_lwm2m_client *client, uint16_t instance_id,
    uint16_t res_id, struct sol_lwm2m_resource *res)
{
    struct location_obj_instance_ctx *ctx = instance_data;
    int r;

    switch (res_id) {
    case LOCATION_OBJ_LATITUDE_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
            sol_str_slice_from_str(ctx->latitude));
        break;
    case LOCATION_OBJ_LONGITUDE_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
            sol_str_slice_from_str(ctx->longitude));
        break;
    case LOCATION_OBJ_TIMESTAMP_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_TIME, ctx->timestamp);
        break;
    default:
        if (res_id >= 2 && res_id <= 4)
            r = -ENOENT;
        else
            r = -EINVAL;
    }

    return r;
}

static int
read_security_server_obj(void *instance_data, void *user_data,
    struct sol_lwm2m_client *client,
    uint16_t instance_id, uint16_t res_id, struct sol_lwm2m_resource *res)
{
    int r;

    //It implements only the necassary info to connect to a LWM2M
    //server Without encryption.
    switch (res_id) {
    case SECURITY_SERVER_SERVER_URI_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, 0, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
            sol_str_slice_from_str("coap://[fe80::5846:1502:7238:bcee]:5683"));
        break;
    case SECURITY_SERVER_IS_BOOTSTRAP_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, 1, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_BOOLEAN, false);
        break;
    case SECURITY_SERVER_SERVER_ID_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, 10, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_INT, (int64_t)101);
        break;
    default:
        if (res_id >= 2 && res_id <= 11)
            r = -ENOENT;
        else
            r = -EINVAL;
    }

    return r;
}

static int
read_server_obj(void *instance_data, void *user_data,
    struct sol_lwm2m_client *client,
    uint16_t instance_id, uint16_t res_id, struct sol_lwm2m_resource *res)
{
    int r;

    //It implements only the necassary info to connect to a LWM2M
    //server Without encryption.

    switch (res_id) {
    case SERVER_OBJ_SHORT_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_INT, (int64_t)101);
        break;
    case SERVER_OBJ_LIFETIME_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_INT, (int64_t)LIFETIME);
        break;
    case SERVER_OBJ_BINDING_RES_ID:
        SOL_LWM2M_RESOURCE_INIT(r, res, res_id, 1,
            SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
            sol_str_slice_from_str("U"));
        break;
    default:
        if (res_id >= 2 && res_id <= 6)
            r = -ENOENT;
        else
            r = -EINVAL;
    }

    return r;
}

static int
execute_server_obj(void *instance_data, void *user_data,
    struct sol_lwm2m_client *client, uint16_t instance_id,
    uint16_t res_id, const struct sol_str_slice args)
{
    if (res_id != SERVER_OBJ_REGISTRATION_UPDATE_RES_ID)
        return -EINVAL;

    return sol_lwm2m_send_update(client);
}

static int
del_location_obj(void *instance_data, void *user_data,
    struct sol_lwm2m_client *client, uint16_t instance_id)
{
    struct location_obj_instance_ctx *instance_ctx = instance_data;
    bool *has_location_instance = user_data;

    if (instance_ctx->timeout)
        sol_timeout_del(instance_ctx->timeout);
    free(instance_ctx->latitude);
    free(instance_ctx->longitude);
    free(instance_ctx);
    *has_location_instance = false;
    return 0;
}

static const struct sol_lwm2m_object location_object = {
    SOL_SET_API_VERSION(.api_version = SOL_LWM2M_OBJECT_API_VERSION, )
    .id = LOCATION_OBJ_ID,
    .create = create_location_obj,
    .read = read_location_obj,
    .del = del_location_obj,
    .resources_count = 6
};

static const struct sol_lwm2m_object security_object = {
    SOL_SET_API_VERSION(.api_version = SOL_LWM2M_OBJECT_API_VERSION, )
    .id = SECURITY_SERVER_OBJ_ID,
    .resources_count = 12,
    .read = read_security_server_obj
};

static const struct sol_lwm2m_object server_object = {
    SOL_SET_API_VERSION(.api_version = SOL_LWM2M_OBJECT_API_VERSION, )
    .id = SERVER_OBJ_ID,
    .resources_count = 9,
    .read = read_server_obj,
    .execute = execute_server_obj
};

static bool
setup_client(void)
{
    struct sol_lwm2m_client *client;
    static const struct sol_lwm2m_object *objects[] =
    { &security_object, &server_object, &location_object, NULL };
    static bool has_location_instance = false;
    bool ret = false;
    int r;

    srand(time(NULL));

    client = sol_lwm2m_client_new("lwm2m-client", NULL, NULL, objects,
        &has_location_instance);

    if (!client) {
        SOL_WRN("Could not the create the LWM2M client");
        goto exit;
    }

    r = sol_lwm2m_add_object_instance(client, &server_object, NULL);
    if (r < 0) {
        SOL_WRN("Could not add a server object instance");
        goto exit_del;
    }

    r = sol_lwm2m_add_object_instance(client, &security_object, NULL);

    if (r < 0) {
        SOL_WRN("Could not add a security object instance");
        goto exit_del;
    }

    sol_lwm2m_client_start(client);

    ret = true;
    return ret;

exit_del:
    sol_lwm2m_client_del(client);
exit:
    return ret;
}

static void
show_interfaces(void)
{
    const struct sol_vector *links;

    links = sol_network_get_available_links();
    if (links) {
        const struct sol_network_link *l;
        uint16_t i;

        SOL_DBG("Found %d links", links->len);
        SOL_VECTOR_FOREACH_IDX (links, l, i) {
            uint16_t j;
            const struct sol_network_link_addr *addr;

            SOL_DBG("Link #%d", i);
            SOL_VECTOR_FOREACH_IDX (&l->addrs, addr, j) {
                SOL_BUFFER_DECLARE_STATIC(buf, SOL_INET_ADDR_STRLEN);
                const char *ret;

                ret = sol_network_link_addr_to_str(addr, &buf);
                if (ret)
                    SOL_DBG("\tAddr #%d: %.*s", j,
                        SOL_STR_SLICE_PRINT(sol_buffer_get_slice(&buf)));
            }
        }
    } else {
        SOL_DBG("No interfaces?");
    }
}

static void
startup(void)
{
    SOL_WRN("Showing interfaces");
    show_interfaces();

    setup_client();
}
SOL_MAIN_DEFAULT(startup, NULL);
