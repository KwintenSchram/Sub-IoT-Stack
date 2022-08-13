#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hwleds.h"
#include "hwsystem.h"

#include "d7ap_fs.h"
#include "debug.h"
#include "log.h"
#include "scheduler.h"
#include "timer.h"

#include "alp_layer.h"
#include "d7ap.h"
#include "dae.h"

#include "platform.h"

#include "adc_stuff.h"
#include "button.h"
#include "led.h"
#include "little_queue.h"

#include "HDC1080DM.h"

#include "hwgpio.h"
#include "math.h"
#include "platform.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "stm32_common_gpio.h"

#include "button_file.h"
#include "hall_effect_file.h"
#include "humidity_file.h"
#include "light_file.h"
#include "little_queue.h"
#include "pir_file.h"
#include "push7_state_file.h"

#ifdef FRAMEWORK_SENSOR_MANAGER_LOG
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static bool current_transmit_state = false;
static bool current_testmode_state = false;

void sensor_manager_init()
{
    push7_state_files_initialize();
    pir_files_initialize();
    light_files_initialize();
    humidity_files_initialize();
    hall_effect_files_initialize();
    button_files_initialize();
}

void sensor_manager_set_transmit_state(bool state)
{
    if (state == current_transmit_state)
        return;

    humidity_file_set_measure_state(state);
    push7_state_file_set_measure_state(state);
    pir_file_set_measure_state(state);
    light_file_set_measure_state(state);
    hall_effect_file_set_measure_state(state);
    button_file_set_measure_state(state);

    current_transmit_state = state;
}

void sensor_manager_set_test_mode(bool enable)
{
    if (enable == current_testmode_state)
        return;

    DPRINT("setting test mode: %d", enable);
    humidity_file_set_test_mode(enable);
    push7_state_file_set_test_mode(enable);
    pir_file_set_test_mode(enable);
    light_file_set_test_mode(enable);
    hall_effect_file_set_test_mode(enable);
    button_file_set_test_mode(enable);
    current_testmode_state = enable;
}

void sensor_manager_set_sensor_states(uint8_t sensor_enabled_state_array[])
{
    DPRINT("setting enable states");
    DPRINT_DATA(sensor_enabled_state_array, 6);
    humidity_file_set_enabled(sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX]);
    light_file_set_enabled(sensor_enabled_state_array[LIGHT_SENSOR_INDEX]);
    pir_file_set_enabled(sensor_enabled_state_array[PIR_SENSOR_INDEX]);
    hall_effect_file_set_enabled(sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX]);
    button_file_set_enabled(sensor_enabled_state_array[BUTTON_SENSOR_INDEX]);
    push7_flash_set_led_enabled(sensor_enabled_state_array[QUEUE_LIGHT_STATE]);

    DPRINT("SET HUMIDITY %d, LIGHT %d, PIR %d, HALL_EFFECT %d, BUTTON %d, QUEUE LED %d",
        sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX], sensor_enabled_state_array[LIGHT_SENSOR_INDEX],
        sensor_enabled_state_array[PIR_SENSOR_INDEX], sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX],
        sensor_enabled_state_array[BUTTON_SENSOR_INDEX], sensor_enabled_state_array[QUEUE_LIGHT_STATE]);
}

void sensor_manager_set_interval(uint32_t interval)
{
    humidity_file_set_interval(interval);
    light_file_set_interval(interval);
    DPRINT("setting sensor interval %d", interval);
}

void sensor_manager_get_sensor_states(uint8_t sensor_enabled_state_array[])
{
    sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX] = humidity_file_is_enabled();
    sensor_enabled_state_array[LIGHT_SENSOR_INDEX] = light_file_is_enabled();
    sensor_enabled_state_array[PIR_SENSOR_INDEX] = pir_file_is_enabled();
    sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX] = hall_effect_file_is_enabled();
    sensor_enabled_state_array[BUTTON_SENSOR_INDEX] = button_file_is_enabled();
    sensor_enabled_state_array[QUEUE_LIGHT_STATE] = push7_flash_is_led_enabled();
    DPRINT("getting enable states");
    DPRINT_DATA(sensor_enabled_state_array, 6);
    DPRINT("GET HUMIDITY %d, LIGHT %d, PIR %d, HALL_EFFECT %d, BUTTON %d, QUEUE LED %d",
        sensor_enabled_state_array[HUMIDITY_SENSOR_INDEX], sensor_enabled_state_array[LIGHT_SENSOR_INDEX],
        sensor_enabled_state_array[PIR_SENSOR_INDEX], sensor_enabled_state_array[HALL_EFFECT_SENSOR_INDEX],
        sensor_enabled_state_array[BUTTON_SENSOR_INDEX], sensor_enabled_state_array[QUEUE_LIGHT_STATE]);
}

void sensor_manager_measure_sensor(uint8_t sensor)
{
    if (sensor == 0)
        humidity_file_execute_measurement();
    else if (sensor == 1)
        light_file_execute_measurement();
    else if (sensor == 2)
        push7_state_file_execute_measurement();
}

void sensor_manager_send_config_files()
{
    push7_state_file_execute_measurement();
    humidity_file_transmit_config_file();
    light_file_transmit_config_file();
    pir_file_transmit_config_file();
    hall_effect_file_transmit_config_file();
}