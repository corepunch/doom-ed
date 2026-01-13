#include <SDL2/SDL.h>
#include "gl_compat.h"

// Include UI Framework - generic window management now comes from here
#include "ui_framework.h"
#include "commctl/commctl.h"  // Common controls

#include "map.h"
#include "sprites.h"
#include "editor.h"

#define RESIZE_HANDLE 8

extern int screen_width, screen_height;

// NOTE: The following globals are now provided by ui_framework:
// - window_t *windows
// - window_t *_focused
// - window_t *_tracked
// - window_t *_captured

// Application-specific state
static window_t *_dragging = NULL;
static window_t *_resizing = NULL;
static int drag_anchor[2];

#define LOCAL_X(VALUE, WIN) (SCALE_POINT((VALUE).x) - (WIN)->frame.x + (WIN)->scroll[0])
#define LOCAL_Y(VALUE, WIN) (SCALE_POINT((VALUE).y) - (WIN)->frame.y + (WIN)->scroll[1])

// Application-specific rendering functions (DOOM-ED specific, stays in mapview)

void draw_button(int x, int y, int w, int h, bool pressed) {
  fill_rect(pressed?COLOR_DARK_EDGE:COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
  fill_rect(pressed?COLOR_LIGHT_EDGE:COLOR_DARK_EDGE, x, y, w+1, h+1);
  fill_rect(pressed?COLOR_PANEL_DARK_BG:COLOR_PANEL_BG, x, y, w, h);
  if (pressed) {
    fill_rect(COLOR_FLARE, x+w, y+h, 1, 1);
  } else {
    fill_rect(COLOR_FLARE, x-1, y-1, 1, 1);
  }
}

static void draw_focused(rect_t const *r) {
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_FOCUSED, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_FOCUSED, r->x, r->y+r->h, r->w+1, 1);
}

static void draw_bevel(rect_t const *r) {
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_DARK_EDGE, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_DARK_EDGE, r->x, r->y+r->h, r->w+1, 1);
  fill_rect(COLOR_FLARE, r->x-1, r->y-1, 1, 1);
}

static int titlebar_height(window_t const *win) {
  int t = 0;
  if (!(win->flags&WINDOW_NOTITLE)) {
    t += TITLEBAR_HEIGHT;
  }
  if (win->flags&WINDOW_TOOLBAR) {
    t += TOOLBAR_HEIGHT;
  }
  return t;
}

static void draw_panel(window_t const *win) {
  int t = titlebar_height(win);
  int x = win->frame.x, y = win->frame.y-t;
  int w = win->frame.w, h = win->frame.h+t;
  bool active = _focused == win;
  if (active) {
    draw_focused(MAKERECT(x, y, w, h));
  } else {
    draw_bevel(MAKERECT(x, y, w, h));
  }
  if (!(win->flags & WINDOW_NORESIZE)) {
    int r = RESIZE_HANDLE;
    fill_rect(COLOR_LIGHT_EDGE, x+w, y+h-r+1, 1, r);
    fill_rect(COLOR_LIGHT_EDGE, x+w-r+1, y+h, r, 1);
  }
  if (!(win->flags&WINDOW_NOFILL)) {
    fill_rect(COLOR_PANEL_BG, x, y, w, h);
  }
}


void set_viewport(window_t const *win) {
  extern SDL_Window *window;
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);
  
  float scale_x = (float)w / screen_width;
  float scale_y = (float)h / screen_height;
  
  int vp_x = (int)(win->frame.x * scale_x);
  int vp_y = (int)((screen_height - win->frame.y - win->frame.h) * scale_y); // flip Y
  int vp_w = (int)(win->frame.w * scale_x);
  int vp_h = (int)(win->frame.h * scale_y);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(vp_x, vp_y, vp_w, vp_h);
  glScissor(vp_x, vp_y, vp_w, vp_h);
}

static void paint_window_stencil(window_t const *w) {
  int p = 1;
  int t = titlebar_height(w);
  glStencilFunc(GL_ALWAYS, w->id, 0xFF);            // Always pass
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // Replace stencil with window ID
  draw_rect(1, w->frame.x-p, w->frame.y-t-p, w->frame.w+p*2, w->frame.h+t+p*2);
}

