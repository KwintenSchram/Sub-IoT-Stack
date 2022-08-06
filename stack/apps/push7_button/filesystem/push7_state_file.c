#include "push7_state_file.h"
#include "adc_stuff.h"
#include "d7ap_fs.h"
#include "errors.h"
#include "little_queue.h"
#include "log.h"
#include "stdint.h"
#include "timer.h"

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#else
#define DPRINT(...)
#endif

#define PUSH7_STATE_FILE_ID 56
#define PUSH7_STATE_FILE_SIZE sizeof(push7_state_file_t)
#define RAW_PUSH7_STATE_FILE_SIZE 4

#define PUSH7_STATE_CONFIG_FILE_ID 66
#define PUSH7_STATE_CONFIG_FILE_SIZE sizeof(push7_state_config_file_t)
#define RAW_PUSH7_STATE_CONFIG_FILE_SIZE 5

#define TESTMODE_STATE_INTERVAL_SEC 30

typedef struct {
    union {
        uint8_t bytes[RAW_PUSH7_STATE_FILE_SIZE];
        struct {
            uint16_t battery_voltage;
            uint8_t hw_version;
            uint8_t sw_version;
        } __attribute__((__packed__));
    };
} push7_state_file_t;

typedef struct {
    union {
        uint8_t bytes[RAW_PUSH7_STATE_CONFIG_FILE_SIZE];
        struct {
            uint32_t interval;
            bool enabled;
        } __attribute__((__packed__));
    };
} push7_state_config_file_t;

static void file_modified_callback(uint8_t file_id);
static void execute_measurement();

static push7_state_config_file_t push7_state_config_file_cached
    = (push7_state_config_file_t) { .interval = 5 * 60, .enabled = true };

static bool push7_state_file_transmit_state = false;
static bool push7_state_config_file_transmit_state = false;
static bool test_mode_state = false;

error_t push7_state_files_initialize()
{
    d7ap_fs_file_header_t volatile_file_header
        = { .file_permissions = (file_permission_t) { .guest_read = true, .user_read = true },
              .file_properties.storage_class = FS_STORAGE_VOLATILE,
              .length = PUSH7_STATE_FILE_SIZE,
              .allocated_length = PUSH7_STATE_FILE_SIZE };

    d7ap_fs_file_header_t permanent_file_header = { .file_permissions
        = (file_permission_t) { .guest_read = true, .guest_write = true, .user_read = true, .user_write = true },
        .file_properties.storage_class = FS_STORAGE_PERMANENT,
        .length = PUSH7_STATE_CONFIG_FILE_SIZE,
        .allocated_length = PUSH7_STATE_CONFIG_FILE_SIZE + 10 };

    push7_state_config_file_t push7_state_config_file;
    uint32_t length = PUSH7_STATE_CONFIG_FILE_SIZE;
    error_t ret = d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file.bytes, &length, ROOT_AUTH);
    if (ret == -ENOENT) {
        ret = d7ap_fs_init_file(
            PUSH7_STATE_CONFIG_FILE_ID, &permanent_file_header, push7_state_config_file_cached.bytes);
        if (ret != SUCCESS) {
            log_print_error_string("Error initializing push7_state effect configuration file: %d", ret);
            return ret;
        }
    } else if (ret != SUCCESS)
        log_print_error_string("Error reading push7_state effect configuration file: %d", ret);

    push7_state_file_t push7_state_file = {
        0,
    };

    ret = d7ap_fs_init_file(PUSH7_STATE_FILE_ID, &volatile_file_header, push7_state_file.bytes);
    if (ret != SUCCESS) {
        log_print_error_string("Error initializing push7_state effect file: %d", ret);
    }
    adc_stuff_init();
    d7ap_fs_register_file_modified_callback(PUSH7_STATE_CONFIG_FILE_ID, &file_modified_callback);
    d7ap_fs_register_file_modified_callback(PUSH7_STATE_FILE_ID, &file_modified_callback);
    sched_register_task(&execute_measurement);
}

static void file_modified_callback(uint8_t file_id)
{
    if (file_id == PUSH7_STATE_CONFIG_FILE_ID) {
        uint32_t size = PUSH7_STATE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &size, ROOT_AUTH);
        if (push7_state_config_file_cached.enabled && push7_state_file_transmit_state)
            timer_post_task_delay(&execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
        else
            timer_cancel_task(&execute_measurement);
        if (push7_state_config_file_transmit_state)
            queue_add_file(
                push7_state_config_file_cached.bytes, PUSH7_STATE_CONFIG_FILE_SIZE, PUSH7_STATE_CONFIG_FILE_ID);
    } else if (file_id == PUSH7_STATE_FILE_ID) {
        push7_state_file_t push7_state_file;
        uint32_t size = PUSH7_STATE_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_FILE_ID, 0, push7_state_file.bytes, &size, ROOT_AUTH);
        queue_add_file(push7_state_file.bytes, PUSH7_STATE_FILE_SIZE, PUSH7_STATE_FILE_ID);
        timer_post_task_delay(&execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

static void execute_measurement()
{
    update_battery_voltage();
    uint16_t voltage = get_battery_voltage();
    push7_state_file_t push7_state_file = { .hw_version = 0, .sw_version = 0, .battery_voltage = voltage };
    d7ap_fs_write_file(PUSH7_STATE_FILE_ID, 0, push7_state_file.bytes, PUSH7_STATE_FILE_SIZE, ROOT_AUTH);
}

void push7_state_file_set_measure_state(bool enable)
{
    timer_cancel_task(&execute_measurement);
    push7_state_file_transmit_state = enable;
    push7_state_config_file_transmit_state = enable;
    if (push7_state_config_file_cached.enabled && push7_state_file_transmit_state)
        timer_post_task_delay(&execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
}

void push7_state_file_set_test_mode(bool enable)
{
    if (test_mode_state == enable)
        return;
    test_mode_state == enable;
    timer_cancel_task(&execute_measurement);
    if (enable) {
        push7_state_config_file_cached.interval = TESTMODE_STATE_INTERVAL_SEC;
        push7_state_config_file_cached.enabled = true;
        timer_post_task_delay(&execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    } else {
        uint32_t size = PUSH7_STATE_CONFIG_FILE_SIZE;
        d7ap_fs_read_file(PUSH7_STATE_CONFIG_FILE_ID, 0, push7_state_config_file_cached.bytes, &size, ROOT_AUTH);
        if (push7_state_config_file_cached.enabled && push7_state_config_file_transmit_state)
            timer_post_task_delay(&execute_measurement, push7_state_config_file_cached.interval * TIMER_TICKS_PER_SEC);
    }
}

bool push7_state_file_is_enabled() { return push7_state_config_file_cached.enabled; }

void push7_state_file_set_enabled(bool enable)
{
    if (push7_state_config_file_cached.enabled != enable) {
        push7_state_config_file_cached.enabled = enable;
        d7ap_fs_write_file(PUSH7_STATE_CONFIG_FILE_SIZE, 0, push7_state_config_file_cached.bytes,
            PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}

void push7_state_file_set_interval(uint32_t interval)
{
    if (push7_state_config_file_cached.interval != interval) {
        push7_state_config_file_cached.interval = interval;
        d7ap_fs_write_file(PUSH7_STATE_CONFIG_FILE_SIZE, 0, push7_state_config_file_cached.bytes,
            PUSH7_STATE_CONFIG_FILE_SIZE, ROOT_AUTH);
    }
}