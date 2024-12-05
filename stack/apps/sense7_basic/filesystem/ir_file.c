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
#include "ir_file.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "platform.h"
#include "stdint.h"
#include "timer.h"
#include "sths34_STM32.h"
#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define IR_FILE_ID 75
#define IR_FILE_SIZE sizeof(ir_file_t)
#define RAW_IR_FILE_SIZE 2

#define IR_CONFIG_FILE_ID 76
#define IR_CONFIG_FILE_SIZE sizeof(ir_config_file_t)
#define RAW_IR_CONFIG_FILE_SIZE 1

typedef struct {
    union {
        uint8_t bytes[RAW_IR_FILE_SIZE];
        struct {
            bool motion_state;
            bool presence_state;
        } __attribute__((__packed__));
    };
} ir_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_IR_CONFIG_FILE_SIZE];
        struct {
            bool enabled;
        } __attribute__((__packed__));
    };
} ir_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void sths34_data_change_callback(bool motion_state, bool presence_state);

static ir_config_file_t ir_config_file_cached = (ir_config_file_t) {
    .enabled = false };

static bool ir_file_transmit_state = false;
static bool ir_config_file_transmit_state = false;
static bool test_mode_state = false;

error_t ir_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = IR_FILE_SIZE,
              .allocated_length = IR_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = IR_CONFIG_FILE_SIZE,
        .allocated_length = IR_CONFIG_FILE_SIZE + 10 };

    uint32_t length = IR_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(IR_CONFIG_FILE_ID, 0, ir_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(IR_CONFIG_FILE_ID, &permanent_file_header, ir_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing pir configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading pir configuration file: %d", ret);

    ir_file_t ir_file = {
        0,
    };

    ret = d7ap_fs_init_file(IR_FILE_ID, &volatile_file_header, ir_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    // register callbacks on any modification of the IR (config) file
    d7ap_fs_register_file_modified_callback(IR_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(IR_FILE_ID, &file_modified_callback);
    shts34_interface_init(platf_get_i2c_handle());
    shts34_setup_presence_detection(PIR_INT_PIN, false, false, &sths34_data_change_callback);
    
}


static void sths34_data_change_callback(bool motion_state, bool presence_state)
{
    ir_file_t ir_file = { .motion_state = motion_state, .presence_state = presence_state };
    d7ap_fs_write_file(IR_FILE_ID, 0, ir_file.bytes, IR_FILE_SIZE, ROOT_AUTH);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == IR_CONFIG_FILE_ID) {
        // pir config file got modified, apply settings and send file
        uint32_t size = IR_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(IR_CONFIG_FILE_ID, 0, ir_config_file_cached.bytes, &size, ROOT_AUTH);
        //TODO adjust settings
        
        if (ir_config_file_transmit_state)
            queue_add_file(ir_config_file_cached.bytes, IR_CONFIG_FILE_SIZE, IR_CONFIG_FILE_ID);
    } else if (file_id == IR_FILE_ID) {
        // pir file got modified (internally), send file if configuration allows for it
        ir_file_t ir_file;
        uint32_t size = IR_FILE_SIZE;
        d7ap_fs_read_file(IR_FILE_ID, 0, ir_file.bytes, &size, ROOT_AUTH);
        queue_add_file(ir_file.bytes, IR_FILE_SIZE, IR_FILE_ID);
    }
}

void ir_file_transmit_config_file()
{
    uint32_t size = IR_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(IR_CONFIG_FILE_ID, 0, ir_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(ir_config_file_cached.bytes, IR_CONFIG_FILE_SIZE, IR_CONFIG_FILE_ID);
}

/**
 * @brief This function allows us to disable the measurement state of the sensor.
 *
 * @param enable
 */
void ir_file_set_measure_state(bool enable)
{
    // enable or disable pir
    if (ir_file_transmit_state != enable) {
        ir_file_transmit_state = enable;
        shts34_set_interrupt_enabled_state(enable);
    }
}

/**
 * @brief This function allows us to overwrite any enable restriction so that we can verify the functionality.
 *
 * @param enable
 */
void ir_file_set_test_mode(bool enable)
{
    // enable test mode which overrides the current configuration to always send
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    shts34_set_interrupt_enabled_state(enable);
    if (enable) {
        ir_config_file_cached.enabled = true;
    } else {
        uint32_t size = IR_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(IR_CONFIG_FILE_ID, 0, ir_config_file_cached.bytes, &size, ROOT_AUTH);
    }
}

bool ir_file_is_enabled() { return ir_config_file_cached.enabled; }

void ir_file_set_enabled(bool enable)
{
    if (ir_config_file_cached.enabled != enable) {
        ir_config_file_cached.enabled = enable;
        d7ap_fs_write_file(IR_CONFIG_FILE_ID, 0, ir_config_file_cached.bytes, RAW_IR_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}
