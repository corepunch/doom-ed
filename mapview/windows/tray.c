#include "map.h"

#define TRAY_HEIGHT (BUTTON_HEIGHT+4)

extern int screen_width, screen_height;
extern window_t *windows;

result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void create_button(window_t *tray, window_t *window) {
  rect_t r = { tray->cursor_pos, 2, 0, 12 };
  window_t *button = create_window(window->title, 0, &r, tray, win_button, window);
  tray->cursor_pos += button->frame.w + 4;
  button->userdata = window;
}

static void on_win_created(window_t *win, uint32_t msg, uint32_t wparam, void *lparam, void *userdata) {
  if (!win->parent) {
    create_button(userdata, win);
  }
}

result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->cursor_pos = 8;
      win->frame = (rect_t){0,screen_height-TRAY_HEIGHT,screen_width,TRAY_HEIGHT};
      register_window_hook(WM_CREATE, on_win_created, win);
      return true;
    case WM_COMMAND:
      if (HIWORD(wparam) == BN_CLICKED) {
        window_t *button = lparam;
        show_window(button->userdata, ((window_t *)button->userdata)->hidden);
      }
      return true;
    default:
      break;
  }
  return false;
}
