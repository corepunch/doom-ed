//
//  ui_window.c
//  UI Framework - Window management implementation
//
//  Extracted from DOOM-ED mapview/window.c
//

#include "ui_framework.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Internal state
typedef struct winhook_s {
  winhook_func_t func;
  uint32_t msg;
  void *userdata;
  struct winhook_s *next;
} winhook_t;

winhook_t *g_hooks = NULL;
window_t *windows = NULL;
window_t *_focused = NULL;
window_t *_tracked = NULL;
window_t *_captured = NULL;

static window_t *_dragging = NULL;
static window_t *_resizing = NULL;
static int drag_anchor[2];

typedef struct {
  window_t *target;
  int32_t msg;
  int32_t wparam;
  void *lparam;
} msg_t;

struct {
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
  // Stub: would trigger repaint in full implementation
}

static void push_window(window_t *win, window_t **list) {
  window_t *current = *list;
  if (!current) {
    *list = win;
    win->next = NULL;
    return;
  }
  
  if (win->flags & WINDOW_ALWAYSINBACK) {
    // Insert at end
    while (current->next && !(current->next->flags & WINDOW_ALWAYSINBACK)) {
      current = current->next;
    }
    win->next = current->next;
    current->next = win;
  } else {
    // Insert at beginning
    win->next = *list;
    *list = win;
  }
}

static bool do_windows_overlap(const window_t *a, const window_t *b) {
  return !(a->frame.x + a->frame.w < b->frame.x ||
           a->frame.x > b->frame.x + b->frame.w ||
           a->frame.y + a->frame.h < b->frame.y ||
           a->frame.y > b->frame.y + b->frame.h);
}

void move_window(window_t *win, int x, int y) {
  win->frame.x = x;
  win->frame.y = y;
}

void resize_window(window_t *win, int new_w, int new_h) {
  win->frame.w = new_w;
  win->frame.h = new_h;
}

void clear_window_children(window_t *win) {
  window_t *child = win->children;
  while (child) {
    window_t *next = child->next;
    destroy_window(child);
    child = next;
  }
  win->children = NULL;
}

void destroy_window(window_t *win) {
  if (!win) return;
  
  // Send WM_DESTROY message
  send_message(win, WM_DESTROY, 0, NULL);
  
  // Destroy children first
  clear_window_children(win);
  
  // Remove from parent's children list
  if (win->parent) {
    window_t **current = &win->parent->children;
    while (*current && *current != win) {
      current = &(*current)->next;
    }
    if (*current) {
      *current = win->next;
    }
  } else {
    // Remove from top-level windows list
    window_t **current = &windows;
    while (*current && *current != win) {
      current = &(*current)->next;
    }
    if (*current) {
      *current = win->next;
    }
  }
  
  // Free toolbar buttons if any
  if (win->toolbar_buttons) {
    free(win->toolbar_buttons);
  }
  
  // Clear focus if this window had it
  if (_focused == win) {
    _focused = NULL;
  }
  if (_tracked == win) {
    _tracked = NULL;
  }
  if (_captured == win) {
    _captured = NULL;
  }
  
  free(win);
}

static window_t *find_window(int x, int y) {
  // Search from front to back
  window_t *current = windows;
  while (current) {
    if (current->visible &&
        x >= current->frame.x && x < current->frame.x + current->frame.w &&
        y >= current->frame.y && y < current->frame.y + current->frame.h) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static window_t *get_root_window(window_t *window) {
  while (window && window->parent) {
    window = window->parent;
  }
  return window;
}

void track_mouse(window_t *win) {
  _tracked = win;
}

void set_capture(window_t *win) {
  _captured = win;
}

void set_focus(window_t *win) {
  if (_focused != win) {
    _focused = win;
    // In full implementation, would send focus change messages
  }
}

void dispatch_message(SDL_Event *evt) {
  // This is a simplified version - full implementation in original window.c
  // handles all SDL event types and converts them to window messages
  
  if (!evt) return;
  
  // Process hooks
  winhook_t *hook = g_hooks;
  while (hook) {
    if (hook->msg == 0 || (evt->type == hook->msg)) {
      hook->func(NULL, evt->type, 0, evt, hook->userdata);
    }
    hook = hook->next;
  }
}

int get_message(SDL_Event *evt) {
  if (queue.read != queue.write) {
    msg_t *msg = &queue.messages[queue.read++];
    // Convert internal message to SDL_Event (simplified)
    return 1;
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
  // Calculate title bar Y position
  int t = 0;
  if (!(win->flags & WINDOW_NOTITLE)) {
    t += 16; // TITLEBAR_HEIGHT
  }
  if (win->flags & WINDOW_TOOLBAR) {
    t += 16; // TOOLBAR_HEIGHT
  }
  return win->frame.y - t;
}

int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (!win || !win->proc) return 0;
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
  
  window_t *child = win->children;
  while (child) {
    if (child->id == id) {
      return child;
    }
    child = child->next;
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
}

void load_window_children(window_t *win, windef_t const *def) {
  // Stub: would create child windows from definition
}

void show_window(window_t *win, bool visible) {
  if (!win) return;
  win->visible = visible;
}

bool is_window(window_t *win) {
  // Check if window is valid
  window_t *current = windows;
  while (current) {
    if (current == win) return true;
    current = current->next;
  }
  return false;
}

void end_dialog(window_t *win, uint32_t code) {
  // Stub: dialog support
}

uint32_t show_dialog(char const *title, const rect_t *rect, window_t *parent, 
                      winproc_t proc, void *param) {
  // Stub: dialog support
  return 0;
}

void enable_window(window_t *win, bool enable) {
  if (!win) return;
  win->disabled = !enable;
}

window_t *create_window(char const *title, flags_t flags, const rect_t *rect, 
                         window_t *parent, winproc_t proc, void *param) {
  window_t *win = calloc(1, sizeof(window_t));
  if (!win) return NULL;
  
  if (rect) {
    win->frame = *rect;
  }
  
  win->flags = flags;
  win->proc = proc;
  win->visible = false;
  win->parent = parent;
  
  if (title) {
    strncpy(win->title, title, sizeof(win->title) - 1);
  }
  
  // Add to parent's children or to top-level windows
  if (parent) {
    push_window(win, &parent->children);
  } else {
    push_window(win, &windows);
  }
  
  // Send WM_CREATE message
  if (proc) {
    proc(win, WM_CREATE, 0, param);
  }
  
  return win;
}
