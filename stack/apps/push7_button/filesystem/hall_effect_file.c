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
#include "hall_effect_file.h"
#include "VEML7700.h"
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

#define HALL_EFFECT_FILE_ID 59
#define HALL_EFFECT_FILE_SIZE sizeof(hall_effect_file_t)
#define RAW_HALL_EFFECT_FILE_SIZE 1

#define HALL_EFFECT_CONFIG_FILE_ID 69
#define HALL_EFFECT_CONFIG_FILE_SIZE sizeof(hall_effect_config_file_t)
#define RAW_HALL_EFFECT_CONFIG_FILE_SIZE 3

typedef struct {
    union {
        uint8_t bytes[RAW_HALL_EFFECT_FILE_SIZE];
        struct {
            bool mask;
        } __attribute__((__packed__));
    };
} hall_effect_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_HALL_EFFECT_CONFIG_FILE_SIZE];
        struct {
            bool transmit_mask_0;
            bool transmit_mask_1;
            bool enabled;
        } __attribute__((__packed__));
    };
} hall_effect_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void hall_effect_interrupt_callback(void* arg);
static void hall_effect_sched_task();

static hall_effect_config_file_t hall_effect_config_file_cached
    = (hall_effect_config_file_t) { .transmit_mask_0 = true, .transmit_mask_1 = true, .enabled = true };

static bool hall_effect_file_transmit_state = false;
static bool hall_effect_config_file_transmit_state = false;
static bool test_mode_state = false;

static GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW };

error_t hall_effect_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = HALL_EFFECT_FILE_SIZE,
              .allocated_length = HALL_EFFECT_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = HALL_EFFECT_CONFIG_FILE_SIZE,
        .allocated_length = HALL_EFFECT_CONFIG_FILE_SIZE + 10 };

    uint32_t length = HALL_EFFECT_CONFIG_FILE_SIZE;
    error_t ret
        = d7ap_fs_read_file(HALL_EFFECT_CONFIG_FILE_ID, 0, hall_effect_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(
            HALL_EFFECT_CONFIG_FILE_ID, &permanent_file_header, hall_effect_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing hall effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading hall effect configuration file: %d", ret);

    hall_effect_file_t hall_effect_file = {
        0,
    };

    ret = d7ap_fs_init_file(HALL_EFFECT_FILE_ID, &volatile_file_header, hall_effect_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing hall effect file: %d", ret);
    }

    sched_register_task(&hall_effect_sched_task);
    d7ap_fs_register_file_modified_callback(HALL_EFFECT_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(HALL_EFFECT_FILE_ID, &file_modified_callback);

    platf_set_HALL_power_state(true);

    hw_gpio_configure_pin_stm(HAL_EFFECT_PIN, &input_config);
    hw_gpio_configure_interrupt(
        HAL_EFFECT_PIN, GPIO_FALLING_EDGE | GPIO_RISING_EDGE, &hall_effect_interrupt_callback, NULL);
    hw_gpio_enable_interrupt(HAL_EFFECT_PIN);
}

static void hall_effect_interrupt_callback(void* arg) { sched_post_task(&hall_effect_sched_task); }
static void hall_effect_sched_task()
{
    hall_effect_file_t hall_effect_file = { .mask = hw_gpio_get_out(HAL_EFFECT_PIN) };
    d7ap_fs_write_file(HALL_EFFECT_FILE_ID, 0, hall_effect_file.bytes, HALL_EFFECT_FILE_SIZE, ROOT_AUTH);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == HALL_EFFECT_CONFIG_FILE_ID && hall_effect_config_file_transmit_state) {
        uint32_t size = HALL_EFFECT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(HALL_EFFECT_CONFIG_FILE_ID, 0, hall_effect_config_file_cached.bytes, &size, ROOT_AUTH);
        queue_add_file(hall_effect_config_file_cached.bytes, HALL_EFFECT_CONFIG_FILE_SIZE, HALL_EFFECT_CONFIG_FILE_ID);
    } else if (file_id == HALL_EFFECT_FILE_ID && hall_effect_file_transmit_state) {
        hall_effect_file_t hall_effect_file;
        uint32_t size = HALL_EFFECT_FILE_SIZE;
        d7ap_fs_read_file(HALL_EFFECT_FILE_ID, 0, hall_effect_file.bytes, &size, ROOT_AUTH);
        if ((hall_effect_file.mask == true && hall_effect_config_file_cached.transmit_mask_1)
            || (hall_effect_file.mask == false && hall_effect_config_file_cached.transmit_mask_0))
            if (hall_effect_config_file_cached.enabled)
                queue_add_file(hall_effect_file.bytes, HALL_EFFECT_FILE_SIZE, HALL_EFFECT_FILE_ID);
    }
}

void hall_effect_file_transmit_config_file()
{
    uint32_t size = HALL_EFFECT_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(HALL_EFFECT_CONFIG_FILE_ID, 0, hall_effect_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(hall_effect_config_file_cached.bytes, HALL_EFFECT_CONFIG_FILE_SIZE, HALL_EFFECT_CONFIG_FILE_ID);
}

void hall_effect_file_set_measure_state(bool enable)
{
    hall_effect_file_transmit_state = enable;
    hall_effect_config_file_transmit_state = enable;
}

void hall_effect_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    if (enable) {
        hall_effect_config_file_cached.transmit_mask_0 = true;
        hall_effect_config_file_cached.transmit_mask_1 = true;
        hall_effect_config_file_cached.enabled = true;
    } else {
        uint32_t size = HALL_EFFECT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(HALL_EFFECT_CONFIG_FILE_ID, 0, hall_effect_config_file_cached.bytes, &size, ROOT_AUTH);
    }
}

bool hall_effect_file_is_enabled() { return hall_effect_config_file_cached.enabled; }

void hall_effect_file_set_enabled(bool enable)
{
    if (hall_effect_config_file_cached.enabled != enable) {
        hall_effect_config_file_cached.enabled = enable;
        d7ap_fs_write_file(HALL_EFFECT_CONFIG_FILE_ID, 0, hall_effect_config_file_cached.bytes,
            HALL_EFFECT_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}
