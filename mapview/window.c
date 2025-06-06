#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "sprites.h"
#include "editor.h"

#define MAX_WINDOWS 64

#define RESIZE_HANDLE 8

extern int screen_width, screen_height;

#define MAX_HOOKED_MESSAGES 512

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

#define LOCAL_X(VALUE, WIN) (SCALE_POINT((VALUE).x) - (WIN)->frame.x + (WIN)->scroll[0])
#define LOCAL_Y(VALUE, WIN) (SCALE_POINT((VALUE).y) - (WIN)->frame.y + (WIN)->scroll[1])

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

void register_window_hook(uint32_t msg, winhook_func_t func, void *userdata) {
  winhook_t *hook = malloc(sizeof(winhook_t));
  hook->func = func;
  hook->msg = msg;
  hook->userdata = userdata;
  hook->next = g_hooks;
  g_hooks = hook;
}

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

static void draw_panel(window_t const *win) {
  int t = (win->flags&WINDOW_NOTITLE)?0:TITLEBAR_HEIGHT;
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

void invalidate_window(window_t *win) {
  if (!win->parent) {
    post_message(win, WM_NCPAINT, 0, NULL);
  }
  post_message(win, WM_PAINT, 0, NULL);
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
  int t = (w->flags&WINDOW_NOTITLE)?0:TITLEBAR_HEIGHT;
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

void push_window(window_t *win, window_t **windows)  {
  if (!*windows) {
    *windows = win;
  } else {
    window_t *p = *windows;
    while (p->next) p = p->next;
    p->next = win;
  }
}

window_t*
create_window(char const *title,
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
  while (win == g_hooks->userdata) {
    winhook_t *h = g_hooks;
    g_hooks = g_hooks->next;
    free(h);
  }
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
  if (_captured == win) set_capture(NULL);
  if (_tracked == win) track_mouse(NULL);
  if (_dragging == win) _dragging = NULL;
  if (_resizing == win) _resizing = NULL;
  remove_from_global_list(win);
  remove_from_global_hooks(win);
  remove_from_global_queue(win);
  clear_window_children(win);
  free(win);
}

#define CONTAINS(x, y, x1, y1, w1, h1) \
((x1) <= (x) && (y1) <= (y) && (x1) + (w1) > (x) && (y1) + (h1) > (y))

window_t *find_window(int x, int y) {
  window_t *last = NULL;
  for (window_t *win = windows; win; win = win->next) {
    if (!win->visible) continue;
    int t = win->flags & WINDOW_NOTITLE ? 0 : TITLEBAR_HEIGHT;
    if (CONTAINS(x, y, win->frame.x, win->frame.y-t, win->frame.w, win->frame.h+t)) {
      last = win;
      send_message(win, WM_HITTEST, MAKEDWORD(x - win->frame.x, y - win->frame.y), &last);
    }
  }
  return last;
}

window_t *get_root_window(window_t *window) {
  return window->parent ? get_root_window(window->parent) : window;
}

void track_mouse(window_t *win) {
  if (_tracked == win)
    return;
  if (_tracked) {
    send_message(_tracked, WM_MOUSELEAVE, 0, win);
    invalidate_window(_tracked);
  }
  _tracked = win;
}

void set_capture(window_t *win) {
  _captured = win;
}

void set_focus(window_t* win) {
  if (win == _focused)
    return;
  if (_focused) {
    _focused->editing = false;
    post_message(_focused, WM_KILLFOCUS, 0, win);
    invalidate_window(_focused);
  }
  if (win) {
    post_message(win, WM_SETFOCUS, 0, _focused);
    invalidate_window(win);
  }
  _focused = win;
}

static void move_to_top(window_t* _win) {
  window_t *win = get_root_window(_win);
  post_message(win, WM_REFRESHSTENCIL, 0, NULL);
  invalidate_window(win);
  
  if (win->flags&WINDOW_ALWAYSINBACK)
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

window_t* find_next_tab_stop(window_t *win, bool allow_current) {
  if (!win) return false;
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

int get_message(SDL_Event *evt) {
  return SDL_PollEvent(evt);
}

void repost_messages(void) {
  for (uint8_t write = queue.write; queue.read != write;) {
    msg_t *m = &queue.messages[queue.read++];
    if (m->target == NULL) continue;
    if (m->msg == WM_REFRESHSTENCIL) {
      repaint_stencil();
      continue;
    }
    send_message(m->target, m->msg, m->wparam, m->lparam);
  }
  glFlush();
  // SDL_GL_SwapWindow(window);
}

void draw_windows(bool rich) {
  for (window_t *win = windows; win; win = win->next) {
    invalidate_window(win);
  }
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
}

int window_title_bar_y(window_t const *win) {
  return win->frame.y + 2 - TITLEBAR_HEIGHT;
}

void draw_window_controls(window_t *win) {
  rect_t r = win->frame;
  fill_rect(COLOR_PANEL_DARK_BG, r.x, r.y-TITLEBAR_HEIGHT, r.w, TITLEBAR_HEIGHT);
//  draw_bevel(MAKERECT(r.x+1, r.y+1-TITLEBAR_HEIGHT, r.w-2, TITLEBAR_HEIGHT-2));
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  // draw_button(frame->x+frame->w-11, frame->y-TITLEBAR_HEIGHT+1, 10, 10, false);
  // draw_button(frame->x+frame->w-11-12, frame->y-TITLEBAR_HEIGHT+1, 10, 10, false);
  // draw_rect(icon3_tex, frame->x+2, frame->y+2-TITLEBAR_HEIGHT, 8, 8);
  for (int i = 0; i < 1; i++) {
    int x = win->frame.x + win->frame.w - (i+1)*CONTROL_BUTTON_WIDTH - CONTROL_BUTTON_PADDING;
    int y = window_title_bar_y(win);
    draw_icon8(icon8_minus + i, x, y, COLOR_TEXT_NORMAL);
  }
}

int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (!win) return false;
  rect_t const *frame = &win->frame;
  window_t *root = get_root_window(win);
  int value = 0;
  if (win) {
    for (winhook_t *hook = g_hooks; hook; hook = hook->next) {
      if (msg == hook->msg) {
        hook->func(win, msg, wparam, lparam, hook->userdata);
      }
    }
    switch (msg) {
      case WM_NCPAINT:
        glStencilFunc(GL_EQUAL, win->id, 0xFF);
        set_viewport(&(window_t){0, 0, screen_width, screen_height});
        set_projection(0, 0, screen_width, screen_height);
        if (!(win->flags&WINDOW_TRANSPARENT)) {
          draw_panel(win);
        }
        if (!(win->flags&WINDOW_NOTITLE)) {
          draw_window_controls(win);
        }
        if (!(win->flags&WINDOW_NOTITLE)) {
          // fill_rect(0x40ffffff, frame->x+frame->w-TITLEBAR_HEIGHT, frame->y-TITLEBAR_HEIGHT, TITLEBAR_HEIGHT, TITLEBAR_HEIGHT);
          draw_text_small(win->title, frame->x+2, window_title_bar_y(win), -1);
        }
        break;
      case WM_PAINT:
        glStencilFunc(GL_EQUAL, get_root_window(win)->id, 0xFF);
        set_viewport(root);
        set_projection(root->scroll[0],
                       root->scroll[1],
                       root->frame.w + root->scroll[0],
                       root->frame.h + root->scroll[1]);
        break;
    }
    if (!(value = win->proc(win, msg, wparam, lparam))) {
      switch (msg) {
        case WM_PAINT:
          for (window_t *sub = win->children; sub; sub = sub->next) {
            sub->proc(sub, WM_PAINT, wparam, lparam);
          }
          break;
        case WM_WHEEL:
          if (win->flags & WINDOW_HSCROLL) {
            win->scroll[0] = MIN(0, (int)win->scroll[0]+(int16_t)LOWORD(wparam));
          }
          if (win->flags & WINDOW_VSCROLL) {
            win->scroll[1] = MAX(0, (int)win->scroll[1]-(int16_t)HIWORD(wparam));
          }
          if (win->flags & (WINDOW_VSCROLL|WINDOW_HSCROLL)) {
            invalidate_window(win);
          }
          break;
        case WM_PAINTSTENCIL:
          paint_window_stencil(win);
          break;
        case WM_HITTEST:
          for (window_t *item = win->children; item; item = item->next) {
            rect_t r = item->frame;
            uint16_t x = LOWORD(wparam), y = HIWORD(wparam);
            if (!item->notabstop && CONTAINS(x, y, r.x, r.y, r.w, r.h)) {
              *(window_t **)lparam = item;
            }
          }
          break;
      }
    }
    if (win->disabled && msg == WM_PAINT) {
      uint32_t col = (COLOR_PANEL_BG & 0x00FFFFFF) | 0x80000000;
      set_viewport(&(window_t){0, 0, screen_width, screen_height});
      set_projection(0, 0, screen_width, screen_height);
      fill_rect(col, win->frame.x, win->frame.y, win->frame.w, win->frame.h);
    }
  }
  return value;
}

void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  for (uint8_t w = queue.write, r = queue.read; r != w; r++) {
    if (queue.messages[r].target == win &&
        queue.messages[r].msg == msg)
    {
      queue.messages[r].target = NULL;
    }
  }
  queue.messages[queue.write++] = (msg_t) {
    .target = win,
    .msg = msg,
    .wparam = wparam,
    .lparam = lparam,
  };
}

int get_text_width(const char*);

result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
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

#define LIST_HEIGHT BUTTON_HEIGHT
#define LIST_X 3
#define LIST_Y 3

#define MAX_COMBOBOX_STRINGS 256
typedef char combobox_string_t[64];

#define LIST_SELITEM 0x5001

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
      send_message(list, LIST_SELITEM, 2, NULL);
      set_capture(list);
      return true;
    }
    case CB_ADDSTRING:
      if (win->cursor_pos < MAX_COMBOBOX_STRINGS) {
        strncpy(texts[win->cursor_pos++], lparam, sizeof(combobox_string_t));
        strncpy(win->title, lparam, sizeof(win->title));
        return true;
      } else {
        return false;
      }
    case CB_GETLBTEXT:
      if (wparam < win->cursor_pos) {
        strcpy(lparam, texts[wparam]);
        return true;
      } else {
        return false;
      }
    case CB_SETCURSEL:
      if (wparam < win->cursor_pos) {
        strncpy(win->title, texts[wparam], sizeof(win->title));
        return true;
      } else {
        return false;
      }
    case CB_GETCURSEL:
      for (int i = 0; i < win->cursor_pos; i++) {
        if (!strncmp(texts[i], win->title, sizeof(win->title)))
          return i;
      }
      return CB_ERR;
    default:
      return win_button(win, msg, wparam, lparam);
  }
}

