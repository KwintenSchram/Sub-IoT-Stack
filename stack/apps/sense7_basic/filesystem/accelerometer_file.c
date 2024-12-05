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
 * @author contact@liquibit.be
 */
#include "accelerometer_file.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "platform.h"
#include "stdint.h"
#include "stm32_common_gpio.h"
#include "timer.h"
#include "bma400.h"
#include "bma400_STM32.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define ACCELEROMETER_FILE_ID 73
#define ACCELEROMETER_FILE_SIZE sizeof(accelerometer_file_t)
#define RAW_ACCELEROMETER_FILE_SIZE 6

#define ACCELEROMETER_CONFIG_FILE_ID 74
#define ACCELEROMETER_CONFIG_FILE_SIZE sizeof(accelerometer_config_file_t)
#define RAW_ACCELEROMETER_CONFIG_FILE_SIZE 3

typedef struct {
    union {
        uint8_t bytes[RAW_ACCELEROMETER_FILE_SIZE];
        struct {
            bool current_motion_state;
            uint32_t steps;
            uint8_t current_activity_type;
        } __attribute__((__packed__));
    };
} accelerometer_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_ACCELEROMETER_CONFIG_FILE_SIZE];
        struct {
            bool transmit_mask_0;
            bool transmit_mask_1;
            bool enabled;

        } __attribute__((__packed__));
    };
} accelerometer_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void accelerometer_interrupt_callback(bma400_data_t data);

static accelerometer_config_file_t accelerometer_config_file_cached
    = (accelerometer_config_file_t) { .transmit_mask_0 = true, .transmit_mask_1 = true, .enabled = true };

static bool accelerometer_file_transmit_state = false;
static bool accelerometer_config_file_transmit_state = false;
static bool test_mode_state = false;


error_t accelerometer_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = ACCELEROMETER_FILE_SIZE,
              .allocated_length = ACCELEROMETER_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = ACCELEROMETER_CONFIG_FILE_SIZE,
        .allocated_length = ACCELEROMETER_CONFIG_FILE_SIZE + 10 };

    uint32_t length = ACCELEROMETER_CONFIG_FILE_SIZE;
    error_t ret
        = d7ap_fs_read_file(ACCELEROMETER_CONFIG_FILE_ID, 0, accelerometer_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(
            ACCELEROMETER_CONFIG_FILE_ID, &permanent_file_header, accelerometer_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing hall effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading hall effect configuration file: %d", ret);

    accelerometer_file_t accelerometer_file = {
        0,
    };

    ret = d7ap_fs_init_file(ACCELEROMETER_FILE_ID, &volatile_file_header, accelerometer_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    d7ap_fs_register_file_modified_callback(ACCELEROMETER_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(ACCELEROMETER_FILE_ID, &file_modified_callback);

    bma400_interface_init(platf_get_i2c_handle());
    bma400_setup_interrupts(false, false, ACCELEROMETER_INT_PIN, &accelerometer_interrupt_callback);
}

static void accelerometer_interrupt_callback(bma400_data_t data)
{
    accelerometer_file_t accelerometer_file = { .current_motion_state = data.current_motion_state, .steps = data.steps, .current_activity_type = data.current_activity_type };
    d7ap_fs_write_file(ACCELEROMETER_FILE_ID, 0, accelerometer_file.bytes, ACCELEROMETER_FILE_SIZE, ROOT_AUTH);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == ACCELEROMETER_CONFIG_FILE_ID && accelerometer_config_file_transmit_state) {
        uint32_t size = ACCELEROMETER_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(ACCELEROMETER_CONFIG_FILE_ID, 0, accelerometer_config_file_cached.bytes, &size, ROOT_AUTH);
        queue_add_file(accelerometer_config_file_cached.bytes, ACCELEROMETER_CONFIG_FILE_SIZE, ACCELEROMETER_CONFIG_FILE_ID);
    } else if (file_id == ACCELEROMETER_FILE_ID && accelerometer_file_transmit_state) {
        accelerometer_file_t accelerometer_file;
        uint32_t size = ACCELEROMETER_FILE_SIZE;
        d7ap_fs_read_file(ACCELEROMETER_FILE_ID, 0, accelerometer_file.bytes, &size, ROOT_AUTH);
        if ((accelerometer_file.current_motion_state == true && accelerometer_config_file_cached.transmit_mask_1)
            || (accelerometer_file.current_motion_state == false && accelerometer_config_file_cached.transmit_mask_0))
            if (accelerometer_config_file_cached.enabled)
                queue_add_file(accelerometer_file.bytes, ACCELEROMETER_FILE_SIZE, ACCELEROMETER_FILE_ID);
    }
}

void accelerometer_file_transmit_config_file()
{
    uint32_t size = ACCELEROMETER_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(ACCELEROMETER_CONFIG_FILE_ID, 0, accelerometer_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(accelerometer_config_file_cached.bytes, ACCELEROMETER_CONFIG_FILE_SIZE, ACCELEROMETER_CONFIG_FILE_ID);
}

void accelerometer_file_set_measure_state(bool enable)
{
    accelerometer_file_transmit_state = enable;
    accelerometer_config_file_transmit_state = enable;
    bma400_set_interrupt_enabled_state(enable);
}

void accelerometer_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    if (enable) {
        accelerometer_config_file_cached.transmit_mask_0 = true;
        accelerometer_config_file_cached.transmit_mask_1 = true;
        accelerometer_config_file_cached.enabled = true;
    } else {
        uint32_t size = ACCELEROMETER_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(ACCELEROMETER_CONFIG_FILE_ID, 0, accelerometer_config_file_cached.bytes, &size, ROOT_AUTH);
    }
}

bool accelerometer_file_is_enabled() { return accelerometer_config_file_cached.enabled; }

void accelerometer_file_set_enabled(bool enable)
{
    if (accelerometer_config_file_cached.enabled != enable) {
        accelerometer_config_file_cached.enabled = enable;
        d7ap_fs_write_file(ACCELEROMETER_CONFIG_FILE_ID, 0, accelerometer_config_file_cached.bytes,
            ACCELEROMETER_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}
