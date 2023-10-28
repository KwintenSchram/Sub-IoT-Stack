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
#include "water_detect_file.h"
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

#define WATER_DETECT_FILE_ID 72
#define WATER_DETECT_FILE_SIZE sizeof(water_detect_file_t)
#define RAW_WATER_DETECT_FILE_SIZE 1


#define WATER_INPUT_EVENT 5

typedef struct {
    union {
        uint8_t bytes[RAW_WATER_DETECT_FILE_SIZE];
        struct {
            bool mask;
        } __attribute__((__packed__));
    };
} water_detect_file_t;

static void file_modified_callback(uint8_t file_id);
static void water_detect_interrupt_callback(void* arg);
static void water_detect_sched_task();

static app_state_input_t app_state_input_cb;
static bool water_detect_file_transmit_state = false;
static bool water_detect_config_file_transmit_state = false;
static bool prev_state = false;

static GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_LOW }; // 
static GPIO_InitTypeDef output_config
    = { .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_HIGH };

error_t water_detect_files_initialize(app_state_input_t app_state_input)
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = WATER_DETECT_FILE_SIZE,
              .allocated_length = WATER_DETECT_FILE_SIZE };

    water_detect_file_t water_detect_file = {
        0,
    };

    app_state_input_cb = app_state_input;
    error_t ret = d7ap_fs_init_file(WATER_DETECT_FILE_ID, &volatile_file_header, water_detect_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    sched_register_task(&water_detect_sched_task);
    d7ap_fs_register_file_modified_callback(WATER_DETECT_FILE_ID, &file_modified_callback);

    hw_gpio_configure_pin_stm(WATER_OUTPUT_PIN, &output_config);

    hw_gpio_configure_pin_stm(WATER_DETECT_PIN, &input_config);
    hw_gpio_configure_interrupt(
        WATER_DETECT_PIN, GPIO_FALLING_EDGE | GPIO_RISING_EDGE, &water_detect_interrupt_callback, NULL);
    hw_gpio_enable_interrupt(WATER_DETECT_PIN);
    hw_gpio_set(WATER_OUTPUT_PIN);
}

static void water_detect_interrupt_callback(void* arg) { 
    sched_post_task(&water_detect_sched_task); }

static void water_detect_sched_task()
{
    water_detect_file_t water_detect_file = { .mask = hw_gpio_get_out(WATER_DETECT_PIN) };
    if (prev_state != water_detect_file.mask)
    {
        app_state_input_cb(WATER_INPUT_EVENT, water_detect_file.mask );
        d7ap_fs_write_file(WATER_DETECT_FILE_ID, 0, water_detect_file.bytes, WATER_DETECT_FILE_SIZE, ROOT_AUTH);
        prev_state = water_detect_file.mask;
    } 
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == WATER_DETECT_FILE_ID && water_detect_file_transmit_state) {
        water_detect_file_t water_detect_file;
        uint32_t size = WATER_DETECT_FILE_SIZE;
        d7ap_fs_read_file(WATER_DETECT_FILE_ID, 0, water_detect_file.bytes, &size, ROOT_AUTH);
        queue_add_file(water_detect_file.bytes, WATER_DETECT_FILE_SIZE, WATER_DETECT_FILE_ID);
    }
}