result_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+16);
      win->frame.h = MAX(win->frame.h, BUTTON_HEIGHT);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, 14, 14);
      draw_button(win->frame.x, win->frame.y, 10, 10, win->pressed);
      draw_text_small(win->title, win->frame.x + 17, win->frame.y + 3, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x + 16, win->frame.y + 2, COLOR_TEXT_NORMAL);
      if (win->value) {
        draw_icon8(icon8_checkbox, win->frame.x+1, win->frame.y+1, COLOR_TEXT_NORMAL);
      }
      return true;
    case WM_LBUTTONDOWN:
      win->pressed = true;
      invalidate_window(win);
      return true;
    case WM_LBUTTONUP:
      win->pressed = false;
      send_message(win, BM_SETCHECK, !send_message(win, BM_GETCHECK, 0, NULL), NULL);
      send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
      invalidate_window(win);
      return true;
    case BM_SETCHECK:
      win->value = (wparam != BST_UNCHECKED);
      return true;
    case BM_GETCHECK:
      return win->value ? BST_CHECKED : BST_UNCHECKED;
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
        send_message(win, BM_SETCHECK, !send_message(win, BM_GETCHECK, 0, NULL), NULL);
        send_message(get_root_window(win), WM_COMMAND, MAKEDWORD(win->id, BN_CLICKED), win);
        invalidate_window(win);
        return true;
      } else {
        return false;
      }
  }
  return false;
}

