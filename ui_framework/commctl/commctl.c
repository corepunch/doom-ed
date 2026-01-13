//
//  commctl.c
//  Common Controls - Standard UI controls implementation
//
//  Like Windows Common Controls (ComCtl32)
//

#include "commctl.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdlib.h>

// External dependencies - these must be provided by the application
// In Windows, these would be GDI functions, but here they're app-specific
extern void fill_rect(int color, int x, int y, int w, int h);
extern void draw_text_small(const char *text, int x, int y, int color);
extern int strwidth(const char *text);
extern int strnwidth(const char *text, int n);
extern void draw_icon8(const void *icon, int x, int y, int color);
extern void draw_button(int x, int y, int w, int h, bool pressed);

// External color constants
extern const int COLOR_FOCUSED;
extern const int COLOR_PANEL_BG;
extern const int COLOR_DARK_EDGE;
extern const int COLOR_TEXT_NORMAL;

// External constants
extern const int BUTTON_HEIGHT;
extern const int PADDING;
extern const int BUFFER_SIZE;
extern const void *icon8_maximize;

// Utility macros
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAKEDWORD(low, high) ((uint32_t)(((uint16_t)(low)) | ((uint32_t)((uint16_t)(high))) << 16))
#define LOWORD(dword) ((uint16_t)((dword) & 0xFFFF))
#define HIWORD(dword) ((uint16_t)(((dword) >> 16) & 0xFFFF))

//
// Button Control
//
result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern window_t *_focused;
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+6);
      win->frame.h = MAX(win->frame.h, BUTTON_HEIGHT);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.h, win->pressed);
      if (!win->pressed) {
        draw_text_small(win->title, win->frame.x+4, win->frame.y+4, COLOR_DARK_EDGE);
      }
      draw_text_small(win->title, win->frame.x+((win->pressed)?4:3), win->frame.y+((win->pressed)?4:3), COLOR_TEXT_NORMAL);
      return true;
    case WM_LBUTTONDOWN:
      win->pressed = true;
      invalidate_window(win);
      return true;
    case WM_LBUTTONUP:
      win->pressed = false;
      send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
      invalidate_window(win);
      return true;
    case WM_KEYDOWN:
      if (wparam == SDL_SCANCODE_RETURN || wparam == SDL_SCANCODE_SPACE) {
        win->pressed = true;
        invalidate_window(win);
        return true;
      }
      return false;
    case WM_KEYUP:
      if (wparam == SDL_SCANCODE_RETURN || wparam == SDL_SCANCODE_SPACE) {
        win->pressed = false;
        send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
        invalidate_window(win);
        return true;
      } else {
        return false;
      }
  }
  return false;
}

//
// Checkbox Control
//
result_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern window_t *_focused;
  bool *checked = (void *)&win->userdata;
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+18);
      win->frame.h = MAX(win->frame.h, 13);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, 10, 10, true);
      if (*checked) {
        draw_text_small("X", win->frame.x+2, win->frame.y+1, COLOR_TEXT_NORMAL);
      }
      draw_text_small(win->title, win->frame.x+13, win->frame.y+1, COLOR_TEXT_NORMAL);
      return true;
    case WM_LBUTTONUP:
      *checked = !*checked;
      send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
      invalidate_window(win);
      return true;
    case WM_KEYDOWN:
      if (wparam == SDL_SCANCODE_RETURN || wparam == SDL_SCANCODE_SPACE) {
        *checked = !*checked;
        send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
        invalidate_window(win);
        return true;
      }
      return false;
  }
  return false;
}

