#include "sol-log.h"

#include "sol-lwm2m.h"
#include "soletta.h"
#include "sol-mainloop.h"
#include "sol-vector.h"
#include "sol-util.h"

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#define LOCATION_OBJ_ID (6)
#define LONGITUDE_ID (1)
#define LATITUDE_ID (0)
#define TIMESTAMP_ID (5)

enum location_object_status {
    LOCATION_OBJECT_NOT_FOUND,
    LOCATION_OBJECT_WITH_NO_INSTANCES,
    LOCATION_OBJECT_WITH_INSTANCES
};

static enum location_object_status
get_location_object_status(const struct sol_lwm2m_client_info *cinfo)
{
    uint16_t i;
    struct sol_lwm2m_client_object *object;
    const struct sol_ptr_vector *objects =
        sol_lwm2m_client_info_get_objects(cinfo);

    SOL_PTR_VECTOR_FOREACH_IDX (objects, object, i) {
        const struct sol_ptr_vector *instances;
        uint16_t id;
        int r;

        r = sol_lwm2m_client_object_get_id(object, &id);
        if (r < 0) {
            SOL_WRN("Could not fetch the object id from %p", object);
            return LOCATION_OBJECT_NOT_FOUND;
        }

        if (id != LOCATION_OBJ_ID)
            continue;

        instances = sol_lwm2m_client_object_get_instances(object);

        if (sol_ptr_vector_get_len(instances))
            return LOCATION_OBJECT_WITH_INSTANCES;
        return LOCATION_OBJECT_WITH_NO_INSTANCES;
    }

    return LOCATION_OBJECT_NOT_FOUND;
}

static void
location_changed_cb(void *data,
    struct sol_lwm2m_server *server,
    struct sol_lwm2m_client_info *cinfo,
    const char *path,
    enum sol_coap_response_code response_code,
    enum sol_lwm2m_content_type content_type,
    struct sol_str_slice content)
{
    struct sol_vector tlvs;
    const char *name = sol_lwm2m_client_info_get_name(cinfo);
    int r;
    uint16_t i;
    struct sol_lwm2m_tlv *tlv;

    if (response_code != SOL_COAP_RESPONSE_CODE_CHANGED &&
        response_code != SOL_COAP_RESPONSE_CODE_CONTENT) {
        SOL_WRN("Could not get the location object value from"
            " client %s", name);
        return;
    }

    if (content_type != SOL_LWM2M_CONTENT_TYPE_TLV) {
        SOL_WRN("The location object content from client %s is not"
            " in TLV format. Received format: %d", name, content_type);
        return;
    }

    r = sol_lwm2m_parse_tlv(content, &tlvs);

    if (r < 0) {
        SOL_WRN("Could not parse the tlv from client: %s", name);
        return;
    }

    SOL_VECTOR_FOREACH_IDX (&tlvs, tlv, i) {
        const char *prop;
        SOL_BUFFER_DECLARE_STATIC(buf, 32);

        if (tlv->id == LATITUDE_ID)
            prop = "latitude";
        else if (tlv->id == LONGITUDE_ID)
            prop = "longitude";
        else
            continue;

        r = sol_lwm2m_tlv_get_bytes(tlv, &buf);
        if (r < 0) {
            SOL_WRN("Could not the %s value from client %s",
                prop, name);
            break;
        }

        SOL_DBG("Client %s %s is %.*s", name, prop, (int)buf.used,
                (char *)buf.data);

        sol_buffer_fini(&buf);
    }
}

static void
observe_location(struct sol_lwm2m_server *server,
    struct sol_lwm2m_client_info *cinfo)
{
    int r;

    r = sol_lwm2m_server_add_observer(server, cinfo, "/6",
        location_changed_cb, NULL);

    if (r < 0)
        SOL_WRN("Could not send an observe request to the location"
            " object");
    else
        SOL_DBG("Observe request to the location object sent");
}

static void
create_cb(void *data,
    struct sol_lwm2m_server *server,
    struct sol_lwm2m_client_info *cinfo, const char *path,
    enum sol_coap_response_code response_code)
{
    const char *name = sol_lwm2m_client_info_get_name(cinfo);

    if (response_code != SOL_COAP_RESPONSE_CODE_CREATED) {
        SOL_WRN("The client %s could not create the location object.",
            name);
        return;
    }

