#include "pico/stdlib.h"
#include "hardware/gpio.h"


#include "buttons.h"

void init_buttons(void)
{
    // Array of all button pins
    const uint8_t button_pins[] = {
        PIN_MENU,
        PIN_REWIND,
        PIN_PLAY,
        PIN_FASTFORWARD,
        PIN_RECORD};

    for (int i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); ++i)
    {
        uint pin = button_pins[i];
        gpio_init(pin);             // Initialize GPIO
        gpio_set_dir(pin, GPIO_IN); // Set as input
        gpio_pull_up(pin);          // Enable internal pull-up
    }
}


button_state_t read_buttons_struct(void) {
    button_state_t state;

    state.menu         = !gpio_get(PIN_MENU);
    state.rewind       = !gpio_get(PIN_REWIND);
    state.play         = !gpio_get(PIN_PLAY);
    state.fast_forward = !gpio_get(PIN_FASTFORWARD);
    state.record       = !gpio_get(PIN_RECORD);

    return state;
}