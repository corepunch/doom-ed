#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

// Game-specific types (defined in mapview/sprites.h and mapview/map.h)
typedef struct {
  int texture;
  int width;
  int height;
} sprite_t;

typedef struct {
  int texture;
  int width;
  int height;
} mapside_texture_t;

// Weak symbols for game-specific functions - can be overridden by application
// This allows the UI framework to compile standalone
#ifndef __APPLE__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif

// Stub implementations that can be overridden by the application
WEAK_SYMBOL sprite_t const *find_sprite(const char *name) {
  (void)name;
  return NULL;
}

WEAK_SYMBOL mapside_texture_t const *get_flat_texture(const char *name) {
  (void)name;
  return NULL;
}

WEAK_SYMBOL mapside_texture_t const *get_texture(const char *name) {
  (void)name;
  return NULL;
}

extern window_t *_focused;

// Helper function to fit sprite in target rect
rect_t fit_sprite(sprite_t const *spr, rect_t const *target) {
  float scale = fminf(1, fminf(((float)target->w) / spr->width,
                               ((float)target->h) / spr->height));
  return (rect_t) {
    target->x+(target->w-spr->width*scale)/2,
    target->y+(target->h-spr->height*scale)/2,
    spr->width * scale,
    spr->height * scale
  };
}

// Sprite control window procedure
result_t win_sprite(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  sprite_t const *spr;
  mapside_texture_t const *tex;
  switch (msg) {
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.w, true);
      if (!*win->title) return false;
      if ((spr = find_sprite(win->title))) {
        rect_t r = fit_sprite(spr, &win->frame);
        draw_rect(spr->texture, r.x, r.y, r.w, r.h);
      } else if ((tex = get_flat_texture(win->title))||(tex = get_texture(win->title))) {
        float scale = fminf(1, fminf(((float)win->frame.w) / tex->width,
                                     ((float)win->frame.h) / tex->height));
        draw_rect(tex->texture,
                  win->frame.x+(win->frame.w-tex->width*scale)/2,
                  win->frame.y+(win->frame.h-tex->height*scale)/2,
                  tex->width * scale,
                  tex->height * scale);
      }
      return true;
    case WM_LBUTTONUP:
      send_message(win->parent, WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), NULL);
      return true;
  }
  return false;
}
