#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
void init_switch_mirror(PIO pio, uint sm);
void set_switch_mirror(PIO pio, uint sm, bool enable);
void jjs_init(void);
void shutdown_switch_mirror(PIO pio, uint sm);

#ifdef __cplusplus
}
#endif
