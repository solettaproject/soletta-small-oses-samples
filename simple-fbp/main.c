#include "sol-flow.h"
#include "sol-flow-static.h"
#include "sol-mainloop.h"

#include "timer-gen.h"
#include "boolean-gen.h"
#include "console-gen.h"

static const struct sol_flow_node_type *
create_0_root_type(void)
{
    static const struct sol_flow_node_type_timer_options opts0 =
        SOL_FLOW_NODE_TYPE_TIMER_OPTIONS_DEFAULTS(
            .interval = {
                .val = 1000,
            },
        );

    static const struct sol_flow_static_conn_spec conns[] = {
        { 0, 0, 1, 0 },
        { 1, 0, 2, 0 },
        { 1, 0, 3, 0 },
        { 3, 0, 4, 0 },
        SOL_FLOW_STATIC_CONN_SPEC_GUARD
    };


    static struct sol_flow_static_node_spec nodes[] = {
        [0] = {NULL, "timer", (struct sol_flow_node_options *) &opts0},
        [1] = {NULL, "toggle", NULL},
        [2] = {NULL, "console_toggle", NULL},
        [3] = {NULL, "not", NULL},
        [4] = {NULL, "console_not", NULL},
        SOL_FLOW_STATIC_NODE_SPEC_GUARD
    };

    struct sol_flow_static_spec spec = {
        .api_version = SOL_FLOW_STATIC_API_VERSION,
        .nodes = nodes,
        .conns = conns,
        .exported_in = NULL,
        .exported_out = NULL,
    };

    nodes[0].type = SOL_FLOW_NODE_TYPE_TIMER;
    nodes[1].type = SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE;
    nodes[2].type = SOL_FLOW_NODE_TYPE_CONSOLE;
    nodes[3].type = SOL_FLOW_NODE_TYPE_BOOLEAN_NOT;
    nodes[4].type = SOL_FLOW_NODE_TYPE_CONSOLE;

    return sol_flow_static_new_type(&spec);
}

static void
initialize_types(void)
{
    if (SOL_FLOW_NODE_TYPE_TIMER->init_type)
        SOL_FLOW_NODE_TYPE_TIMER->init_type();
    if (SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE->init_type)
        SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE->init_type();
    if (SOL_FLOW_NODE_TYPE_CONSOLE->init_type)
        SOL_FLOW_NODE_TYPE_CONSOLE->init_type();
    if (SOL_FLOW_NODE_TYPE_BOOLEAN_NOT->init_type)
        SOL_FLOW_NODE_TYPE_BOOLEAN_NOT->init_type();
}

static struct sol_flow_node *flow;

static void
startup(void)
{
    const struct sol_flow_node_type *type;

    initialize_types();
    type = create_0_root_type();
    if (!type)
        return;

    flow = sol_flow_node_new(NULL, NULL, type, NULL);
}

static void
shutdown(void)
{
    sol_flow_node_del(flow);
}

SOL_MAIN_DEFAULT(startup, shutdown);
