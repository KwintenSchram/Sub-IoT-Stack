#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H

#include "button.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    HUMIDITY_SENSOR_INDEX = BUTTON1_PRESSED,
    LIGHT_SENSOR_INDEX = BUTTON2_PRESSED,
    PIR_SENSOR_INDEX = BUTTON3_PRESSED,
    HALL_EFFECT_SENSOR_INDEX = BUTTON1_2_PRESSED,
    BUTTON_SENSOR_INDEX = BUTTON1_3_PRESSED
} SENSOR_ARRAY_INDEXES;

void sensor_manager_init();
void sensor_manager_set_transmit_state(bool state);
void sensor_manager_set_test_mode(bool enable);
void sensor_manager_set_sensor_states(uint8_t sensor_enabled_state_array[]);
void sensor_manager_get_sensor_states(uint8_t sensor_enabled_state_array[]);

#endif //__NETWORK_MANAGER_H