    SOL_DBG("The client %s created the location object."
        " Observing it now.", name);
    observe_location(server, cinfo);
}

static void
create_location_obj(struct sol_lwm2m_server *server,
    struct sol_lwm2m_client_info *cinfo)
{
    int r;
    struct sol_lwm2m_resource res[3];
    size_t i;

    /*
       Send a request the create a location object instance.
       It sets only the mandatory fields.
       The coordinates are the position of the Eiffel tower.
     */
    SOL_LWM2M_RESOURCE_INIT(r, &res[0], LATITUDE_ID, 1,
        SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
        sol_str_slice_from_str("48.858093"));

    if (r < 0) {
        SOL_WRN("Could not init the latitude resource");
        return;
    }

    SOL_LWM2M_RESOURCE_INIT(r, &res[1], LONGITUDE_ID, 1,
        SOL_LWM2M_RESOURCE_DATA_TYPE_STRING,
        sol_str_slice_from_str("2.294694"));

    if (r < 0) {
        SOL_WRN("Could not init the longitude resource");
        return;
    }

    SOL_LWM2M_RESOURCE_INIT(r, &res[2], TIMESTAMP_ID, 1,
        SOL_LWM2M_RESOURCE_DATA_TYPE_TIME,
        (int64_t)time(NULL));

    if (r < 0) {
        SOL_WRN("Could not init the longitude resource");
        return;
    }

    r = sol_lwm2m_server_create_object_instance(server, cinfo, "/6", res,
        sol_util_array_size(res), create_cb, NULL);

    for (i = 0; i < sol_util_array_size(res); i++)
        sol_lwm2m_resource_clear(&res[i]);

    if (r < 0)
        SOL_WRN("Could not send a request to create a"
            " location object");
    else
        SOL_DBG("Creation request sent");
}

static void
registration_cb(void *data,
    struct sol_lwm2m_server *server,
    struct sol_lwm2m_client_info *cinfo,
    enum sol_lwm2m_registration_event event)
{
    const char *name;
    enum location_object_status status;

    name = sol_lwm2m_client_info_get_name(cinfo);

    if (event == SOL_LWM2M_REGISTRATION_EVENT_UPDATE) {
        SOL_DBG("Client %s updated", name);
        return;
    } else if (event == SOL_LWM2M_REGISTRATION_EVENT_UNREGISTER) {
        SOL_DBG("Client %s unregistered", name);
        return;
    } else if (event == SOL_LWM2M_REGISTRATION_EVENT_TIMEOUT) {
        SOL_DBG("Client %s timeout", name);
        return;
    }

    SOL_DBG("Client %s registered", name);
    status = get_location_object_status(cinfo);

    if (status == LOCATION_OBJECT_NOT_FOUND) {
        SOL_WRN(
            "The client %s does not implement the location object!",
            name);
    } else if (status == LOCATION_OBJECT_WITH_NO_INSTANCES) {
        SOL_DBG("The client %s does not have an instance of the location"
            " object. Creating one.", name);
        create_location_obj(server, cinfo);
    } else {
        SOL_DBG("The client %s have an location object instance,"
            " observing", name);
        observe_location(server, cinfo);
    }
}

static bool
setup_server(void)
{
    struct sol_lwm2m_server *server;
    uint16_t port = SOL_LWM2M_DEFAULT_SERVER_PORT;
    int r, ret = false;

    SOL_DBG("Using the default LWM2M port (%" PRIu16 ")", port);

    server = sol_lwm2m_server_new(port);
    if (!server) {
        SOL_WRN("Could not create the LWM2M server");
        goto exit;
    }

    r = sol_lwm2m_server_add_registration_monitor(server, registration_cb,
        NULL);
    if (r < 0) {
        SOL_WRN("Could not add a registration monitor");
        goto exit_del;
    }

    ret = true;

    SOL_DBG("setup_server() ok");
    return ret;

exit_del:
    sol_lwm2m_server_del(server);
exit:
    sol_shutdown();
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
                SOL_BUFFER_DECLARE_STATIC(buf, SOL_NETWORK_INET_ADDR_STR_LEN);
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

    SOL_WRN("Setting up LWM2M server");
    setup_server();
}
SOL_MAIN_DEFAULT(startup, NULL);
