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
#define RAW_LIGHT_CONFIG_FILE_SIZE 9

#define TESTMODE_LIGHT_INTERVAL_SEC 30
#define DEFAULT_LIGHT_INTERVAL_SEC 60 * 5

typedef struct {
    union {
        uint8_t bytes[RAW_LIGHT_FILE_SIZE];
        struct {
            uint32_t light_als;
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
            bool enabled;
        } __attribute__((__packed__));
    };
} light_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void execute_measurement();

static const light_config_file_t light_config_file_default
    = (light_config_file_t) { .interval = DEFAULT_LIGHT_INTERVAL_SEC,
          .integration_time = ALS_INTEGRATION_100ms,
          .persistence_protect_number = ALS_PERSISTENCE_1,
          .gain = ALS_GAIN_x1,
          .enabled = true };

static light_config_file_t light_config_file_cached = (light_config_file_t) { .interval = DEFAULT_LIGHT_INTERVAL_SEC,
    .integration_time = ALS_INTEGRATION_100ms,
    .persistence_protect_number = ALS_PERSISTENCE_1,
    .gain = ALS_GAIN_x1,
    .enabled = true };

static bool light_file_transmit_state = false;
static bool light_config_file_transmit_state = false;
static bool test_mode_state = false;

error_t light_files_initialize()
{
    sched_register_task(&execute_measurement);
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = LIGHT_FILE_SIZE,
              .allocated_length = LIGHT_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = LIGHT_CONFIG_FILE_SIZE,
        .allocated_length = LIGHT_CONFIG_FILE_SIZE };

    light_config_file_t light_config_file;
    uint32_t length = LIGHT_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(LIGHT_CONFIG_FILE_ID, &permanent_file_header, light_config_file_default.bytes);
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

    d7ap_fs_register_file_modified_callback(LIGHT_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(LIGHT_FILE_ID, &file_modified_callback);
    d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &length, ROOT_AUTH);
    VEML7700_init(platf_get_i2c_handle());
    VEML7700_change_settings(light_config_file_cached.integration_time,
        light_config_file_cached.persistence_protect_number, light_config_file_cached.gain);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == LIGHT_CONFIG_FILE_ID) {
        uint32_t size = LIGHT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &size, ROOT_AUTH);
        VEML7700_change_settings(light_config_file_cached.integration_time,
            light_config_file_cached.persistence_protect_number, light_config_file_cached.gain);
        timer_cancel_task(&execute_measurement);
        if (light_config_file_cached.enabled && light_config_file_transmit_state)
            timer_post_task_delay(&execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);

        if (light_config_file_transmit_state)
            queue_add_file(light_config_file_cached.bytes, LIGHT_CONFIG_FILE_SIZE, LIGHT_CONFIG_FILE_ID);
    } else if (file_id == LIGHT_FILE_ID) {
        light_file_t light_file;
        uint32_t size = LIGHT_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_FILE_ID, 0, light_file.bytes, &size, ROOT_AUTH);
        queue_add_file(light_file.bytes, LIGHT_FILE_SIZE, LIGHT_FILE_ID);
        timer_post_task_delay(&execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

static void execute_measurement()
{
    float parsed_light_als;
    uint16_t raw_data = 0;
    VEML7700_set_shutdown_state(false);
    VEML7700_read_ALS_Lux(&raw_data, &parsed_light_als);
    light_file_t light_file = { .light_als = (uint32_t)round(parsed_light_als) };
    VEML7700_set_shutdown_state(true);
    d7ap_fs_write_file(LIGHT_FILE_ID, 0, light_file.bytes, LIGHT_FILE_SIZE, ROOT_AUTH);
}

void light_file_set_measure_state(bool enable)
{
    timer_cancel_task(&execute_measurement);
    light_file_transmit_state = enable;
    light_config_file_transmit_state = enable;
    if (light_config_file_cached.enabled && light_config_file_transmit_state)
        timer_post_task_delay(&execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
}

void light_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&execute_measurement);
    if (enable) {
        light_config_file_cached.interval = TESTMODE_LIGHT_INTERVAL_SEC;
        light_config_file_cached.enabled = true;
        timer_post_task_delay(&execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = LIGHT_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(LIGHT_CONFIG_FILE_ID, 0, light_config_file_cached.bytes, &size, ROOT_AUTH);
        if (light_config_file_cached.enabled && light_config_file_transmit_state) {
            timer_post_task_delay(&execute_measurement, light_config_file_cached.interval * TIMER_TICKS_PER_SEC);
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