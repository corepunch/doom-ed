#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "sprites.h"
#include "editor.h"

// Base UI Colors
#define COLOR_PANEL_BG       0xff3c3c3c  // main panel or window background
#define COLOR_PANEL_DARK_BG  0xff2c2c2c  // main panel or window background
#define COLOR_LIGHT_EDGE     0xff7f7f7f  // top-left edge for beveled elements
#define COLOR_DARK_EDGE      0xff1a1a1a  // bottom-right edge for bevel
#define COLOR_FLARE          0xffcfcfcf  // top-left edge for beveled elements
#define COLOR_FOCUSED        0xff5EC4F3

// Additional UI Colors
#define COLOR_BUTTON_BG      0xff404040  // button background (unpressed)
#define COLOR_BUTTON_INNER   0xff505050  // inner fill of button
#define COLOR_BUTTON_HOVER   0xff5a5a5a  // slightly brighter for hover state
#define COLOR_TEXT_NORMAL    0xffc0c0c0  // standard text color
#define COLOR_TEXT_DISABLED  0xff808080  // for disabled/inactive text
#define COLOR_TEXT_ERROR     0xffff4444  // red text for errors
#define COLOR_TEXT_SUCCESS   0xff44ff44  // green text for success messages
#define COLOR_BORDER_FOCUS   0xff101010  // very dark outline for focused item
#define COLOR_BORDER_ACTIVE  0xff808080  // light gray for active border

// Transparency helpers (if needed)
#define COLOR_TRANSPARENT    0x00000000  // fully transparent

#define MAX_WINDOWS 64

#define RESIZE_HANDLE 8

extern int screen_width, screen_height;

window_t *windows = NULL;
window_t *_focused = NULL;
window_t *_tracked = NULL;

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

void draw_wallpaper(void);

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

