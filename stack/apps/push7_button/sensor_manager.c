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
#include "file_definitions.h"
#include "led.h"
#include "little_queue.h"

#include "HDC1080DM.h"
#include "PYD1598.h"
#include "VEML7700.h"
#include "hwgpio.h"
#include "math.h"
#include "platform.h"
#include "scheduler.h"
#include "sensor_manager.h"
#include "stm32_common_gpio.h"

#ifdef FRAMEWORK_NETWORK_MANAGER_LOG
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

bool prev_state = false;

void sensor_manager_init()
{
    // init all files
    // read config and set correct parameters
    // modified callback on config to set parameters in sensor driver and readout interval
    // schedule readouts
}

void sensor_manager_set_state(bool state)
{
    if (state && !prev_state) {
        // turn on all scheduled sensor measurements
    } else if (!state && prev_state) {
        // turn off all scheduled sensor measurements
    }
}


void pir_callback()
{
    // pir_file_t pir_file =
    // {
    //     .state = hw_gpio_get_in(PIR_PIN),
    //     .battery_voltage=get_battery_voltage(),
    // };
    // queue_add_file(pir_file.bytes, PIR_FILE_SIZE, PIR_FILE_ID);
    led_flash_white();
}

static void execute_measurements()
{
    float test;
    uint16_t raw;
    HDC1080DM_read_temperature(&test);
    HDC1080DM_read_humidity(&test);
    // VEML7700_set_shutdown_state(false);
    VEML7700_read_ALS_Lux(&raw, &test);
    PYD1598_register_callback(&pir_callback);
    PYD1598_set_state(true);
}

void sensor_manager_button_pressed(button_id_t button_id, uint8_t mask, buttons_state_t buttons_state)
{

    button_file_t button_file = {
        .button_id = button_id,
        .mask = mask,
        .elapsed_deciseconds = 0,
        .buttons_state = buttons_state,
        .battery_voltage = get_battery_voltage(),
    };

    queue_add_file(button_file.bytes, BUTTON_FILE_SIZE, BUTTON_FILE_ID);
    DPRINT("Button callback - id: %d, mask: %d, elapsed time: %d, all_button_state %d \n", button_id, mask, 0,
        buttons_state);
}