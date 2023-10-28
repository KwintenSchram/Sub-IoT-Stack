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
#include "charging_detect_file.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "platform.h"
#include "stdint.h"
#include "stm32_common_gpio.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define CHARGING_DETECT_FILE_ID 71
#define CHARGING_DETECT_FILE_SIZE sizeof(charging_detect_file_t)
#define RAW_CHARGING_DETECT_FILE_SIZE 1


#define CHARGING_INPUT_EVENT 6

typedef struct {
    union {
        uint8_t bytes[RAW_CHARGING_DETECT_FILE_SIZE];
        struct {
            bool mask;
        } __attribute__((__packed__));
    };
} charging_detect_file_t;

static void file_modified_callback(uint8_t file_id);
static void charging_detect_interrupt_callback(void* arg);
static void charging_detect_sched_task();

static app_state_input_t app_state_input_cb;
static bool charging_detect_file_transmit_state = false;
static bool charging_detect_config_file_transmit_state = false;
static bool prev_state = false;

static GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW }; // 

error_t charging_detect_files_initialize(app_state_input_t app_state_input)
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = CHARGING_DETECT_FILE_SIZE,
              .allocated_length = CHARGING_DETECT_FILE_SIZE };

    charging_detect_file_t charging_detect_file = {
        0,
    };
    app_state_input_cb = app_state_input;
    error_t ret = d7ap_fs_init_file(CHARGING_DETECT_FILE_ID, &volatile_file_header, charging_detect_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    sched_register_task(&charging_detect_sched_task);
    d7ap_fs_register_file_modified_callback(CHARGING_DETECT_FILE_ID, &file_modified_callback);


    hw_gpio_configure_pin_stm(CHARGING_STATE_PIN, &input_config);
    hw_gpio_configure_interrupt(
        CHARGING_STATE_PIN, GPIO_FALLING_EDGE | GPIO_RISING_EDGE, &charging_detect_interrupt_callback, NULL);
    hw_gpio_enable_interrupt(CHARGING_STATE_PIN);
}

static void charging_detect_interrupt_callback(void* arg) { 
    
    sched_post_task(&charging_detect_sched_task); }

static void charging_detect_sched_task()
{
    charging_detect_file_t charging_detect_file = { .mask = hw_gpio_get_out(CHARGING_STATE_PIN) };
    if (prev_state != charging_detect_file.mask)
    {
        app_state_input_cb(CHARGING_INPUT_EVENT,charging_detect_file.mask );
        d7ap_fs_write_file(CHARGING_DETECT_FILE_ID, 0, charging_detect_file.bytes, CHARGING_DETECT_FILE_SIZE, ROOT_AUTH);
        prev_state = charging_detect_file.mask;
    }
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == CHARGING_DETECT_FILE_ID && charging_detect_file_transmit_state) {
        charging_detect_file_t charging_detect_file;
        uint32_t size = CHARGING_DETECT_FILE_SIZE;
        d7ap_fs_read_file(CHARGING_DETECT_FILE_ID, 0, charging_detect_file.bytes, &size, ROOT_AUTH);
        queue_add_file(charging_detect_file.bytes, CHARGING_DETECT_FILE_SIZE, CHARGING_DETECT_FILE_ID);
    }
}