#define BUFFER_SIZE 64
#define PADDING 3

result_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
//  bool *pressed = (void *)&win->userdata;
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
  }
  return false;
}

result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title));
      win->notabstop = true;
      return true;
    case WM_PAINT:
      draw_text_small(win->title, win->frame.x+1, win->frame.y+1+PADDING, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x, win->frame.y+PADDING, COLOR_TEXT_NORMAL);
      return true;
  }
  return false;
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
        float scale = fminf(1, fminf(((float)win->frame.w) / spr->width,
                                     ((float)win->frame.h) / spr->height));
        draw_rect(spr->texture,
                  win->frame.x+(win->frame.w-spr->width*scale)/2,
                  win->frame.y+(win->frame.h-spr->height*scale)/2,
                  spr->width * scale,
                  spr->height * scale);
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

result_t win_stack(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  stack_data_t *data = win->userdata;
  switch (msg) {
    case WM_CREATE:
      data = malloc(sizeof(stack_data_t));
      memset(data, 0, sizeof(stack_data_t));
      win->userdata = data;
      return true;
    case WM_DESTROY:
      free(win->userdata);
      return true;
    case WM_NCPAINT:
      for (int i = 0; i < data->num_items; i++) {
        send_message(data->items[i], WM_NCPAINT, 0, NULL);
      }
      return false;
    case WM_PAINT:
      for (int i = 0; i < data->num_items; i++) {
        send_message(data->items[i], WM_PAINT, 0, NULL);
      }
      return true;
    case WM_PAINTSTENCIL:
      paint_window_stencil(win);
      for (int i = 0; i < data->num_items; i++) {
        paint_window_stencil(data->items[i]);
      }
      return true;
    case WM_HITTEST:
      for (int i = 0, y = 1; i < data->num_items; i++) {
        window_t *item = data->items[i];
        int bottom = y + item->frame.h + 2;
        int mx = LOWORD(wparam), my = HIWORD(wparam);
        if (my > y && my < bottom) {
          *((window_t **)lparam) = item;
          uint32_t wparam = MAKEDWORD(mx, my - item->frame.y + win->frame.y);
          send_message(item, WM_HITTEST, wparam, lparam);
        }
        y = bottom;
      }
      return true;
    case ST_ADDWINDOW:
      data->items[data->num_items] = lparam;
      data->items[data->num_items]->flags |= WINDOW_NOTITLE | WINDOW_NORESIZE;
      data->num_items++;
      post_message(win, WM_REFRESHSTENCIL, 0, NULL);
      // fall through
    case WM_RESIZE:
      for (int i = 0, y = win->frame.y + 1; i < data->num_items; i++) {
        window_t *item = data->items[i];
        move_window(item, win->frame.x + 1, y);
        resize_window(item, win->frame.w-2, item->frame.h);
        y += item->frame.h + 2;
      }
      return true;
  }
  return false;
}

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