static void repaint_stencil(void) {
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  
  glEnable(GL_STENCIL_TEST);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  for (window_t *w = windows; w; w = w->next) {
    if (!w->visible)
      continue;
    send_message(w, WM_PAINTSTENCIL, 0, NULL);
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}




// NOTE: The following are now in ui_framework:
// - move_to_top, find_next_tab_stop, find_prev_tab_stop (window navigation)
// - handle_mouse (mouse event routing) - will be moved next

static int handle_mouse(int msg, window_t *win, int x, int y) {
  for (window_t *c = win->children; c; c = c->next) {
    if (CONTAINS(x, y, c->frame.x, c->frame.y, c->frame.w, c->frame.h) &&
        c->proc(c, msg, MAKEDWORD(x, y), NULL))
    {
      return true;
    }
  }
  return false;
}

// NOTE: dispatch_message contains SDL event handling - this is being moved to ui_framework
// with application-specific hooks for _dragging/_resizing behavior

void dispatch_message(SDL_Event *evt) {
  extern bool running;
  window_t *win;
  switch (evt->type) {
    case SDL_QUIT:
      running = false;
      break;
    case SDL_TEXTINPUT:
      send_message(_focused, WM_TEXTINPUT, 0, evt->text.text);
      break;
    case SDL_KEYDOWN:
      if (_focused && !send_message(_focused, WM_KEYDOWN, evt->key.keysym.scancode, NULL)) {
        switch (evt->key.keysym.scancode) {
          case SDL_SCANCODE_TAB:
            if (evt->key.keysym.mod & KMOD_SHIFT) {
              set_focus(find_prev_tab_stop(_focused));
            } else {
              set_focus(find_next_tab_stop(_focused, false));
            }
            break;
          default:
            break;
        }
      }
      break;
    case SDL_KEYUP:
      send_message(_focused, WM_KEYUP, evt->key.keysym.scancode, NULL);
      break;
    case SDL_JOYAXISMOTION:
      send_message(_focused, WM_JOYAXISMOTION, MAKEDWORD(evt->jaxis.axis, evt->jaxis.value), NULL);
      break;
    case SDL_JOYBUTTONDOWN:
      send_message(_focused, WM_JOYBUTTONDOWN, evt->jbutton.button, NULL);
      break;
    case SDL_MOUSEMOTION:
      if (_dragging) {
        move_window(_dragging,
                    SCALE_POINT(evt->motion.x) - drag_anchor[0],
                    SCALE_POINT(evt->motion.y) - drag_anchor[1]);
      } else if (_resizing) {
        int new_w = SCALE_POINT(evt->motion.x) - _resizing->frame.x;
        int new_h = SCALE_POINT(evt->motion.y) - _resizing->frame.y;
        resize_window(_resizing, new_w, new_h);
      } else if (((win = _captured) ||
                  (win = find_window(SCALE_POINT(evt->motion.x),
                                     SCALE_POINT(evt->motion.y)))))
      {
        if (win->disabled) return;
        int16_t x = LOCAL_X(evt->motion, win);
        int16_t y = LOCAL_Y(evt->motion, win);
        int16_t dx = evt->motion.xrel;
        int16_t dy = evt->motion.yrel;
        if (y >= 0 && (win == _captured || win == _focused)) {
          send_message(win, WM_MOUSEMOVE, MAKEDWORD(x, y), (void*)(intptr_t)MAKEDWORD(dx, dy));
        }
      }
      if (_tracked && !CONTAINS(SCALE_POINT(evt->motion.x),
                                SCALE_POINT(evt->motion.y),
                                _tracked->frame.x, _tracked->frame.y,
                                _tracked->frame.w, _tracked->frame.h))
      {
        track_mouse(NULL);
      }
      break;
    case SDL_MOUSEWHEEL:
      if ((win = _captured) ||
          (win = find_window(SCALE_POINT(evt->wheel.mouseX),
                             SCALE_POINT(evt->wheel.mouseY))))
      {
        if (win->disabled) return;
        send_message(win, WM_WHEEL, MAKEDWORD(-evt->wheel.x * SCROLL_SENSITIVITY, evt->wheel.y * SCROLL_SENSITIVITY), NULL);
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      if ((win = _captured) ||
          (win = find_window(SCALE_POINT(evt->button.x),
                             SCALE_POINT(evt->button.y))))
      {
        if (win->disabled) return;
        if (win->parent) {
          set_focus(win);
        } else {
          move_to_top(win);
        }
        int x = LOCAL_X(evt->button, win);
        int y = LOCAL_Y(evt->button, win);
        if (x >= win->frame.w - RESIZE_HANDLE &&
            y >= win->frame.h - RESIZE_HANDLE &&
            !win->parent &&
            !(win->flags&WINDOW_NORESIZE) &&
            win != _captured)
        {
          _resizing = win;
        } else if (SCALE_POINT(evt->button.y) < win->frame.y && !win->parent && win != _captured) {
          _dragging = win;
          drag_anchor[0] = SCALE_POINT(evt->button.x) - win->frame.x;
          drag_anchor[1] = SCALE_POINT(evt->button.y) - win->frame.y;
        } else if (win == _focused) {
          int msg = 0;
          switch (evt->button.button) {
            case 1: msg = WM_LBUTTONDOWN; break;
            case 3: msg = WM_RBUTTONDOWN; break;
          }
          if (!handle_mouse(msg, win, x, y)) {
            send_message(win, msg, MAKEDWORD(x, y), NULL);
          }
        }
      }
      break;
      
    case SDL_MOUSEBUTTONUP:
      if (_dragging) {
        int x = SCALE_POINT(evt->button.x);
        int y = SCALE_POINT(evt->button.y);
        int b = (_dragging->frame.x + _dragging->frame.w - CONTROL_BUTTON_PADDING - x) / CONTROL_BUTTON_WIDTH;
        if (b == 0) {
          if (_dragging->flags & WINDOW_DIALOG) {
            end_dialog(_dragging, -1);
          } else {
            show_window(_dragging, false);
          }
          _dragging = NULL;
        } else {
          switch (evt->button.button) {
            case 1: send_message(_dragging, WM_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
              // case 3: send_message(win, WM_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
          }
          set_focus(_dragging);
          _dragging = NULL;
        }
      } else if (_resizing) {
        set_focus(_resizing);
        _resizing = NULL;
      } else if ((win = _captured) ||
                 (win = find_window(SCALE_POINT(evt->button.x),
                                    SCALE_POINT(evt->button.y))))
      {
        if (win->disabled) return;
        set_focus(win);
        if (SCALE_POINT(evt->button.y) >= win->frame.y || win == _captured) {
          int x = LOCAL_X(evt->button, win);
          int y = LOCAL_Y(evt->button, win);
          int msg = 0;
          switch (evt->button.button) {
            case 1: msg = WM_LBUTTONUP; break;
            case 3: msg = WM_RBUTTONUP; break;
          }
          if (!handle_mouse(msg, win, x, y)) {
            send_message(win, msg, MAKEDWORD(x, y), NULL);
          }
        } else {
          int x = SCALE_POINT(evt->button.x);
          int y = SCALE_POINT(evt->button.y);
          switch (evt->button.button) {
            case 1: send_message(win, WM_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
              //              case 3: send_message(win, WM_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
          }
        }
      }
      break;
  }
}



void draw_windows(bool rich) {
  for (window_t *win = windows; win; win = win->next) {
    invalidate_window(win);
  }
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
}


void draw_window_controls(window_t *win) {
  rect_t r = win->frame;
  int t = titlebar_height(win);
  fill_rect(COLOR_PANEL_DARK_BG, r.x, r.y-t, r.w, t);
//  draw_bevel(MAKERECT(r.x+1, r.y+1-t, r.w-2, t-2));
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  // draw_button(frame->x+frame->w-11, frame->y-t+1, 10, 10, false);
  // draw_button(frame->x+frame->w-11-12, frame->y-t+1, 10, 10, false);
  // draw_rect(icon3_tex, frame->x+2, frame->y+2-t, 8, 8);
  for (int i = 0; i < 1; i++) {
    int x = win->frame.x + win->frame.w - (i+1)*CONTROL_BUTTON_WIDTH - CONTROL_BUTTON_PADDING;
    int y = window_title_bar_y(win);
    draw_icon8(icon8_minus + i, x, y, COLOR_TEXT_NORMAL);
  }
}

#define TB_SPACING 18



int get_text_width(const char*);

// NOTE: Common controls (win_button, win_checkbox, win_textedit, win_label, win_list, win_combobox)
// have been moved to ui_framework/commctl/
// Application-specific controls remain here:


}




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

typedef struct {
  window_t *items[16];
  uint8_t num_items;
} stack_data_t;

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


#include <stdarg.h>
#include <stdio.h>
#include <string.h>





static uint32_t _return_code;

uint32_t
show_dialog(char const *title,
            const rect_t *frame,
            struct window_s *owner,
            winproc_t proc,
            void *param)
{
  extern bool running;
  SDL_Event event;
  uint32_t flags = WINDOW_VSCROLL|WINDOW_DIALOG|WINDOW_NOTRAYBUTTON;
  window_t *dlg = create_window("Things", flags, frame, NULL, proc, param);
  enable_window(owner, false);
  show_window(dlg, true);
  while (running && is_window(dlg)) {
    while (get_message(&event)) {
      dispatch_message(&event);
    }
    repost_messages();
  }
  enable_window(owner, true);
  return _return_code;
}

