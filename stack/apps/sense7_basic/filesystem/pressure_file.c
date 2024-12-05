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
#include "pressure_file.h"
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

#define PRESSURE_FILE_ID 57
#define PRESSURE_FILE_SIZE sizeof(pressure_file_t)
#define RAW_PRESSURE_FILE_SIZE 8

#define PRESSURE_CONFIG_FILE_ID 67
#define PRESSURE_CONFIG_FILE_SIZE sizeof(pressure_config_file_t)
#define RAW_PRESSURE_CONFIG_FILE_SIZE 16

#define TESTMODE_PRESSURE_INTERVAL_SEC 30
#define DEFAULT_PRESSURE_INTERVAL_SEC 60 * 5

typedef struct {
    union {
        uint8_t bytes[RAW_PRESSURE_FILE_SIZE];
        struct {
            uint32_t pressure_level;
            uint16_t pressure_level_raw;
            bool threshold_high_triggered;
            bool threshold_low_triggered;
        } __attribute__((__packed__));
    };
} pressure_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_PRESSURE_CONFIG_FILE_SIZE];
        struct {
            uint32_t interval;
            uint16_t threshold_high;
            uint16_t threshold_low;
            bool pressure_detection_mode;
            uint8_t interrupt_check_interval;
            bool enabled;
        } __attribute__((__packed__));
    };
} pressure_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void check_interrupt_state();

static pressure_config_file_t pressure_config_file_cached = (pressure_config_file_t) { .interval = DEFAULT_PRESSURE_INTERVAL_SEC,
    .integration_time = ALS_INTEGRATION_100ms,
    .threshold_high = 4000,
    .threshold_low = 100,
    .pressure_detection_mode = false,
    .interrupt_check_interval = 2,
    .enabled = true };

static bool pressure_file_transmit_state = false;
static bool pressure_config_file_transmit_state = false;
static bool test_mode_state = false;
static bool prev_high_trigger_state = false;
static bool prev_low_trigger_state = false;

