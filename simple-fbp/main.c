/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
