//
//  ui_window.c
//  UI Framework - Window management implementation
//
//  Extracted from DOOM-ED mapview/window.c
//

#include "ui_framework.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// External dependencies from application
extern int screen_width, screen_height;
extern bool running;

// Configuration macros - applications can override these
#ifndef SCALE_POINT
#define SCALE_POINT(x) ((x)/2)  // Default coordinate scaling
#endif

#ifndef SCROLL_SENSITIVITY
#define SCROLL_SENSITIVITY 5
#endif

#ifndef RESIZE_HANDLE
#define RESIZE_HANDLE 8
#endif

#ifndef CONTROL_BUTTON_WIDTH
#define CONTROL_BUTTON_WIDTH 8
#endif

#ifndef CONTROL_BUTTON_PADDING
#define CONTROL_BUTTON_PADDING 2
#endif

#ifndef CONTAINS
#define CONTAINS(x, y, rx, ry, rw, rh) ((x) >= (rx) && (x) < (rx) + (rw) && (y) >= (ry) && (y) < (ry) + (rh))
#endif

#define LOCAL_X(VALUE, WIN) (SCALE_POINT((VALUE).x) - (WIN)->frame.x + (WIN)->scroll[0])
#define LOCAL_Y(VALUE, WIN) (SCALE_POINT((VALUE).y) - (WIN)->frame.y + (WIN)->scroll[1])

// Internal structures
typedef struct winhook_s {
  winhook_func_t func;
  uint32_t msg;
  void *userdata;
  struct winhook_s *next;
} winhook_t;

typedef struct {
  window_t *target;
  int32_t msg;
  int32_t wparam;
  void *lparam;
} msg_t;

// Global state - exported
window_t *windows = NULL;
window_t *_focused = NULL;
window_t *_tracked = NULL;
window_t *_captured = NULL;

// Internal state
static winhook_t *g_hooks = NULL;
static window_t *_dragging = NULL;
static window_t *_resizing = NULL;
static int drag_anchor[2];
static struct {
  uint8_t read, write;
  msg_t messages[0x100];
} queue = {0};

// Window management functions

void register_window_hook(uint32_t msg, winhook_func_t func, void *userdata) {
  winhook_t *hook = malloc(sizeof(winhook_t));
  hook->func = func;
  hook->msg = msg;
  hook->userdata = userdata;
  hook->next = g_hooks;
  g_hooks = hook;
}

void invalidate_window(window_t *win) {
  if (!win->parent) {
    post_message(win, WM_NCPAINT, 0, NULL);
  }
  post_message(win, WM_PAINT, 0, NULL);
}

static void push_window(window_t *win, window_t **list)  {
  if (!*list) {
    *list = win;
  } else {
    window_t *p = *list;
    while (p->next) p = p->next;
    p->next = win;
  }
}

window_t *create_window(char const *title,
                        flags_t flags,
                        rect_t const *frame,
                        window_t *parent,
                        winproc_t proc,
                        void *lparam)
{
  window_t *win = malloc(sizeof(window_t));
  memset(win, 0, sizeof(window_t));
  win->frame = *frame;
  win->proc = proc;
  win->flags = flags;
  if (parent) {
    win->id = ++parent->child_id;
  } else {
    bool used[256]={0};
    for (window_t *w = windows; w; w = w->next) {
      used[w->id] = true;
    }
    for (int i = 1; i < 256; i++) {
      if (!used[i]) {
        win->id = i;
      }
    }
    if (win->id == 0) {
      printf("Too many windows open\n");
    }
  }
  win->parent = parent;
  strncpy(win->title, title, sizeof(win->title));
  _focused = win;
  push_window(win, parent ? &parent->children : &windows);
  send_message(win, WM_CREATE, 0, lparam);
  if (parent) {
    invalidate_window(win);
  }
  return win;
}

bool do_windows_overlap(const window_t *a, const window_t *b) {
  if (!a->visible || !b->visible)
    return false;
  return a && b &&
  a->frame.x < b->frame.x + b->frame.w && a->frame.x + a->frame.w > b->frame.x &&
  a->frame.y < b->frame.y + b->frame.h && a->frame.y + a->frame.h > b->frame.y;
}

static void invalidate_overlaps(window_t *win) {
  for (window_t *t = windows; t; t = t->next) {
    if (t != win && do_windows_overlap(t, win)) {
      invalidate_window(t);
    }
  }
}

void move_window(window_t *win, int x, int y) {
  post_message(win, WM_RESIZE, 0, NULL);
  post_message(win, WM_REFRESHSTENCIL, 0, NULL);

  invalidate_overlaps(win);
  invalidate_window(win);

  win->frame.x = x;
  win->frame.y = y;
}