window_t *get_window_item(window_t const *win, uint32_t id) {
  for (window_t *c = win->children; c; c = c->next) {
    if (c->id == id)
      return c;
  }
  return NULL;
}

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...) {
  char text[256] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  for (window_t *c = win->children; c; c = c->next) {
    if (c->id == id) {
      strncpy(c->title, text, sizeof(c->title));
    }
  }
}

void load_window_children(window_t *win, windef_t const *def) {
  int x = WINDOW_PADDING;
  int y = WINDOW_PADDING;
  for (; def->classname; def++) {
    int w = def->w == -1 ? win->frame.w - WINDOW_PADDING*2 : def->w;
    int h = def->h == 0 ? CONTROL_HEIGHT : def->h;
    if (x + w > win->frame.w - WINDOW_PADDING || !strcmp(def->classname, "SPACE")) {
      x = WINDOW_PADDING;
      for (window_t *child = win->children; child; child = child->next) {
        y = MAX(y, child->frame.y + child->frame.h);
      }
      y += LINE_PADDING;
    }
    if (!strcmp(def->classname, "SPACE"))
      continue;
    window_t *item = create_window2(def, MAKERECT(x, y, w, h), win);
    if (item) {
      x += item->frame.w + LINE_PADDING;
    }
  }
}

void show_window(window_t *win, bool visible) {
  post_message(win, WM_REFRESHSTENCIL, 0, NULL);
  if (!visible) {
    invalidate_overlaps(win);
    if (_focused == win) set_focus(NULL);
    if (_captured == win) set_capture(NULL);
    if (_tracked == win) track_mouse(NULL);
  } else {
    move_to_top(win);
    set_focus(win);
  }
  win->visible = visible;
  post_message(win, WM_SHOWWINDOW, visible, NULL);
}

bool is_window(window_t *win) {
  for (window_t *it = windows; it; it = it->next) {
    if (it == win) {
      return true;
    }
  }
  return false;
}

static uint32_t _return_code;
void end_dialog(window_t *win, uint32_t code) {
  _return_code = code;
  destroy_window(win);
}

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

void enable_window(window_t *win, bool enable) {
  if (!enable && _focused == win) {
    set_focus(NULL);
  }
  win->disabled = !enable;
  invalidate_window(win);
}