//
// Text Edit Control
//
result_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern window_t *_focused;
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+PADDING*2);
      win->frame.h = MAX(win->frame.h, 13);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.h, true);
      draw_text_small(win->title, win->frame.x+PADDING, win->frame.y+PADDING, COLOR_TEXT_NORMAL);
      if (_focused == win && win->editing) {
        fill_rect(COLOR_TEXT_NORMAL,
                  win->frame.x+PADDING+strnwidth(win->title, win->cursor_pos),
                  win->frame.y+PADDING,
                  2, 8);
      }
      return true;
    case WM_LBUTTONUP:
      if (_focused == win) {
        invalidate_window(win);
        win->editing = true;
        win->cursor_pos = 0;
        for (int i = 0; i <= strlen(win->title); i++) {
          int x1 = win->frame.x+PADDING+strnwidth(win->title, i);
          int x2 = win->frame.x+PADDING+strnwidth(win->title, win->cursor_pos);
          if (abs((int)LOWORD(wparam) - x1) < abs((int)LOWORD(wparam) - x2)) {
            win->cursor_pos = i;
          }
        }
      }
      return true;
    case WM_TEXTINPUT:
      if (strlen(win->title) + strlen(lparam) < BUFFER_SIZE - 1) {
        memmove(win->title + win->cursor_pos + 1,
                win->title + win->cursor_pos,
                strlen(win->title + win->cursor_pos) + 1);
        win->title[win->cursor_pos] = *(char *)lparam; // Only handle 1-byte characters
        win->cursor_pos++;
      }
      invalidate_window(win);
      return true;
    case WM_KEYDOWN:
      switch (wparam) {
        case SDL_SCANCODE_RETURN:
          if (!win->editing) {
            win->cursor_pos = (int)strlen(win->title);
            win->editing = true;
          } else {
            send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, EN_UPDATE), win);
            win->editing = false;
          }
          break;
        case SDL_SCANCODE_ESCAPE:
          win->editing = false;
          break;
        case SDL_SCANCODE_BACKSPACE:
          if (win->cursor_pos > 0 && win->editing) {
            memmove(win->title + win->cursor_pos - 1,
                    win->title + win->cursor_pos,
                    strlen(win->title + win->cursor_pos) + 1);
            win->cursor_pos--;
          }
          break;
        case SDL_SCANCODE_LEFT:
          if (win->cursor_pos > 0 && win->editing) {
            win->cursor_pos--;
          }
          break;
        case SDL_SCANCODE_RIGHT:
          if (win->cursor_pos < strlen(win->title) && win->editing) {
            win->cursor_pos++;
          }
          break;
        default:
          return win->editing;
      }
      invalidate_window(win);
      return true;
    case WM_KILLFOCUS:
      win->editing = false;
      invalidate_window(win);
      return true;
  }
  return false;
}

//
// Label (Static Text) Control
//
result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title));
      win->frame.h = MAX(win->frame.h, 8);
      win->notabstop = true;
      return true;
    case WM_PAINT:
      draw_text_small(win->title, win->frame.x, win->frame.y, COLOR_TEXT_NORMAL);
      return true;
  }
  return false;
}

//
// List Control  
//
#define LIST_HEIGHT BUTTON_HEIGHT
#define LIST_X 3
#define LIST_Y 3
#define MAX_COMBOBOX_STRINGS 256
typedef char combobox_string_t[64];

result_t win_list(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  window_t *cb = win->userdata;
  combobox_string_t const *texts = cb?cb->userdata:NULL;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      return true;
    case WM_PAINT:
      for (int i = 0; i < cb->cursor_pos; i++) {
        if (i == win->cursor_pos) {
          fill_rect(COLOR_TEXT_NORMAL, 0, i*LIST_HEIGHT, win->frame.h, LIST_HEIGHT);
          draw_text_small(texts[i], LIST_X, i*LIST_HEIGHT+LIST_Y, COLOR_PANEL_BG);
        } else {
          draw_text_small(texts[i], LIST_X, i*LIST_HEIGHT+LIST_Y, COLOR_TEXT_NORMAL);
        }
      }
      return true;
    case WM_LBUTTONDOWN:
      win->cursor_pos = HIWORD(wparam)/LIST_HEIGHT;
      if (win->cursor_pos < cb->cursor_pos) {
        strncpy(cb->title, texts[win->cursor_pos], sizeof(cb->title));
      }
      invalidate_window(win);
      return true;
    case WM_LBUTTONUP:
      send_message(get_root_window(cb), WM_COMMAND, MAKEDWORD(cb->id, CBN_SELCHANGE), cb);
      destroy_window(win);
      return true;
    case LIST_SELITEM:
      win->cursor_pos = wparam;
      return true;
  }
  return false;
}

//
// Combobox Control
//
result_t win_combobox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  combobox_string_t *texts = win->userdata;
  switch (msg) {
    case WM_CREATE:
      win_button(win, msg, wparam, lparam);
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+16);
      win->userdata = malloc(sizeof(combobox_string_t) * MAX_COMBOBOX_STRINGS);
      return true;
    case WM_DESTROY:
      free(win->userdata);
      return true;
    case WM_PAINT:
      win_button(win, msg, wparam, lparam);
      draw_icon8(icon8_maximize, win->frame.x+win->frame.w-10, win->frame.y+3, COLOR_TEXT_NORMAL);
      return true;
    case WM_LBUTTONUP: {
      win_button(win, msg, wparam, lparam);
      rect_t rect = {
        get_root_window(win)->frame.x + win->frame.x,
        get_root_window(win)->frame.y + win->frame.y + win->frame.h + 2,
        win->frame.w,
        100,
      };
      window_t *list = create_window("", WINDOW_NOTITLE|WINDOW_NORESIZE|WINDOW_VSCROLL, &rect, NULL, win_list, win);
      show_window(list, true);
      return true;
    }
    case WM_COMMAND:
      send_message(get_root_window(win), msg, MAKEDWORD(win->id, HIWORD(wparam)), win);
      return true;
  }
  return false;
}
