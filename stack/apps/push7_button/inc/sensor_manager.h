#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "button.h"

void sensor_manager_init();
void sensor_manager_button_pressed(button_id_t button_id, uint8_t mask, buttons_state_t buttons_state);
void sensor_manager_set_state(bool state);

#endif //__NETWORK_MANAGER_H