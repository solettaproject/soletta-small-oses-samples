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

#include <stdio.h>

#include <boolean-gen.h>
#include <gpio-gen.h>
#include <sol-flow-static.h>
#include <sol-mainloop.h>

static struct sol_flow_node_type *
create_flow(void)
{
    struct sol_flow_node_type_gpio_reader_options button_opts =
        SOL_FLOW_NODE_TYPE_GPIO_READER_OPTIONS_DEFAULTS(
            .pin = {
                .val = 0x4100441c
            },
            .edge_falling = true
        );
    struct sol_flow_node_type_gpio_writer_options led_opts =
        SOL_FLOW_NODE_TYPE_GPIO_WRITER_OPTIONS_DEFAULTS(
            .pin = {
                .val = 0x41004413
            },
            .active_low = true
        );
    struct sol_flow_static_node_spec nodes[] = {
        [0] = { NULL, "button", &button_opts.base },
        [1] = { NULL, "toggle", NULL },
        [2] = { NULL, "led", &led_opts.base },
        SOL_FLOW_STATIC_NODE_SPEC_GUARD
    };

    struct sol_flow_static_conn_spec conns[] = {
        { 0, SOL_FLOW_NODE_TYPE_GPIO_READER__OUT__OUT,
          1, SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE__IN__IN },
        { 1, SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE__OUT__OUT,
          2, SOL_FLOW_NODE_TYPE_GPIO_WRITER__IN__IN },
        SOL_FLOW_STATIC_CONN_SPEC_GUARD
    };

    struct sol_flow_static_spec spec = {
        .api_version = SOL_FLOW_STATIC_API_VERSION,
        .nodes = nodes,
        .conns = conns
    };

    nodes[0].type = SOL_FLOW_NODE_TYPE_GPIO_READER;
    nodes[1].type = SOL_FLOW_NODE_TYPE_BOOLEAN_TOGGLE;
    nodes[2].type = SOL_FLOW_NODE_TYPE_GPIO_WRITER;

    return sol_flow_static_new_type(&spec);
}

int main(void)
{
    struct sol_flow_node_type *type;
    struct sol_flow_node *node;

    sol_init();
    sol_log_set_level(10);

    type = create_flow();
    node = sol_flow_node_new(NULL, NULL, type, NULL);
    if (!node)
        return -1;

    sol_run();

    sol_shutdown();

    return 0;
}
