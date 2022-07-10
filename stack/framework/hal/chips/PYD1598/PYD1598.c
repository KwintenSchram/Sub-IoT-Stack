#include "PYD1598.h"

#include "errors.h"
#include "hwgpio.h"
#include "hwsystem.h"
#include "log.h"
#include "stm32_common_gpio.h"
#include "timer.h"
#include <stdio.h>

#ifdef true
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static void interrupt_callback(void* arg);
static void write_register_value(unsigned long regval);
static void PYD1598_setup_interrupt_mode();
static void process_interrupt();
static void reset_direct_link();


typedef union {
    uint32_t rawData;
    struct {
        uint32_t Factory_Params : 5; // Must be set to 0x10
        uint32_t Filter_Source : 2; //
        uint32_t Operation_Mode : 2;
        uint32_t Window_Time : 2; // Window time = [RegisterValue] * 2s + 2s
        uint32_t Pulse_Counter : 2; // Amount of pulses = [RegisterValue] + 1
        uint32_t Blind_Time : 4; //[RegisterValue] *0.5s + 0.5s
        uint32_t Threshold : 8; //
        uint32_t reserved1 : 7;
    };
} PYD1598_CONFIG_REG;

typedef enum {
    FORCED_READOUT = 0x00,
    INTERRUPT_READOUT = 0x01,
    WAKE_UP_OPERATION = 0x02, // only this one is support
    RESERVED_MODE = 0x03,
} PYD1598_OPERATIONAL_MODE_t;

typedef enum { PIR_BPF = 0, PIR_LPF = 1, reserved_source = 2, Temperature_Sensor = 3 } PYD1598_FILTER_SOURCE_t;

PYD1598_CONFIG_REG default_config =  {
    .Factory_Params = 0X10,
    .Filter_Source = PIR_BPF,
    .Operation_Mode = WAKE_UP_OPERATION,
    .Window_Time = 1, // 4s
    .Pulse_Counter = 1, //2 pulses
    .Blind_Time = 3, // 2s
    .Threshold = 0X18,
    .reserved1 = 0X00,
};

GPIO_InitTypeDef output_config = { .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_HIGH };
GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW };

static pin_id_t power_supply_pin;
static pin_id_t direct_link;
static pin_id_t serial_in;
static bool current_state = false;
PYD1598_callback_t PYD1598_cb;

/*!
 * @brief Sets up the default parameters of the sensor.
 */
error_t PYD1598_init(pin_id_t data_in, pin_id_t data_out, pin_id_t supply)
{
    power_supply_pin = supply;
    direct_link = data_out;
    serial_in = data_in;
    sched_register_task(&process_interrupt);

    hw_gpio_configure_pin_stm(power_supply_pin, &output_config);
    hw_gpio_configure_pin_stm(serial_in, &output_config);

    hw_gpio_clr(power_supply_pin);

    current_state = false;
}

void PYD1598_register_callback(PYD1598_callback_t PYD1598_callback) { PYD1598_cb = PYD1598_callback; }

static void PYD1598_setup_interrupt_mode()
{
    write_register_value(default_config.rawData);
    reset_direct_link();
}

error_t PYD1598_set_state(bool state)
{
    if (state && !current_state) {
        hw_gpio_set(power_supply_pin);
        hw_busy_wait(1000);
        PYD1598_setup_interrupt_mode();
    } else if (!state && current_state) {
        hw_gpio_disable_interrupt(serial_in);
        hw_gpio_clr(power_supply_pin);
    }
    return SUCCESS;
}

static void write_register_value(unsigned long regval)
{
    hw_gpio_clr(serial_in);

    int i;
    unsigned char nextbit;
    unsigned long regmask = 0x1000000;

    for (i = 0; i < 25; i++) {
        nextbit = (regval & regmask) != 0;
        regmask >>= 1;

        hw_gpio_clr(serial_in);
        hw_busy_wait(1);
        hw_gpio_set(serial_in);
        hw_busy_wait(1);
        if (!nextbit)
            hw_gpio_clr(serial_in);

        hw_busy_wait(100);
    }
    hw_gpio_clr(serial_in);
    hw_busy_wait(600);
}

static void reset_direct_link()
{
    hw_gpio_disable_interrupt(direct_link);
    hw_gpio_configure_pin_stm(direct_link, &output_config);
    hw_gpio_clr(direct_link);
    hw_busy_wait(500);
    hw_gpio_configure_pin_stm(direct_link, &input_config);
    hw_gpio_configure_interrupt(direct_link, GPIO_RISING_EDGE, &interrupt_callback, NULL);
    hw_gpio_enable_interrupt(direct_link);
}

static void process_interrupt()
{
    log_print_string("processing interrupt");
    if (hw_gpio_get_in(direct_link)) {

        reset_direct_link();

        DPRINT("PYD1598 movement detected");
        if (PYD1598_cb)
            PYD1598_cb();
    }
}

static void interrupt_callback(void* arg) { sched_post_task(&process_interrupt); }