#ifndef __PYD1598__H
#define __PYD1598__H

#include "errors.h"
#include "hwgpio.h"
#include <stdio.h>

typedef void (*PYD1598_callback_t)(bool mask);

error_t PYD1598_init(pin_id_t data_in, pin_id_t data_out);
error_t PYD1598_set_state(bool state);
void PYD1598_register_callback(PYD1598_callback_t PYD1598_callback);
void PYD1598_set_settings(
    uint8_t filter_Source, uint8_t window_Time, uint8_t pulse_Counter, uint8_t blind_Time, uint8_t threshold);

#endif