#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <ui/user/user.h>
#include <ui/user/messages.h>
#include <ui/user/draw.h>

#include <mapview/sprites.h>
#include <mapview/map.h>

// External functions from game code
extern sprite_t *find_sprite(const char *name);
extern mapside_texture_t const *get_flat_texture(const char *name);
extern mapside_texture_t const *get_texture(const char *name);
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
    case kWindowMessagePaint:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(&win->frame, 1, 1, true);
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
    case kWindowMessageLeftButtonUp:
      send_message(win->parent, kWindowMessageCommand, kMakeDWord(win->id, BN_CLICKED), NULL);
      return true;
  }
  return false;
}