error_t pressure_files_initialize()
{
    sched_register_task(&pressure_file_execute_measurement);
    sched_register_task(&check_interrupt_state);
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = PRESSURE_FILE_SIZE,
              .allocated_length = PRESSURE_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = PRESSURE_CONFIG_FILE_SIZE,
        .allocated_length = PRESSURE_CONFIG_FILE_SIZE + 10 };

    uint32_t length = PRESSURE_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(PRESSURE_CONFIG_FILE_ID, &permanent_file_header, pressure_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing pressure effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading pressure effect configuration file: %d", ret);

    pressure_file_t pressure_file = {
        0,
    };

    ret = d7ap_fs_init_file(PRESSURE_FILE_ID, &volatile_file_header, pressure_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing pressure effect file: %d", ret);
    }

    // register callbacks for any changes in the pressure (config) file
    d7ap_fs_register_file_modified_callback(PRESSURE_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(PRESSURE_FILE_ID, &file_modified_callback);

    // initialize hardware TODO
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == PRESSURE_CONFIG_FILE_ID) {
        // pressure config file got modified, apply all configurations and transmit
        uint32_t size = PRESSURE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, &size, ROOT_AUTH);
         // adjust settings on hardware TODO

        if (pressure_config_file_cached.enabled && pressure_config_file_transmit_state)
            timer_post_task_delay(
                &pressure_file_execute_measurement, pressure_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        else
            timer_cancel_task(&pressure_file_execute_measurement);

        if (pressure_config_file_cached.enabled && pressure_config_file_cached.pressure_detection_mode
            && pressure_config_file_transmit_state) {
            timer_post_task_delay(
                &check_interrupt_state, pressure_config_file_cached.interrupt_check_interval * TIMER_TICKS_PER_SEC);
        } else {
            timer_cancel_task(&check_interrupt_state);
        }

        if (pressure_config_file_transmit_state)
            queue_add_file(pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, PRESSURE_CONFIG_FILE_ID);
    } else if (file_id == PRESSURE_FILE_ID) {
        // pressure file got modified (internally), transmit changes
        pressure_file_t pressure_file;
        uint32_t size = PRESSURE_FILE_SIZE;
        d7ap_fs_read_file(PRESSURE_FILE_ID, 0, pressure_file.bytes, &size, ROOT_AUTH);
        queue_add_file(pressure_file.bytes, PRESSURE_FILE_SIZE, PRESSURE_FILE_ID);
        timer_post_task_delay(&pressure_file_execute_measurement, pressure_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

/**
 * @brief process pressure interrupt
 */
static void check_interrupt_state() 
{
    bool high_triggered, low_triggered;
    float parsed_pressure_als = 0;
    uint16_t raw_data = 0;

    //measure pressure TODO

    // high_triggered = (raw_data > pressure_config_file_cached.threshold_high);
    // low_triggered = (raw_data < pressure_config_file_cached.threshold_low);

    // if ((high_triggered != prev_high_trigger_state) || (low_triggered != prev_low_trigger_state)) {
    //     pressure_file_t pressure_file = { .pressure_level = (uint32_t)round(parsed_pressure_als * 1000),
    //         .pressure_level_raw = raw_data,
    //         .threshold_high_triggered = high_triggered,
    //         .threshold_low_triggered = low_triggered };
    //     d7ap_fs_write_file(PRESSURE_FILE_ID, 0, pressure_file.bytes, PRESSURE_FILE_SIZE, ROOT_AUTH);
    //     DPRINT("interrupt triggered high %d, low %d, high thresh %d, low thresh %d, actual value %d", high_triggered,
    //         low_triggered, pressure_config_file_cached.threshold_high, pressure_config_file_cached.threshold_low, raw_data);

    //     prev_low_trigger_state = low_triggered;
    //     prev_high_trigger_state = high_triggered;
    // }

    timer_post_task_delay(
        &check_interrupt_state, pressure_config_file_cached.interrupt_check_interval * TIMER_TICKS_PER_SEC);
}

void pressure_file_transmit_config_file()
{
    uint32_t size = PRESSURE_CONFIG_FILE_SIZE;
    d7ap_fs_read_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, &size, ROOT_AUTH);
    queue_add_file(pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, PRESSURE_CONFIG_FILE_ID);
}

void pressure_file_execute_measurement()
{
    float parsed_pressure_als;
    uint16_t raw_data = 0;

    //measure pressure TODO

    pressure_file_t pressure_file = { .pressure_level = (uint32_t)round(parsed_pressure_als * 10),
        .pressure_level_raw = raw_data,
        .threshold_high_triggered = false,
        .threshold_low_triggered = false };
    
    d7ap_fs_write_file(PRESSURE_FILE_ID, 0, pressure_file.bytes, PRESSURE_FILE_SIZE, ROOT_AUTH);
}

void pressure_file_set_measure_state(bool enable)
{
    // enable or disable measurement of pressure
    timer_cancel_task(&pressure_file_execute_measurement);
    pressure_file_transmit_state = enable;
    pressure_config_file_transmit_state = enable;
    if (pressure_config_file_cached.enabled && pressure_config_file_transmit_state)
        timer_post_task_delay(&pressure_file_execute_measurement, pressure_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    if (pressure_config_file_cached.enabled && pressure_config_file_cached.pressure_detection_mode
        && pressure_config_file_transmit_state) {
       // enable interrupt TODO
    } else {
        // disable interrupt TODO
    }
}

void pressure_file_set_test_mode(bool enable)
{
    // send every 30 seconds if pressure mode is enabled
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&pressure_file_execute_measurement);
    if (enable) {
        pressure_config_file_cached.interval = TESTMODE_PRESSURE_INTERVAL_SEC;
        pressure_config_file_cached.enabled = true;
        timer_post_task_delay(&pressure_file_execute_measurement, pressure_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = PRESSURE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, &size, ROOT_AUTH);
        if (pressure_config_file_cached.enabled && pressure_config_file_transmit_state) {
            timer_post_task_delay(
                &pressure_file_execute_measurement, pressure_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        }
    }
}

bool pressure_file_is_enabled() { return pressure_config_file_cached.enabled; }

void pressure_file_set_enabled(bool enable)
{
    if (pressure_config_file_cached.enabled != enable) {
        pressure_config_file_cached.enabled = enable;
        d7ap_fs_write_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void pressure_file_set_interval(uint32_t interval)
{
    if (pressure_config_file_cached.interval != interval) {
        pressure_config_file_cached.interval = interval;
        d7ap_fs_write_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void pressure_file_set_pressure_detection_mode(bool state)
{
    if (pressure_config_file_cached.pressure_detection_mode != state) {
        pressure_config_file_cached.pressure_detection_mode = state;
        d7ap_fs_write_file(PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

bool pressure_file_get_pressure_detection_mode() { return pressure_config_file_cached.pressure_detection_mode; }

/**
 * @brief Use the current pressure level as threshold for interrupts
 * @param high_threshold indicate if the threshold you want to configure is the high or the low one
 */
void pressure_file_set_current_pressure_as_threshold(bool high_threshold)
{
    float parsed_pressure_als;
    uint16_t raw_data = 0;
    
    //get pressure and set as threshold with offsets

    if (high_threshold) {
        raw_data = raw_data - pressure_config_file_cached.threshold_menu_offset;
        if (pressure_config_file_cached.threshold_high != raw_data) {
            pressure_config_file_cached.threshold_high = raw_data;
            d7ap_fs_write_file(
                PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, ROOT_AUTH);
        }

    } else {
        raw_data = raw_data + pressure_config_file_cached.threshold_menu_offset;
        if (pressure_config_file_cached.threshold_low != raw_data) {
            pressure_config_file_cached.threshold_low = raw_data;
            d7ap_fs_write_file(
                PRESSURE_CONFIG_FILE_ID, 0, pressure_config_file_cached.bytes, PRESSURE_CONFIG_FILE_SIZE, ROOT_AUTH);
        }
    }
}