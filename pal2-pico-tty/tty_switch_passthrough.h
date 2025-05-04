#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
void enable_switch_mirror(PIO pio, uint sm);
void set_switch_mirror(PIO pio, uint sm, bool enable);
void switch_passthrough_init(void);
void disable_switch_mirror(PIO pio, uint sm);

#ifdef __cplusplus
}
#endif
