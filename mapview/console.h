#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "map.h"

// Initialize the console system
void init_console(void);

// Load the DOOM font from the WAD file
bool load_console_font(void);

// Print a message to the console
void conprintf(const char* format, ...);

// Draw the console overlay
void draw_console(void);

// Clean up console resources
void shutdown_console(void);

// Toggle console visibility
void toggle_console(void);

// Set console text color
void set_console_color(uint8_t r, uint8_t g, uint8_t b);

int get_text_width(const char* text);

#endif // __CONSOLE_H__