void resize_window(window_t *win, int new_w, int new_h) {
  post_message(win, WM_RESIZE, 0, NULL);
  post_message(win, WM_REFRESHSTENCIL, 0, NULL);

  invalidate_overlaps(win);
  invalidate_window(win);

  win->frame.w = new_w > 0 ? new_w : win->frame.w;
  win->frame.h = new_h > 0 ? new_h : win->frame.h;
}

static void remove_from_global_list(window_t *win) {
  if (win == windows) {
    windows = win->next;
  } else {
    for (window_t *w=windows->next,*p=windows;w;w=w->next,p=p->next) {
      if (w == win) {
        p->next = w->next;
        break;
      }
    }
  }
}

static void remove_from_global_hooks(window_t *win) {
  if (!g_hooks) return;
  
  while (g_hooks && g_hooks->userdata == win) {
    winhook_t *h = g_hooks;
    g_hooks = g_hooks->next;
    free(h);
  }
  
  if (!g_hooks) return;
  
  for (winhook_t *w=g_hooks->next,*p=g_hooks;w;w=w->next,p=p->next) {
    if (w->userdata == win) {
      winhook_t *h = w;
      p->next = w->next;
      free(h);
    }
  }
}

static void remove_from_global_queue(window_t *win) {
  for (uint8_t w = queue.write, r = queue.read; r != w; r++) {
    if (queue.messages[r].target == win) {
      queue.messages[r].target = NULL;
    }
  }
}

void clear_window_children(window_t *win) {
  for (window_t *item = win->children, *next = item ? item->next : NULL;
       item; item = next, next = next?next->next:NULL) {
    destroy_window(item);
  }
  win->children = NULL;
}

void destroy_window(window_t *win) {
  post_message((window_t*)1, WM_REFRESHSTENCIL, 0, NULL);
  invalidate_overlaps(win);
  send_message(win, WM_DESTROY, 0, NULL);
  if (_focused == win) set_focus(NULL);
  if (_tracked == win) _tracked = NULL;
  if (_captured == win) _captured = NULL;
  clear_window_children(win);
  remove_from_global_hooks(win);
  remove_from_global_queue(win);
  if (win->parent) {
    if (win->parent->children == win) {
      win->parent->children = win->next;
    } else {
      window_t **p = &win->parent->children;
      while (*p && *p != win) p = &(*p)->next;
      if (*p) *p = win->next;
    }
  } else {
    remove_from_global_list(win);
  }
  if (win->toolbar_buttons) {
    free(win->toolbar_buttons);
  }
  free(win);
}

window_t *find_window(int x, int y) {
  for (window_t *w = windows; w; w = w->next) {
    if (!w->visible) continue;
    if (x >= w->frame.x && x < w->frame.x + w->frame.w &&
        y >= w->frame.y && y < w->frame.y + w->frame.h) {
      return w;
    }
  }
  return NULL;
}

window_t *get_root_window(window_t *window) {
  while (window && window->parent) window = window->parent;
  return window;
}

void track_mouse(window_t *win) {
  if (_tracked != win) {
    if (_tracked) {
      send_message(_tracked, WM_MOUSELEAVE, 0, NULL);
    }
    _tracked = win;
  }
}

void set_capture(window_t *win) {
  _captured = win;
}

void set_focus(window_t* win) {
  if (_focused != win) {
    if (_focused) {
      send_message(_focused, WM_KILLFOCUS, 0, NULL);
    }
    _focused = win;
    if (win) {
      send_message(win, WM_SETFOCUS, 0, NULL);
      // Move to top logic would go here if needed
    }
  }
}

// Helper function for mouse event routing to child windows
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

void dispatch_message(SDL_Event *evt) {
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
          }
        }
      }
      break;
  }
  
  // Forward to hooks
  for (winhook_t *hook = g_hooks; hook; hook = hook->next) {
    if (hook->msg == 0 || evt->type == hook->msg) {
      hook->func(NULL, evt->type, 0, evt, hook->userdata);
    }
  }
}

int get_message(SDL_Event *evt) {
  if (queue.read != queue.write) {
    // Process queued messages first
    return 0; // Stub
  }
  return SDL_PollEvent(evt);
}

void repost_messages(void) {
  while (queue.read != queue.write) {
    msg_t *msg = &queue.messages[queue.read++];
    if (msg->target && msg->target->proc) {
      msg->target->proc(msg->target, msg->msg, msg->wparam, msg->lparam);
    }
  }
  queue.read = queue.write = 0;
}

