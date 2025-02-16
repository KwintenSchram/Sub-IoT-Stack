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
#include "pir_file.h"
#include "PYD1598.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "platform.h"
#include "stdint.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define PIR_FILE_ID 58
#define PIR_FILE_SIZE sizeof(pir_file_t)
#define RAW_PIR_FILE_SIZE 1

#define PIR_CONFIG_FILE_ID 68
#define PIR_CONFIG_FILE_SIZE sizeof(pir_config_file_t)
#define RAW_PIR_CONFIG_FILE_SIZE 9

typedef struct {
    union {
        uint8_t bytes[RAW_PIR_FILE_SIZE];
        struct {
            bool mask;
        } __attribute__((__packed__));
    };
} pir_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_PIR_CONFIG_FILE_SIZE];
        struct {
            bool transmit_mask_0;
            bool transmit_mask_1;
            uint8_t filter_source; // PYD1598_FILTER_SOURCE_t
            uint8_t window_time; // Window time = [RegisterValue] * 2s + 2s
            uint8_t pulse_counter; // Amount of pulses = [RegisterValue] + 1
            uint16_t blind_time; // seconds
            uint8_t threshold;
            bool enabled;
        } __attribute__((__packed__));
    };
} pir_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void pir_interrupt_callback(bool mask);

static pir_config_file_t pir_config_file_cached = (pir_config_file_t) { .transmit_mask_0 = true,
    .transmit_mask_1 = true,
    .filter_source = 0,
    .window_time = 1,
    .pulse_counter = 1,
    .blind_time = 14,
    .threshold = 0x18,
    .enabled = false };

static bool pir_file_transmit_state = false;
static bool pir_config_file_transmit_state = false;
static bool test_mode_state = false;

error_t pir_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = PIR_FILE_SIZE,
              .allocated_length = PIR_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = PIR_CONFIG_FILE_SIZE,
        .allocated_length = PIR_CONFIG_FILE_SIZE + 10 };

    uint32_t length = PIR_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(PIR_CONFIG_FILE_ID, 0, pir_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(PIR_CONFIG_FILE_ID, &permanent_file_header, pir_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing pir configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading pir configuration file: %d", ret);

    pir_file_t pir_file = {
        0,
    };

    ret = d7ap_fs_init_file(PIR_FILE_ID, &volatile_file_header, pir_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    // register callbacks on any modification of the PIR (config) file
    d7ap_fs_register_file_modified_callback(PIR_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(PIR_FILE_ID, &file_modified_callback);

    // initialize the PIR sensor and default disable it
    PYD1598_init(PIR_IN_PIN, PIR_OUT_PIN);
    PYD1598_set_settings(pir_config_file_cached.filter_source, pir_config_file_cached.window_time,
        pir_config_file_cached.pulse_counter, pir_config_file_cached.blind_time, pir_config_file_cached.threshold);
    PYD1598_register_callback(&pir_interrupt_callback);
    platf_set_PIR_power_state(false);
    PYD1598_set_state(false);
}

static void pir_interrupt_callback(bool mask)
{
    pir_file_t pir_file = { .mask = mask };
    d7ap_fs_write_file(PIR_FILE_ID, 0, pir_file.bytes, PIR_FILE_SIZE, ROOT_AUTH);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == PIR_CONFIG_FILE_ID) {
        // pir config file got modified, apply settings and send file
        uint32_t size = PIR_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PIR_CONFIG_FILE_ID, 0, pir_config_file_cached.bytes, &size, ROOT_AUTH);
        PYD1598_set_settings(pir_config_file_cached.filter_source, pir_config_file_cached.window_time,
            pir_config_file_cached.pulse_counter, pir_config_file_cached.blind_time, pir_config_file_cached.threshold);
        platf_set_PIR_power_state(pir_config_file_cached.enabled && pir_file_transmit_state);
        PYD1598_set_state(pir_config_file_cached.enabled && pir_file_transmit_state);
        if (pir_config_file_transmit_state)
            queue_add_file(pir_config_file_cached.bytes, PIR_CONFIG_FILE_SIZE, PIR_CONFIG_FILE_ID);
    } else if (file_id == PIR_FILE_ID) {
        // pir file got modified (internally), send file if configuration allows for it
        pir_file_t pir_file;
        uint32_t size = PIR_FILE_SIZE;
        d7ap_fs_read_file(PIR_FILE_ID, 0, pir_file.bytes, &size, ROOT_AUTH);
        if ((pir_file.mask == true && pir_config_file_cached.transmit_mask_1)
            || (pir_file.mask == false && pir_config_file_cached.transmit_mask_0))
            queue_add_file(pir_file.bytes, PIR_FILE_SIZE, PIR_FILE_ID);
    }
}

void pir_file_transmit_config_file()
{
    uint32_t size = PIR_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(PIR_CONFIG_FILE_ID, 0, pir_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(pir_config_file_cached.bytes, PIR_CONFIG_FILE_SIZE, PIR_CONFIG_FILE_ID);
}

/**
 * @brief This function allows us to disable the measurement state of the sensor.
 *
 * @param enable
 */
void pir_file_set_measure_state(bool enable)
{
    // enable or disable pir
    if (pir_file_transmit_state != enable) {
        pir_file_transmit_state = enable;
        platf_set_PIR_power_state(enable && pir_config_file_cached.enabled);
        PYD1598_set_state(enable && pir_config_file_cached.enabled);
    }
}

/**
 * @brief This function allows us to overwrite any enable restriction so that we can verify the functionality.
 *
 * @param enable
 */
void pir_file_set_test_mode(bool enable)
{
    // enable test mode which overrides the current configuration to always send
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    if (enable) {
        platf_set_PIR_power_state(true);
        PYD1598_set_state(true);
        pir_config_file_cached.transmit_mask_0 = true;
        pir_config_file_cached.transmit_mask_1 = true;
        pir_config_file_cached.enabled = true;
    } else {
        uint32_t size = PIR_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PIR_CONFIG_FILE_ID, 0, pir_config_file_cached.bytes, &size, ROOT_AUTH);
        platf_set_PIR_power_state(pir_config_file_cached.enabled && pir_file_transmit_state);
        PYD1598_set_state(pir_config_file_cached.enabled && pir_file_transmit_state);
    }
}

bool pir_file_is_enabled() { return pir_config_file_cached.enabled; }

void pir_file_set_enabled(bool enable)
{
    if (pir_config_file_cached.enabled != enable) {
        pir_config_file_cached.enabled = enable;
        d7ap_fs_write_file(PIR_CONFIG_FILE_ID, 0, pir_config_file_cached.bytes, RAW_PIR_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}
