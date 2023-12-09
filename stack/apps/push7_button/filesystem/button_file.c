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
#include "button_file.h"
#include "button.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "led.h"
#include "little_queue.h"
#include "log.h"
#include "stdint.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define BUTTON_FILE_ID 51
#define BUTTON_FILE_SIZE sizeof(button_file_t)
#define RAW_BUTTON_FILE_SIZE 3

#define BUTTON_CONFIG_FILE_ID 61
#define BUTTON_CONFIG_FILE_SIZE sizeof(button_config_file_t)
#define RAW_BUTTON_CONFIG_FILE_SIZE 4

typedef struct {
    union {
        uint8_t bytes[RAW_BUTTON_FILE_SIZE];
        struct {
            uint8_t button_id;
            bool mask;
            uint8_t buttons_state;
        } __attribute__((__packed__));
    };
} button_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_BUTTON_CONFIG_FILE_SIZE];
        struct {
            bool transmit_mask_0;
            bool transmit_mask_1;
            bool button_control_menu;
            bool enabled;
        } __attribute__((__packed__));
    };
} button_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void userbutton_callback(uint8_t button_id, uint8_t mask, buttons_state_t buttons_state);

static button_config_file_t button_config_file_cached = (button_config_file_t) {
    .transmit_mask_0 = true, .transmit_mask_1 = true, .button_control_menu = true, .enabled = true
};
static ubutton_callback_t low_level_event_cb;
static bool button_file_transmit_state = false;
static bool button_config_file_transmit_state = false;
static bool test_mode_state = false;

static void userbutton_callback(uint8_t button_id1, uint8_t mask1, buttons_state_t buttons_state1)
{
    // button was pressed, let upper layer know and write data to file
    if (low_level_event_cb)
        low_level_event_cb(button_id1, mask1, buttons_state1);
    button_file_t button_file = { .button_id = button_id1, .mask = mask1, .buttons_state = buttons_state1 };
    // this write file will trigger a file_modified_callback
    error_t ret = d7ap_fs_write_file(BUTTON_FILE_ID, 0, button_file.bytes, BUTTON_FILE_SIZE, ROOT_AUTH);
}

/**
 * @brief Initialize both the button and the button config file
 * The button file has all relevant data in it to let the world know the button was pressed, 
 * the config file is for internal use to configure settings
 * @return error_t
 */
error_t button_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = BUTTON_FILE_SIZE,
              .allocated_length = BUTTON_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = BUTTON_CONFIG_FILE_SIZE,
        .allocated_length = BUTTON_CONFIG_FILE_SIZE + 10 };

    uint32_t length = BUTTON_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(BUTTON_CONFIG_FILE_ID, 0, button_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(BUTTON_CONFIG_FILE_ID, &permanent_file_header, button_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing button configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading button configuration file: %d", ret);

    button_file_t button_file = {
        0,
    };

    ret = d7ap_fs_init_file(BUTTON_FILE_ID, &volatile_file_header, button_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing button file: %d", ret);
    }

    d7ap_fs_register_file_modified_callback(BUTTON_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(BUTTON_FILE_ID, &file_modified_callback);
    ubutton_register_callback(&userbutton_callback);
}

static void file_modified_callback(uint8_t file_id)
{

    if (file_id == BUTTON_CONFIG_FILE_ID) {
        // always send the button config file if any configurations have changed
        uint32_t size = BUTTON_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(BUTTON_CONFIG_FILE_ID, 0, button_config_file_cached.bytes, &size, ROOT_AUTH);
        if (button_config_file_transmit_state)
            queue_add_file(button_config_file_cached.bytes, BUTTON_CONFIG_FILE_SIZE, BUTTON_CONFIG_FILE_ID);
    } else if (file_id == BUTTON_FILE_ID) {
        // if a button gets pressed, this file will be written. 
        // we want to send this file if it's written if the configuration allows us
        button_file_t button_file;
        uint32_t size = BUTTON_FILE_SIZE;
        d7ap_fs_read_file(BUTTON_FILE_ID, 0, button_file.bytes, &size, ROOT_AUTH);
        if ((button_file.mask == true && button_config_file_cached.transmit_mask_1)
            || (button_file.mask == false && button_config_file_cached.transmit_mask_0))
            if (button_file_transmit_state && button_config_file_cached.enabled)
                queue_add_file(button_file.bytes, BUTTON_FILE_SIZE, BUTTON_FILE_ID);
    }
}

void button_file_transmit_config_file()
{
    uint32_t size = BUTTON_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(BUTTON_CONFIG_FILE_ID, 0, button_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(button_config_file_cached.bytes, BUTTON_CONFIG_FILE_SIZE, BUTTON_CONFIG_FILE_ID);
}

void button_file_register_cb(ubutton_callback_t callback) { low_level_event_cb = callback; }

/**
 * @brief Enable that this file is sent or not
 */
void button_file_set_measure_state(bool enable)
{
    button_file_transmit_state = enable;
    button_config_file_transmit_state = enable;
}

/**
 * @brief Enable testing mode which enables transmission on every change of data
 * @param enable 
 */
void button_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    if (enable) {
        button_config_file_cached.transmit_mask_0 = true;
        button_config_file_cached.transmit_mask_1 = true;
        button_config_file_cached.enabled = true;
    } else {
        uint32_t size = BUTTON_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(BUTTON_CONFIG_FILE_ID, 0, button_config_file_cached.bytes, &size, ROOT_AUTH);
    }
}

bool button_file_is_enabled() { return button_config_file_cached.enabled; }

void button_file_set_enabled(bool enable)
{
    if (button_config_file_cached.enabled != enable) {
        button_config_file_cached.enabled = enable;
        d7ap_fs_write_file(
            BUTTON_CONFIG_FILE_ID, 0, button_config_file_cached.bytes, BUTTON_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}
