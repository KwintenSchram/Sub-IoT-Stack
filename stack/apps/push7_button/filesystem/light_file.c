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
#include "light_file.h"
#include "VEML7700.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "led.h"
#include "little_queue.h"
#include "log.h"
#include "math.h"
#include "platform.h"
#include "stdint.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define LIGHT_FILE_ID 57
#define LIGHT_FILE_SIZE sizeof(light_file_t)
#define RAW_LIGHT_FILE_SIZE 8

#define LIGHT_CONFIG_FILE_ID 67
#define LIGHT_CONFIG_FILE_SIZE sizeof(light_config_file_t)
#define RAW_LIGHT_CONFIG_FILE_SIZE 16

#define TESTMODE_LIGHT_INTERVAL_SEC 30
#define DEFAULT_LIGHT_INTERVAL_SEC 60 * 5

typedef struct {
    union {
        uint8_t bytes[RAW_LIGHT_FILE_SIZE];
        struct {
            uint32_t light_level;
            uint16_t light_level_raw;
            bool threshold_high_triggered;
            bool threshold_low_triggered;
        } __attribute__((__packed__));
    };
} light_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_LIGHT_CONFIG_FILE_SIZE];
        struct {
            uint32_t interval;
            uint8_t integration_time;
            uint8_t persistence_protect_number;
            uint8_t gain;
            uint16_t threshold_high;
            uint16_t threshold_low;
            bool light_detection_mode;
            VEML7700_ALS_POWER_MODE low_power_mode;
            uint8_t interrupt_check_interval;
            uint8_t threshold_menu_offset;
            bool enabled;
        } __attribute__((__packed__));
    };
} light_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void check_interrupt_state();

static light_config_file_t light_config_file_cached = (light_config_file_t) { .interval = DEFAULT_LIGHT_INTERVAL_SEC,
    .integration_time = ALS_INTEGRATION_100ms,
    .persistence_protect_number = ALS_PERSISTENCE_1,
    .gain = ALS_GAIN_x1,
    .threshold_high = 4000,
    .threshold_low = 100,
    .light_detection_mode = false,
    .interrupt_check_interval = 2,
    .low_power_mode = ALS_POWER_MODE_2,
    .threshold_menu_offset = 50,
    .enabled = true };

static bool light_file_transmit_state = false;
static bool light_config_file_transmit_state = false;
static bool test_mode_state = false;
static bool prev_high_trigger_state = false;
static bool prev_low_trigger_state = false;

