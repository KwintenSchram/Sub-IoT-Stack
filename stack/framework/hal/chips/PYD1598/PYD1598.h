#ifndef __PYD1598__H
#define __PYD1598__H

#include <stdio.h>
#include "errors.h"
#include "hwgpio.h"

typedef void (*PYD1598_callback_t)();

error_t PYD1598_init(pin_id_t data_in, pin_id_t data_out, pin_id_t supply);
error_t PYD1598_set_state(bool state);
void PYD1598_register_callback(PYD1598_callback_t PYD1598_callback);



#endif