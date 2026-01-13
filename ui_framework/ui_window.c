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

void dispatch_message(SDL_Event *evt) {
  // Forward to hooks
  for (winhook_t *hook = g_hooks; hook; hook = hook->next) {
    if (hook->msg == 0 || evt->type == hook->msg) {
      hook->func(NULL, evt->type, 0, evt, hook->userdata);
    }
  }
  
  // Note: Full SDL event handling should be implemented here or via hooks
  // For now, this is a minimal implementation that applications can extend
  // by registering hooks with register_window_hook()
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

void end_dialog(window_t *win, uint32_t code) {
  // Dialog support stub
  if (win) {
    destroy_window(win);
  }
}

uint32_t show_dialog(char const *title, const rect_t *rect, window_t *parent, 
                      winproc_t proc, void *param) {
  // Dialog support stub
  return 0;
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
