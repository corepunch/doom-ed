// Drawing primitives implementation for UI framework
// These are stub implementations that should be overridden by the application

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "gl_compat.h"
#include "draw.h"
#include "user.h"

// Weak symbols - can be overridden by application implementations
// This allows the UI framework to compile standalone while still
// using the full implementations when linked with the main application

#ifndef __APPLE__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif

// Drawing function stubs
WEAK_SYMBOL void fill_rect(int color, int x, int y, int w, int h) {
  // Stub implementation - override in application
  // Default: no-op
  (void)color; (void)x; (void)y; (void)w; (void)h;
}

WEAK_SYMBOL void draw_rect(int tex, int x, int y, int w, int h) {
  // Stub implementation - override in application
  (void)tex; (void)x; (void)y; (void)w; (void)h;
}

WEAK_SYMBOL void draw_rect_ex(int tex, int x, int y, int w, int h, int type, float alpha) {
  // Stub implementation - override in application
  (void)tex; (void)x; (void)y; (void)w; (void)h; (void)type; (void)alpha;
}

WEAK_SYMBOL void draw_text_small(const char* text, int x, int y, uint32_t col) {
  // Stub implementation - override in application
  (void)text; (void)x; (void)y; (void)col;
}

WEAK_SYMBOL int strwidth(const char* text) {
  // Stub implementation - override in application
  // Default: estimate based on character count
  return text ? (int)strlen(text) * 6 : 0;
}

WEAK_SYMBOL int strnwidth(const char* text, int text_length) {
  // Stub implementation - override in application
  // Default: estimate based on character count
  (void)text; (void)text_length;
  return text_length * 6;
}

WEAK_SYMBOL void draw_icon8(int icon, int x, int y, uint32_t col) {
  // Stub implementation - override in application
  (void)icon; (void)x; (void)y; (void)col;
}

WEAK_SYMBOL void draw_icon16(int icon, int x, int y, uint32_t col) {
  // Stub implementation - override in application
  (void)icon; (void)x; (void)y; (void)col;
}

WEAK_SYMBOL void set_projection(int x, int y, int w, int h) {
  // Stub implementation - override in application
  (void)x; (void)y; (void)w; (void)h;
}

// Viewport management
void set_viewport(window_t const *win) {
  extern SDL_Window *window;
  extern int screen_width, screen_height;
  
  if (!window) return;
  
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);
  
  float scale_x = (float)w / screen_width;
  float scale_y = (float)h / screen_height;
  
  int vp_x = (int)(win->frame.x * scale_x);
  int vp_y = (int)((screen_height - win->frame.y - win->frame.h) * scale_y);
  int vp_w = (int)(win->frame.w * scale_x);
  int vp_h = (int)(win->frame.h * scale_y);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(vp_x, vp_y, vp_w, vp_h);
  glScissor(vp_x, vp_y, vp_w, vp_h);
}

// Stencil management
void ui_set_stencil_for_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

void ui_set_stencil_for_root_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Frame management
void ui_begin_frame(void) {
  // Nothing to do for now
}

void ui_end_frame(void) {
  // Nothing to do for now
}

// Clear screen
void ui_clear_screen(float r, float g, float b) {
  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// Swap buffers
void ui_swap_buffers(void) {
  extern SDL_Window *window;
  if (window) {
    SDL_GL_SwapWindow(window);
  }
}
