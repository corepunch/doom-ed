#include <SDL2/SDL.h>

#include "map.h"
#include "editor.h"

#define MAX_MAPNAMES 256

typedef char map_name_t[64];

typedef struct {
  uint16_t num_items;
  map_name_t items[MAX_MAPNAMES];
} items_t;

void collect_proc(const char *name, void *parm) {
  window_t *win = parm;
  items_t *arr = win->userdata;
  if (arr->num_items == MAX_MAPNAMES) return;
	strncpy(arr->items[arr->num_items], name, sizeof(map_name_t));
  arr->num_items++;
}

result_t win_project(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  items_t const *items = win->userdata;
  switch (msg) {
    case WM_CREATE:
			win->userdata = malloc(sizeof(items_t));
			find_all_maps(collect_proc, win);
      return true;
    case WM_PAINT:
      for (int i = 0, y = 0; i < items->num_items; i++, y += BUTTON_HEIGHT) {
        if (win->cursor_pos == i) {
          fill_rect(COLOR_TEXT_NORMAL, 0, y, win->frame.w, BUTTON_HEIGHT);
          draw_text_small(items->items[i], 4, y+3, COLOR_PANEL_BG);
        } else {
          draw_text_small(items->items[i], 4, y+3, COLOR_TEXT_NORMAL);
        }
      }
      return true;
    case WM_LBUTTONUP:
      win->cursor_pos = HIWORD(wparam)/BUTTON_HEIGHT;
      if (win->cursor_pos < items->num_items) {
        open_map(items->items[win->cursor_pos]);
      }
      invalidate_window(win);
      return true;
    case WM_KEYDOWN:
      if (wparam == SDL_SCANCODE_UP && win->cursor_pos > 0) {
        win->cursor_pos--;
        invalidate_window(win);
        return true;
      }
      if (wparam == SDL_SCANCODE_DOWN && win->cursor_pos < items->num_items-1) {
        win->cursor_pos++;
        invalidate_window(win);
        return true;
      }
      return false;
  }
  return false;
}
