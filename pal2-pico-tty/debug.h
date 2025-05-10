#pragma once

#include <stdio.h>
//#include "config.h"

#define ENABLE_DEBUG false


#if ENABLE_DEBUG
    #define debug_printf(fmt, ...) \
        do { fprintf(stderr, "[DEBUG] " fmt, ##__VA_ARGS__); } while (0)
#else
    #define debug_printf(fmt, ...) \
        do { } while (0)
#endif