error_t light_files_initialize()
{
    sched_register_task(&light_file_execute_measurement);
    sched_register_task(&check_interrupt_state);
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = LIGHT_FILE_SIZE,
              .allocated_length = LIGHT_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = LIGHT_CONFIG_FILE_SIZE,
        .allocated_length = LIGHT_CONFIG_FILE_SIZE + 10 };

    uint32_t length = LIGHT_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(LIGHT_CONFIG_FILE_ID, &permanent_file_header, light_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing light effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading light effect configuration file: %d", ret);

    light_file_t light_file = {
        0,
    };

    ret = d7ap_fs_init_file(LIGHT_FILE_ID, &volatile_file_header, light_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing light effect file: %d", ret);
    }

    // register callbacks for any changes in the light (config) file
    d7ap_fs_register_file_modified_callback(LIGHT_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(LIGHT_FILE_ID, &file_modified_callback);

    // initialize hardware
    VEML7700_init(platf_get_i2c_handle());
    VEML7700_change_settings(light_config_file_cached.integration_time,
        light_config_file_cached.persistence_protect_number, light_config_file_cached.gain,
        light_config_file_cached.light_detection_mode, light_config_file_cached.low_power_mode);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == LIGHT_CONFIG_FILE_ID) {
        // light config file got modified, apply all configurations and transmit
        uint32_t size = LIGHT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &size, ROOT_AUTH);
        VEML7700_change_settings(light_config_file_cached.integration_time,
            light_config_file_cached.persistence_protect_number, light_config_file_cached.gain,
            light_config_file_cached.light_detection_mode, light_config_file_cached.low_power_mode);
        if (light_config_file_cached.enabled && light_config_file_transmit_state)
            timer_post_task_delay(
                &light_file_execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        else
            timer_cancel_task(&light_file_execute_measurement);

        if (light_config_file_cached.enabled && light_config_file_cached.light_detection_mode
            && light_config_file_transmit_state) {
            VEML7700_set_shutdown_state(false);
            timer_post_task_delay(
                &check_interrupt_state, light_config_file_cached.interrupt_check_interval * TIMER_TICKS_PER_SEC);
        } else {
            VEML7700_set_shutdown_state(true);
            timer_cancel_task(&check_interrupt_state);
        }

        if (light_config_file_transmit_state)
            queue_add_file(light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, LIGHT_CONFIG_FILE_ID);
    } else if (file_id == LIGHT_FILE_ID) {
        // light file got modified (internally), transmit changes
        light_file_t light_file;
        uint32_t size = LIGHT_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_FILE_ID, 0, light_file.bytes, &size, ROOT_AUTH);
        queue_add_file(light_file.bytes, LIGHT_FILE_SIZE, LIGHT_FILE_ID);
        timer_post_task_delay(&light_file_execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

/**
 * @brief check if the light interval exceeded the high or low threshold
 * if it did, it will write to the light file which triggers the modified callback
 */
static void check_interrupt_state()
{
    bool high_triggered, low_triggered;
    float parsed_light_als = 0;
    uint16_t raw_data = 0;

    VEML7700_read_ALS_Lux(&raw_data, &parsed_light_als);

    high_triggered = (raw_data > light_config_file_cached.threshold_high);
    low_triggered = (raw_data < light_config_file_cached.threshold_low);

    if ((high_triggered == prev_high_trigger_state) || (low_triggered == prev_low_trigger_state)) {
        light_file_t light_file = { .light_level = (uint32_t)round(parsed_light_als * 1000),
            .light_level_raw = raw_data,
            .threshold_high_triggered = high_triggered,
            .threshold_low_triggered = low_triggered };
        d7ap_fs_write_file(LIGHT_FILE_ID, 0, light_file.bytes, LIGHT_FILE_SIZE, ROOT_AUTH);
        DPRINT("interrupt triggered high %d, low %d, high thresh %d, low thresh %d, actual value %d", high_triggered,
            low_triggered, light_config_file_cached.threshold_high, light_config_file_cached.threshold_low, raw_data);

        prev_low_trigger_state = low_triggered;
        prev_high_trigger_state = high_triggered;
    }

    timer_post_task_delay(
        &check_interrupt_state, light_config_file_cached.interrupt_check_interval * TIMER_TICKS_PER_SEC);
}

void light_file_transmit_config_file()
{
    uint32_t size = LIGHT_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, LIGHT_CONFIG_FILE_ID);
}

void light_file_execute_measurement()
{
    float parsed_light_als;
    uint16_t raw_data = 0;
    if (!light_config_file_cached.light_detection_mode)
        VEML7700_set_shutdown_state(false);
    VEML7700_read_ALS_Lux(&raw_data, &parsed_light_als);
    light_file_t light_file = { .light_level = (uint32_t)round(parsed_light_als * 10),
        .light_level_raw = raw_data,
        .threshold_high_triggered = false,
        .threshold_low_triggered = false };
    if (!light_config_file_cached.light_detection_mode)
        VEML7700_set_shutdown_state(true);
    d7ap_fs_write_file(LIGHT_FILE_ID, 0, light_file.bytes, LIGHT_FILE_SIZE, ROOT_AUTH);
}

void light_file_set_measure_state(bool enable)
{
    // enable or disable measurement of light
    timer_cancel_task(&light_file_execute_measurement);
    light_file_transmit_state = enable;
    light_config_file_transmit_state = enable;
    if (light_config_file_cached.enabled && light_config_file_transmit_state)
        timer_post_task_delay(&light_file_execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    if (light_config_file_cached.enabled && light_config_file_cached.light_detection_mode
        && light_config_file_transmit_state) {
        VEML7700_set_shutdown_state(false);
        timer_post_task_delay(
            &check_interrupt_state, light_config_file_cached.interrupt_check_interval * TIMER_TICKS_PER_SEC);
    } else {
        VEML7700_set_shutdown_state(true);
        timer_cancel_task(&check_interrupt_state);
    }
}

void light_file_set_test_mode(bool enable)
{
    // send every 30 seconds if light mode is enabled
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&light_file_execute_measurement);
    if (enable) {
        light_config_file_cached.interval = TESTMODE_LIGHT_INTERVAL_SEC;
        light_config_file_cached.enabled = true;
        timer_post_task_delay(&light_file_execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = LIGHT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &size, ROOT_AUTH);
        if (light_config_file_cached.enabled && light_config_file_transmit_state) {
            timer_post_task_delay(
                &light_file_execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        }
    }
}

bool light_file_is_enabled() { return light_config_file_cached.enabled; }

void light_file_set_enabled(bool enable)
{
    if (light_config_file_cached.enabled != enable) {
        light_config_file_cached.enabled = enable;
        d7ap_fs_write_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void light_file_set_interval(uint32_t interval)
{
    if (light_config_file_cached.interval != interval) {
        light_config_file_cached.interval = interval;
        d7ap_fs_write_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void light_file_set_light_detection_mode(bool state)
{
    if (light_config_file_cached.light_detection_mode != state) {
        light_config_file_cached.light_detection_mode = state;
        d7ap_fs_write_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

bool light_file_get_light_detection_mode() { return light_config_file_cached.light_detection_mode; }

/**
 * @brief Use the current light level as threshold for interrupts
 * @param high_threshold indicate if the threshold you want to configure is the high or the low one
 */
void light_file_set_current_light_as_threshold(bool high_threshold)
{
    float parsed_light_als;
    uint16_t raw_data = 0;

    VEML7700_change_settings(light_config_file_cached.integration_time,
        light_config_file_cached.persistence_protect_number, light_config_file_cached.gain, false,
        light_config_file_cached.low_power_mode);
    VEML7700_set_shutdown_state(false);
    VEML7700_read_ALS_Lux(&raw_data, &parsed_light_als);
    VEML7700_set_shutdown_state(true);
    VEML7700_change_settings(light_config_file_cached.integration_time,
        light_config_file_cached.persistence_protect_number, light_config_file_cached.gain,
        light_config_file_cached.light_detection_mode, light_config_file_cached.low_power_mode);

    if (high_threshold) {
        raw_data = raw_data - light_config_file_cached.threshold_menu_offset;
        if (light_config_file_cached.threshold_high != raw_data) {
            light_config_file_cached.threshold_high = raw_data;
            d7ap_fs_write_file(
                LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, ROOT_AUTH);
        }

    } else {
        raw_data = raw_data + light_config_file_cached.threshold_menu_offset;
        if (light_config_file_cached.threshold_low != raw_data) {
            light_config_file_cached.threshold_low = raw_data;
            d7ap_fs_write_file(
                LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, ROOT_AUTH);
        }
    }
}