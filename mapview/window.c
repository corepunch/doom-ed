#include <SDL2/SDL.h>
#include "gl_compat.h"

// Include UI Framework - complete UI library
#include "ui_framework.h"
#include "commctl/commctl.h"

#include "map.h"
#include "sprites.h"
#include "editor.h"

// Application-specific: DOOM sprite/texture viewer control
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

// Helper: Maps class names to window procedures
window_t *create_window2(windef_t const *def, rect_t const *r, window_t *parent) {
  winproc_t proc = NULL;
  if (!strcmp(def->classname, "TEXT")) {
    proc = win_label;
  } else if (!strcmp(def->classname, "BUTTON")) {
    proc = win_button;
  } else if (!strcmp(def->classname, "CHECKBOX")) {
    proc = win_checkbox;
  } else if (!strcmp(def->classname, "EDITTEXT")) {
    proc = win_textedit;
  } else if (!strcmp(def->classname, "COMBOBOX")) {
    proc = win_combobox;
  } else if (!strcmp(def->classname, "SPRITE")) {
    proc = win_sprite;
  } else {
    return NULL;
  }
  window_t *win = create_window(def->text, def->flags, r, parent, proc, NULL);
  win->id = def->id;
  return win;
}