void draw_panel(int x, int y, int w, int h, bool active) {
  if (active) {
    fill_rect(COLOR_FOCUSED, x-1, y-1, w+2, h+2);
  } else {
    fill_rect(COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
    fill_rect(COLOR_DARK_EDGE, x, y, w+1, h+1);
    fill_rect(COLOR_FLARE, x-1, y-1, 1, 1);
  }
  fill_rect(COLOR_LIGHT_EDGE, x+w-RESIZE_HANDLE+1, y+h-RESIZE_HANDLE+1, RESIZE_HANDLE, RESIZE_HANDLE);
  fill_rect(COLOR_PANEL_BG, x, y, w, h);
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

static void paint_stencil(uint8_t id) {
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  
  glEnable(GL_STENCIL_TEST);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  for (window_t const *w = windows; w; w = w->next) {
    glStencilFunc(GL_ALWAYS, w->id, 0xFF);            // Always pass
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // Replace stencil with window ID
    int p = 1;
    int t = TITLEBAR_HEIGHT;
    draw_rect(1, w->frame.x-p, w->frame.y-t-p, w->frame.w+p*2, w->frame.h+t+p*2);
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, id, 0xFF);
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
              const rect_t *frame,
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
  proc(win, WM_CREATE, 0, lparam);
  invalidate_window(win);
  return win;
}

void destroy_window(window_t *win) {
  send_message(win, WM_DESTROY, 0, NULL);
  if (win == windows) {
    windows = win->next;
  } else {
    for (window_t *w = windows->next, *p = windows;
         w; w = w->next, p = p->next)
    {
      if (w == win) {
        p->next = win->next;
        break;
      }
    }
  }
  for (uint8_t w = queue.write, r = queue.read; r != w; r++) {
    if (queue.messages[r].target == win) {
      queue.messages[r].target = NULL;
    }
  }
  free(win);
}

#define CONTAINS(x, y, x1, y1, w1, h1) \
((x1) <= (x) && (y1) <= (y) && (x1) + (w1) > (x) && (y1) + (h1) > (y))

window_t *find_window(int x, int y) {
  window_t *last = NULL;
  for (window_t *win = windows; win; win = win->next) {
    if (CONTAINS(x, y, win->frame.x, win->frame.y-TITLEBAR_HEIGHT, win->frame.w, win->frame.h+TITLEBAR_HEIGHT)) {
      last = win;
    }
    for (window_t *item = win->children; item; item = item->next) {
      if (!item->notabstop &&
          CONTAINS(x, y,
                   win->frame.x + item->frame.x,
                   win->frame.y + item->frame.y,
                   item->frame.w,
                   item->frame.h))
      {
        last = item;
      }
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

static void set_focus(window_t* win) {
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
  invalidate_window(win);
  
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

static window_t *dragging = NULL;
static window_t *resizing = NULL;
static int drag_anchor[2];

bool do_windows_overlap(const window_t *a, const window_t *b) {
  return a && b &&
  a->frame.x < b->frame.x + b->frame.w && a->frame.x + a->frame.w > b->frame.x &&
  a->frame.y < b->frame.y + b->frame.h && a->frame.y + a->frame.h > b->frame.y;
}

void move_window(window_t *win, int x, int y) {
  for (window_t *t = windows; t; t = t->next) {
    if (t != win && do_windows_overlap(t, win)) {
      invalidate_window(t);
    }
  }
  win->frame.x = x;
  win->frame.y = y;
  
  paint_stencil(0);
  draw_wallpaper();
  glDisable(GL_STENCIL_TEST);
  
  invalidate_window(win);
}

void resize_window(window_t *win, int new_w, int new_h) {
  for (window_t *t = windows; t; t = t->next) {
    if (t != win && do_windows_overlap(t, win)) {
      invalidate_window(t);
    }
  }
  if (new_w > 0) resizing->frame.w = new_w;
  if (new_h > 0) resizing->frame.h = new_h;
  
  paint_stencil(0);
  draw_wallpaper();
  glDisable(GL_STENCIL_TEST);
  
  invalidate_window(win);
  
  post_message(win, WM_RESIZE, 0, NULL);
}

#define LOCAL_X(VALUE, WIN) (SCALE_POINT((VALUE).x) - (WIN)->frame.x + (WIN)->scroll[0])
#define LOCAL_Y(VALUE, WIN) (SCALE_POINT((VALUE).y) - (WIN)->frame.y + (WIN)->scroll[1])

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
  if (!win->notabstop && allow_current) return win;
  if ((next = find_next_tab_stop(win->next, true))) return next;
  return allow_current ? NULL : find_next_tab_stop(win->parent, false);
}

window_t* find_prev_tab_stop(window_t* win) {
  window_t *it = (win = (win->parent ? win : find_next_tab_stop(win, false)));
  for (window_t *next = find_next_tab_stop(it, false);
       next != win;
       it = next, next = find_next_tab_stop(next, false));
  return it;
}

void handle_windows(void) {
  extern bool running;
  SDL_Event event;
  window_t *win;
  
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_TEXTINPUT:
        if (!_focused) {
          // skip
        } else {
          send_message(_focused, WM_TEXTINPUT, 0, event.text.text);
        }
        break;
      case SDL_KEYDOWN:
        if (!_focused) {
          // skip
        } else if (!send_message(_focused, WM_KEYDOWN, event.key.keysym.scancode, NULL)) {
          switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_TAB:
              if (event.key.keysym.mod & KMOD_SHIFT) {
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
        send_message(_focused, WM_KEYUP, event.key.keysym.scancode, NULL);
        break;
      case SDL_JOYAXISMOTION:
        send_message(_focused, WM_JOYAXISMOTION, MAKEDWORD(event.jaxis.axis, event.jaxis.value), NULL);
        break;
      case SDL_JOYBUTTONDOWN:
        send_message(_focused, WM_JOYBUTTONDOWN, event.jbutton.button, NULL);
        break;
      case SDL_MOUSEMOTION:
        if (dragging) {
          move_window(dragging,
                      SCALE_POINT(event.motion.x) - drag_anchor[0],
                      SCALE_POINT(event.motion.y) - drag_anchor[1]);
        } else if (resizing) {
          int new_w = SCALE_POINT(event.motion.x) - resizing->frame.x;
          int new_h = SCALE_POINT(event.motion.y) - resizing->frame.y;
          resize_window(resizing, new_w, new_h);
        } else if ((SDL_GetRelativeMouseMode() && (win = _focused))||
                   (win = find_window(SCALE_POINT(event.motion.x),
                                      SCALE_POINT(event.motion.y))))
        {
          int16_t x = LOCAL_X(event.motion, win);
          int16_t y = LOCAL_Y(event.motion, win);
          int16_t dx = event.motion.xrel;
          int16_t dy = event.motion.yrel;
          send_message(win, WM_MOUSEMOVE, MAKEDWORD(x, y), (void*)(intptr_t)MAKEDWORD(dx, dy));
        }
        if (_tracked && !CONTAINS(SCALE_POINT(event.motion.x),
                                  SCALE_POINT(event.motion.y),
                                  _tracked->frame.x, _tracked->frame.y,
                                  _tracked->frame.w, _tracked->frame.h))
        {
          track_mouse(NULL);
        }
        break;
      case SDL_MOUSEWHEEL:
        if ((win = find_window(SCALE_POINT(event.wheel.mouseX), SCALE_POINT(event.wheel.mouseY)))) {
          send_message(win, WM_WHEEL, MAKEDWORD(-event.wheel.x * SCROLL_SENSITIVITY, event.wheel.y * SCROLL_SENSITIVITY), NULL);
        }
        break;
        
      case SDL_MOUSEBUTTONDOWN:
        if ((SDL_GetRelativeMouseMode() && (win = _focused)) ||
            (win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y))))
        {
          //        if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          set_focus(win);
          move_to_top(win);
          int x = LOCAL_X(event.button, win);
          int y = LOCAL_Y(event.button, win);
          if (x >= win->frame.w - RESIZE_HANDLE && y >= win->frame.h - RESIZE_HANDLE && !win->parent) {
            resizing = win;
          } else if (SCALE_POINT(event.button.y) < win->frame.y && !win->parent) {
            dragging = win;
            drag_anchor[0] = SCALE_POINT(event.button.x) - win->frame.x;
            drag_anchor[1] = SCALE_POINT(event.button.y) - win->frame.y;
          } else if (win == _focused) {
            int msg = 0;
            switch (event.button.button) {
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
        if (dragging) {
          int x = SCALE_POINT(event.button.x);
          int y = SCALE_POINT(event.button.y);
          switch (event.button.button) {
            case 1: send_message(dragging, WM_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
              //              case 3: send_message(win, WM_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
          }
          set_focus(dragging);
          dragging = NULL;
        } else if (resizing) {
          set_focus(resizing);
          resizing = NULL;
        } else if ((SDL_GetRelativeMouseMode() && (win = _focused))||
                   (win = find_window(SCALE_POINT(event.button.x),
                                      SCALE_POINT(event.button.y)))) {
          //        } else if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          if (SCALE_POINT(event.button.y) >= win->frame.y) {
            int x = LOCAL_X(event.button, win);
            int y = LOCAL_Y(event.button, win);
//            switch (event.button.button) {
//              case 1: send_message(win, WM_LBUTTONUP, MAKEDWORD(x, y), NULL); break;
//              case 3: send_message(win, WM_RBUTTONUP, MAKEDWORD(x, y), NULL); break;
//            }
            int msg = 0;
            switch (event.button.button) {
              case 1: msg = WM_LBUTTONUP; break;
              case 3: msg = WM_RBUTTONUP; break;
            }
            if (!handle_mouse(msg, win, x, y)) {
              send_message(win, msg, MAKEDWORD(x, y), NULL);
            }
          } else {
            int x = SCALE_POINT(event.button.x);
            int y = SCALE_POINT(event.button.y);
            switch (event.button.button) {
              case 1: send_message(win, WM_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
//              case 3: send_message(win, WM_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
            }
          }
        }
        break;
    }
  }
  for (uint8_t write = queue.write; queue.read != write;) {
    msg_t *m = &queue.messages[queue.read++];
    if (m->target == NULL) continue;
//    if (m->msg == WM_PAINT || m->msg == WM_NCPAINT) {
    send_message(m->target, m->msg, m->wparam, m->lparam);
//    }
  }
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
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  // draw_button(frame->x+frame->w-11, frame->y-TITLEBAR_HEIGHT+1, 10, 10, false);
  // draw_button(frame->x+frame->w-11-12, frame->y-TITLEBAR_HEIGHT+1, 10, 10, false);
  // draw_rect(icon3_tex, frame->x+2, frame->y+2-TITLEBAR_HEIGHT, 8, 8);
  for (int i = 0; i < 2; i++) {
    draw_icon(icon_collapse + i,
              win->frame.x + win->frame.w - (i+1)*8-2,
              window_title_bar_y(win),
              0.5f);
  }
}

int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  rect_t const *frame = &win->frame;
  window_t *root = get_root_window(win);
  int value = 0;
  if (win) {
    switch (msg) {
      case WM_NCPAINT:
        paint_stencil(win->id);
        // set_viewport(&(window_t){0, 0, screen_width, screen_height});
        // set_projection(0, 0, screen_width, screen_height);
        if (!(win->flags&WINDOW_TRANSPARENT)) {
          draw_panel(frame->x, frame->y-TITLEBAR_HEIGHT, frame->w, frame->h+TITLEBAR_HEIGHT, _focused == win);
        }
        draw_window_controls(win);
        break;
      case WM_PAINT:
        paint_stencil(root->id);
        set_viewport(root);
        set_projection(root->scroll[0],
                       root->scroll[1],
                       root->frame.w + root->scroll[0],
                       root->frame.h + root->scroll[1]);
        break;
    }
    value = win->proc(win, msg, wparam, lparam);
    if (!value) {
      switch (msg) {
        case WM_PAINT:
          for (window_t *sub = win->children; sub; sub = sub->next) {
            sub->proc(sub, WM_PAINT, wparam, lparam);
          }
          break;
        case WM_NCPAINT:
          if (!(win->flags&WINDOW_NOTITLE)) {
            fill_rect(0x40000000, frame->x, frame->y-TITLEBAR_HEIGHT, frame->w, TITLEBAR_HEIGHT);
            // fill_rect(0x40ffffff, frame->x+frame->w-TITLEBAR_HEIGHT, frame->y-TITLEBAR_HEIGHT, TITLEBAR_HEIGHT, TITLEBAR_HEIGHT);
            draw_text_gl3(win->title, frame->x+2, window_title_bar_y(win)-1, 1);
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
      }
    }
  }
  if (msg == WM_PAINT || msg == WM_NCPAINT) {
    glDisable(GL_STENCIL_TEST);
  }
  return value;
}

void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  for (uint8_t w = queue.write, r = queue.read; r != w; r++) {
    if (queue.messages[r].target == win && queue.messages[r].msg == msg) {
      queue.messages[r].wparam = wparam;
      queue.messages[r].lparam = lparam;
      return;
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

bool win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+6);
      win->frame.h = MAX(win->frame.h, 13);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, win->frame.w+4, win->frame.h+4);
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.h, win->pressed);
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

bool win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->frame.w = MAX(win->frame.w, strwidth(win->title)+6);
      win->frame.h = MAX(win->frame.h, 13);
      return true;
    case WM_PAINT:
      fill_rect(_focused == win?COLOR_FOCUSED:COLOR_PANEL_BG, win->frame.x-2, win->frame.y-2, 14, 14);
      draw_button(win->frame.x, win->frame.y, 10, 10, win->pressed);
      draw_text_small(win->title, win->frame.x + 17, win->frame.y + 2, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x + 16, win->frame.y + 1, COLOR_TEXT_NORMAL);
      if (win->value) {
        draw_icon(icon_checkbox, win->frame.x+1, win->frame.y+1, 1);
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

bool win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
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
//    case WM_LBUTTONDOWN:
//      *pressed = true;
//      invalidate_window(win);
//      return true;
    case WM_LBUTTONUP:
//      *pressed = false;
      set_focus(win);
      win->editing = true;
      win->cursor_pos = 0;
      for (int i = 0; i <= strlen(win->title); i++) {
        int x1 = win->frame.x+PADDING+strnwidth(win->title, i);
        int x2 = win->frame.x+PADDING+strnwidth(win->title, win->cursor_pos);
        if (abs((int)LOWORD(wparam) - x1) < abs((int)LOWORD(wparam) - x2)) {
          win->cursor_pos = i;
        }
      }
//      invalidate_window(win);
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

bool win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      win->notabstop = true;
      return true;
    case WM_PAINT:
      draw_text_small(win->title, win->frame.x+1, win->frame.y+1, COLOR_DARK_EDGE);
      draw_text_small(win->title, win->frame.x, win->frame.y, COLOR_TEXT_NORMAL);
      return true;
  }
  return false;
}

bool win_sprite(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
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
      } else if ((tex = get_flat_texture(win->title))) {
        float scale = fminf(1, fminf(((float)win->frame.w) / tex->width,
                                     ((float)win->frame.h) / tex->height));
        draw_rect(tex->texture,
                  win->frame.x+(win->frame.w-tex->width*scale)/2,
                  win->frame.y+(win->frame.h-tex->height*scale)/2,
                  tex->width * scale,
                  tex->height * scale);
      }
      return true;
  }
  return false;
}

window_t *create_window2(windef_t const *def, window_t *parent) {
  bool (*proc)(window_t *, uint32_t, uint32_t, void *);
  if (!strcmp(def->classname, "TEXT")) {
    proc = win_label;
  } else if (!strcmp(def->classname, "BUTTON")) {
    proc = win_button;
  } else if (!strcmp(def->classname, "CHECKBOX")) {
    proc = win_checkbox;
  } else if (!strcmp(def->classname, "EDITTEXT")) {
    proc = win_textedit;
  } else if (!strcmp(def->classname, "SPRITE")) {
    proc = win_sprite;
  } else {
    return NULL;
  }
  window_t *win = create_window(def->text,
                                def->flags,
                                MAKERECT(def->x, def->y, def->w, def->h),
                                parent, proc, NULL);
  win->id = def->id;
  return NULL;
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
  for (; def->classname; def++) {
    create_window2(def, win);
  }
}
