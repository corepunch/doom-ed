#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "map.h"

// Initialize the console system
void init_console(void);

// Print a message to the console
void conprintf(const char* format, ...);

// Draw the console overlay
void draw_console(void);

// Clean up console resources
void shutdown_console(void);

// Toggle console visibility
void toggle_console(void);

#endif // __CONSOLE_H__