int window_title_bar_y(window_t const *win) {
  // Simplified calculation
  return win->frame.y;
}

int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (!win || !win->proc) return 0;
  
  for (winhook_t *hook = g_hooks; hook; hook = hook->next) {
    if (hook->msg == msg) {
      hook->func(win, msg, wparam, lparam, hook->userdata);
    }
  }
  
  return win->proc(win, msg, wparam, lparam);
}

void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (((queue.write + 1) & 0xFF) == queue.read) {
    return; // Queue full
  }
  
  msg_t *m = &queue.messages[queue.write++];
  m->target = win;
  m->msg = msg;
  m->wparam = wparam;
  m->lparam = lparam;
}

window_t *get_window_item(window_t const *win, uint32_t id) {
  if (!win) return NULL;
  
  for (window_t *item = win->children; item; item = item->next) {
    if (item->id == id) return item;
  }
  return NULL;
}

void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...) {
  window_t *item = get_window_item(win, id);
  if (!item) return;
  
  va_list args;
  va_start(args, fmt);
  vsnprintf(item->title, sizeof(item->title), fmt, args);
  va_end(args);
  invalidate_window(item);
}

void load_window_children(window_t *win, windef_t const *def) {
  // Stub - application should implement this
}

void show_window(window_t *win, bool visible) {
  if (!win) return;
  if (win->visible != visible) {
    win->visible = visible;
    if (visible) {
      post_message(win, WM_SHOWWINDOW, 1, NULL);
      invalidate_window(win);
    } else {
      post_message(win, WM_SHOWWINDOW, 0, NULL);
    }
  }
}

bool is_window(window_t *win) {
  for (window_t *w = windows; w; w = w->next) {
    if (w == win) return true;
  }
  return false;
}

// Dialog support
static uint32_t _return_code;

void end_dialog(window_t *win, uint32_t code) {
  _return_code = code;
  if (win) {
    destroy_window(win);
  }
}

uint32_t show_dialog(char const *title, const rect_t *rect, window_t *parent, 
                      winproc_t proc, void *param) {
  extern bool running;
  SDL_Event event;
  uint32_t flags = WINDOW_VSCROLL|WINDOW_DIALOG|WINDOW_NOTRAYBUTTON;
  window_t *dlg = create_window(title, flags, rect, NULL, proc, param);
  if (parent) enable_window(parent, false);
  show_window(dlg, true);
  while (running && is_window(dlg)) {
    while (get_message(&event)) {
      dispatch_message(&event);
    }
    repost_messages();
  }
  if (parent) enable_window(parent, true);
  return _return_code;
}

void enable_window(window_t *win, bool enable) {
  if (!win) return;
  if (!enable && _focused == win) {
    set_focus(NULL);
  }
  win->disabled = !enable;
  invalidate_window(win);
}

// Window z-ordering
void move_to_top(window_t* _win) {
  window_t *win = get_root_window(_win);
  post_message(win, WM_REFRESHSTENCIL, 0, NULL);
  invalidate_window(win);
  
  if (win->flags & WINDOW_ALWAYSINBACK)
    return;
  
  window_t **head = &windows, *p = NULL, *n = *head;
  
  // Find the node `win` in the list
  while (n != win) {
    p = n;
    n = n->next;
    if (!n) return;  // If `win` is not found, exit
  }
  
  // Remove the node `win` from the list
  if (p) p->next = win->next;
  else *head = win->next;  // If `win` is at the head, update the head
  
  // Ensure that if `win` was the only node, we don't append it to itself
  if (!*head) return;  // If the list becomes empty, there's nothing to append
  
  // Find the tail of the list
  window_t *tail = *head;
  while (tail->next)
    tail = tail->next;
  
  // Append `win` to the end of the list
  tail->next = win;
  win->next = NULL;
}

// Tab navigation
window_t* find_next_tab_stop(window_t *win, bool allow_current) {
  if (!win) return NULL;
  window_t *next;
  if ((next = find_next_tab_stop(win->children, true))) return next;
  if (!win->notabstop && (win->parent || win->visible) && allow_current) return win;
  if ((next = find_next_tab_stop(win->next, true))) return next;
  return allow_current ? NULL : find_next_tab_stop(win->parent, false);
}

window_t* find_prev_tab_stop(window_t* win) {
  window_t *it = (win = (win->parent ? win : find_next_tab_stop(win, false)));
  for (window_t *next = find_next_tab_stop(it, false); next != win;
       it = next, next = find_next_tab_stop(next, false));
  return it;
}
