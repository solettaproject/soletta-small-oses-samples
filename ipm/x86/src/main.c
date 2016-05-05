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

#include <soletta.h>
#include <sol-types.h>
#include <sol-log.h>
#include <sol-ipm.h>

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>

#define MESSAGE_ID 1

static const char *samples[] = {"abcdefghijklmno",
                              "abcdefghijklmnopqrstuvwywz",
                              "abcdefghijklmnopqrstuvwywz0123456789",
                              "abcdefghijklmnopqrstuvwywz0123456789ABCDEF"};
static uint32_t count;

static void
consumed_cb(void *data, uint32_t id, struct sol_blob *message)
{
    printf("x86 receiving consumed confirmation id: %" PRIu32 " %p data: %s\n",
        id, message->mem, (char *)message->mem);
    sol_blob_unref(message);
    printf("After unref %p\n", message);
}

static bool
timeout_send_cb(void *data)
{
    int r;
    struct sol_blob *message = sol_blob_new(&SOL_BLOB_TYPE_NO_FREE, NULL,
        samples[count % 4], strlen(samples[count % 4]));

    printf("x86 sending %p - %s\n", message->mem, (char *)message->mem);
    r = sol_ipm_send(MESSAGE_ID, message);
    if (r < 0) {
        printf("x86 could not send message: %d\n", r);
    }

    count++;
    return true;
}

static bool
unref_cb(void *data)
{
    struct sol_blob *blob = data;

    printf("x86 unrefing %p - %s\n", blob->mem, (char *)blob->mem);
    sol_blob_unref(blob);

    return false;
}

static void
receiver_cb(void *data, uint32_t id, struct sol_blob *message)
{
    printf("x86 received %u bytes: %s [%p]\n", (unsigned)message->size, (char *)message->mem,
         message->mem);

    sol_timeout_add(3000, unref_cb, message);
}

static void
startup(void)
{
    sol_ipm_set_receiver(MESSAGE_ID, receiver_cb, NULL);
    sol_ipm_set_consumed_callback(MESSAGE_ID, consumed_cb, NULL);
    sol_timeout_add(5000, timeout_send_cb, NULL);
}

static void
shutdown(void)
{
}

SOL_MAIN_DEFAULT(startup, shutdown);
