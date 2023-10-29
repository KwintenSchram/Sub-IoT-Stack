/*
 * Copyright (c) 2015-2021 University of Antwerp, Aloxy NV, LiQuiBit VOF.
 *
 * This file is part of Sub-IoT.
 * See https://github.com/Sub-IoT/Sub-IoT-Stack for further info.
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

/* \file
 *
 *
 * @author info@liquibit.be
 */
#include "state_machine_file.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "stdint.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define STATE_MACHINE_FILE_ID 79
#define STATE_MACHINE_FILE_SIZE sizeof(state_machine_file_t)
#define RAW_STATE_MACHINE_FILE_SIZE 2

typedef struct {
    union {
        uint8_t bytes[RAW_STATE_MACHINE_FILE_SIZE];
        struct {
            APP_STATE_t current_app_state;
            APP_STATE_t previous_app_state;
        } __attribute__((__packed__));
    };
} state_machine_file_t;

static state_machine_file_t state_machine_file_cached = (state_machine_file_t) {
    .current_app_state = 0,
    .previous_app_state = 0,
};

error_t state_machine_file_initialize()
{

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = STATE_MACHINE_FILE_SIZE,
        .allocated_length = STATE_MACHINE_FILE_SIZE + 10 };

    uint32_t length = STATE_MACHINE_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(STATE_MACHINE_FILE_ID, 0, state_machine_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(STATE_MACHINE_FILE_ID, &permanent_file_header, state_machine_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing state_machine_file: %d", ret);
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading state_machine_file: %d", ret);

    return ret;
}

/**
 * @brief Switch the state of the global state machine. This will shift the new state to current state and the current state to previous
 * @param state The new state we want to change to
 */
uint8_t state_machine_file_switch_state(APP_STATE_t state)
{
    state_machine_file_cached.previous_app_state = state_machine_file_cached.current_app_state;
    state_machine_file_cached.current_app_state = state;
    d7ap_fs_write_file(STATE_MACHINE_FILE_ID, 0, state_machine_file_cached.bytes, STATE_MACHINE_FILE_SIZE, ROOT_AUTH);
    return state_machine_file_cached.previous_app_state;
}
