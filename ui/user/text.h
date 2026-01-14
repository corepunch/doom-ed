#ifndef __UI_TEXT_H__
#define __UI_TEXT_H__

#include <stdint.h>
#include <stdbool.h>

// Initialize the text rendering system
void init_text_rendering(void);

// Clean up text rendering resources
void shutdown_text_rendering(void);

// Small bitmap font rendering (6x8 font)
void draw_text_small(const char* text, int x, int y, uint32_t col);
int strwidth(const char* text);
int strnwidth(const char* text, int text_length);

// DOOM/Hexen font rendering (game fonts from WAD files)
void draw_text_gl3(const char* text, int x, int y, float alpha);
int get_text_width(const char* text);
bool load_console_font(void);

#endif // __UI_TEXT_H__
