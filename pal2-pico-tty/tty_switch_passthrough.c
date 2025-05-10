#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "tty_switch_passthrough.h"
#include "tty_switch_passthrough.pio.h"
#include "stdio.h"
#include "stdlib.h"

#include "proj_hw.h"
#include "debug.h"

void enable_switch_mirror(PIO pio, uint sm)
{
    uint offset = pio_add_program(pio, &tty_switch_passthrough_program);
    pio_sm_config c = tty_switch_passthrough_program_get_default_config(offset);

    // Init GPIOs for PIO
    pio_gpio_init(pio, TTY_SWITCH1_INPUT);
    pio_gpio_init(pio, TTY_SWITCH2_OUTPUT);

#define input_gpio TTY_SWITCH1_INPUT
#define output_gpio TTY_SWITCH2_OUTPUT

    // Start with output in Hi-Z (input mode)
    pio_sm_set_consecutive_pindirs(pio, sm, output_gpio, 1, false);

    // Map relative pin indices in the PIO program
    sm_config_set_in_pins(&c, input_gpio);      // sets pin base for wait
    sm_config_set_set_pins(&c, output_gpio, 1); // sets pin base for set
    sm_config_set_out_pins(&c, output_gpio, 1); // optional, for completeness
    sm_config_set_clkdiv(&c, 1.0f);

    // Initialize and start
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void switch_passthrough_init()
{
    gpio_init(PAL_RESET_GPIO);
    gpio_init(TTY_SWITCH1_INPUT);
    gpio_init(TTY_SWITCH2_OUTPUT);

    gpio_set_dir(TTY_SWITCH1_INPUT, GPIO_IN);
    gpio_set_dir(TTY_SWITCH2_OUTPUT, GPIO_IN);
    gpio_put(TTY_SWITCH2_OUTPUT, 0);

    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm < 0)
    {
        // Should not happen if 'required' is true, but safe check
        panic("No available state machines!");
    }

    enable_switch_mirror(pio, (uint)sm);
    debug_printf("State machine init\n");

    // sleep_ms(10 * 1000);
    disable_switch_mirror(pio, sm);
}

void disable_switch_mirror(PIO pio, uint sm)
{
    // Disable the state machine
    pio_sm_set_enabled(pio, sm, false);

    // Clear instruction memory (optional but clean)
    uint offset = pio_sm_get_pc(pio, sm);
    for (int i = 0; i < 32; ++i)
    {
        pio->instr_mem[i] = pio_encode_nop();
    }

    // Clear FIFOs
    pio_sm_clear_fifos(pio, sm);

    // Unclaim the state machine
    pio_sm_unclaim(pio, sm);

    gpio_deinit(TTY_SWITCH1_INPUT);
    gpio_deinit(TTY_SWITCH2_OUTPUT);
}