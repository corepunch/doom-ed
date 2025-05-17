#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "sprites.h"
#include "editor.h"

// Base UI Colors
#define COLOR_PANEL_BG       0xff3c3c3c  // main panel or window background
#define COLOR_LIGHT_EDGE     0xff7f7f7f  // top-left edge for beveled elements
#define COLOR_DARK_EDGE      0xff1a1a1a  // bottom-right edge for bevel
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
window_t *active = NULL;

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
  fill_rect(COLOR_PANEL_BG, x, y, w, h);
}

void draw_panel(int x, int y, int w, int h, bool active) {
  if (active) {
    fill_rect(COLOR_FOCUSED, x-1, y-1, w+2, h+2);
  } else {
    fill_rect(COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
    fill_rect(COLOR_DARK_EDGE, x, y, w+1, h+1);
  }
  fill_rect(COLOR_LIGHT_EDGE, x+w-RESIZE_HANDLE+1, y+h-RESIZE_HANDLE+1, RESIZE_HANDLE, RESIZE_HANDLE);
  fill_rect(COLOR_PANEL_BG, x, y, w, h);
}

void invalidate_window(window_t *win) {
  if (!win->parent) {
    post_message(win, MSG_NCPAINT, 0, NULL);
  }
  post_message(win, MSG_PAINT, 0, NULL);
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
  
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glEnable(GL_STENCIL_TEST);
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

static uint8_t _id;

void push_window(window_t *win, window_t **windows)  {
  if (!*windows) {
    *windows = win;
  } else {
    window_t *p = *windows;
    while (p->next) p = p->next;
    p->next = win;
  }
}

void create_window(char const *title,
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
  win->id = parent ? ++parent->child_id : ++_id;
  win->parent = parent;
  strncpy(win->title, title, sizeof(win->title));
  active = win;
  push_window(win, parent ? &parent->children : &windows);
  proc(win, MSG_CREATE, 0, lparam);
  invalidate_window(win);
}

#define CONTAINS(x, y, x1, y1, w1, h1) \
((x1) <= (x) && (y1) <= (y) && (x1) + (w1) > (x) && (y1) + (h1) > (y))

window_t *find_window(int x, int y) {
  window_t *last = NULL;
  for (window_t *win = windows; win; win = win->next) {
    if CONTAINS(x, y, win->frame.x, win->frame.y-TITLEBAR_HEIGHT, win->frame.w, win->frame.h+TITLEBAR_HEIGHT) {
      last = win;
    }
  }
  return last;
}

static void active_window(window_t* win) {
  if (active) {
    post_message(active, MSG_KILLFOCUS, 0, win);
    invalidate_window(active);
  }
  if (win) {
    post_message(active, MSG_SETFOCUS, 0, active);
    invalidate_window(win);
  }
  active = win;
}

static void move_to_top(window_t* win) {
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
  
  post_message(win, MSG_RESIZE, 0, NULL);
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

void handle_windows(void) {
  extern bool running;
  SDL_Event event;
  window_t *win;
  
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
        send_message(active, MSG_KEYDOWN, event.key.keysym.scancode, NULL);
        break;
      case SDL_KEYUP:
        send_message(active, MSG_KEYUP, event.key.keysym.scancode, NULL);
        break;
      case SDL_JOYAXISMOTION:
        send_message(active, MSG_JOYAXISMOTION, MAKEDWORD(event.jaxis.axis, event.jaxis.value), NULL);
        break;
      case SDL_JOYBUTTONDOWN:
        send_message(active, MSG_JOYBUTTONDOWN, event.jbutton.button, NULL);
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
        } else if ((SDL_GetRelativeMouseMode() && (win = active))||
                   (win = find_window(SCALE_POINT(event.motion.x),
                                      SCALE_POINT(event.motion.y))))
        {
          int16_t x = LOCAL_X(event.motion, win);
          int16_t y = LOCAL_Y(event.motion, win);
          int16_t dx = event.motion.xrel;
          int16_t dy = event.motion.yrel;
          send_message(win, MSG_MOUSEMOVE, MAKEDWORD(x, y), (void*)(intptr_t)MAKEDWORD(dx, dy));
        }
        break;
        
      case SDL_MOUSEWHEEL:
        if ((win = find_window(SCALE_POINT(event.wheel.mouseX), SCALE_POINT(event.wheel.mouseY)))) {
          send_message(win, MSG_WHEEL, MAKEDWORD(-event.wheel.x * SCROLL_SENSITIVITY, event.wheel.y * SCROLL_SENSITIVITY), NULL);
        }
        break;
        
      case SDL_MOUSEBUTTONDOWN:
        if ((SDL_GetRelativeMouseMode() && (win = active))||
            (win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          //        if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          move_to_top(win);
          int x = LOCAL_X(event.button, win);
          int y = LOCAL_Y(event.button, win);
          if (x >= win->frame.w - RESIZE_HANDLE && y >= win->frame.h - RESIZE_HANDLE) {
            resizing = win;
          } else if (SCALE_POINT(event.button.y) < win->frame.y) {
            dragging = win;
            drag_anchor[0] = SCALE_POINT(event.button.x) - win->frame.x;
            drag_anchor[1] = SCALE_POINT(event.button.y) - win->frame.y;
          } else if (win == active) {
            int msg = 0;
            switch (event.button.button) {
              case 1: msg = MSG_LBUTTONDOWN; break;
              case 3: msg = MSG_RBUTTONDOWN; break;
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
            case 1: send_message(dragging, MSG_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
              //              case 3: send_message(win, MSG_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
          }
          active_window(dragging);
          dragging = NULL;
        } else if (resizing) {
          active_window(resizing);
          resizing = NULL;
        } else if ((SDL_GetRelativeMouseMode() && (win = active))||
                   (win = find_window(SCALE_POINT(event.button.x),
                                      SCALE_POINT(event.button.y)))) {
          //        } else if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          if (win != active) {
            active_window(win);
          } else if (SCALE_POINT(event.button.y) >= win->frame.y) {
            int x = LOCAL_X(event.button, win);
            int y = LOCAL_Y(event.button, win);
//            switch (event.button.button) {
//              case 1: send_message(win, MSG_LBUTTONUP, MAKEDWORD(x, y), NULL); break;
//              case 3: send_message(win, MSG_RBUTTONUP, MAKEDWORD(x, y), NULL); break;
//            }
            int msg = 0;
            switch (event.button.button) {
              case 1: msg = MSG_LBUTTONUP; break;
              case 3: msg = MSG_RBUTTONUP; break;
            }
            if (!handle_mouse(msg, win, x, y)) {
              send_message(win, msg, MAKEDWORD(x, y), NULL);
            }
          } else {
            int x = SCALE_POINT(event.button.x);
            int y = SCALE_POINT(event.button.y);
            switch (event.button.button) {
              case 1: send_message(win, MSG_NCLBUTTONUP, MAKEDWORD(x, y), NULL); break;
//              case 3: send_message(win, MSG_NCRBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
            }
          }
        }
        break;
    }
  }
  for (uint8_t write = queue.write; queue.read != write; queue.read++) {
    msg_t *m = &queue.messages[queue.read];
    send_message(m->target, m->msg, m->wparam, m->lparam);
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

window_t *get_root_window(window_t *window) {
  return window->parent ? get_root_window(window->parent) : window;
}

void send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  rect_t const *frame = &win->frame;
  window_t *root = get_root_window(win);
  if (win) {
    switch (msg) {
      case MSG_NCPAINT:
        paint_stencil(win->id);
        //        set_viewport(&(window_t){0, 0, screen_width, screen_height});
        //        set_projection(0, 0, screen_width, screen_height);
        if (!(win->flags&WINDOW_TRANSPARENT)) {
          draw_panel(frame->x, frame->y-TITLEBAR_HEIGHT, frame->w, frame->h+TITLEBAR_HEIGHT, active == win);
        }
        draw_window_controls(win);
        break;
      case MSG_PAINT:
        paint_stencil(root->id);
        set_viewport(root);
        set_projection(root->scroll[0],
                       root->scroll[1],
                       root->frame.w + root->scroll[0],
                       root->frame.h + root->scroll[1]);
        break;
    }
    if (!win->proc(win, msg, wparam, lparam)) {
      switch (msg) {
        case MSG_PAINT:
          for (window_t *sub = win->children; sub; sub = sub->next) {
            sub->proc(sub, MSG_PAINT, wparam, lparam);
          }
          break;
        case MSG_NCPAINT:
          if (!(win->flags&WINDOW_NOTITLE)) {
            fill_rect(0x40000000, frame->x, frame->y-TITLEBAR_HEIGHT, frame->w, TITLEBAR_HEIGHT);
            // fill_rect(0x40ffffff, frame->x+frame->w-TITLEBAR_HEIGHT, frame->y-TITLEBAR_HEIGHT, TITLEBAR_HEIGHT, TITLEBAR_HEIGHT);
            draw_text_gl3(win->title, frame->x+2, window_title_bar_y(win)-1, 1);
          }
          break;
        case MSG_WHEEL:
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
  glDisable(GL_STENCIL_TEST);
}

void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  for (uint8_t w = queue.write, r = queue.read + 1; r != w; r++) {
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
  bool *pressed = (void *)&win->userdata;
  switch (msg) {
    case MSG_CREATE:
      win->frame.w = MAX(win->frame.w, get_text_width(win->title)+4);
      win->frame.h = MAX(win->frame.h, 14);
      return true;
    case MSG_PAINT:
      draw_button(win->frame.x, win->frame.y, win->frame.w, win->frame.h, *pressed);
      draw_text_gl3(win->title, win->frame.x+(*pressed?3:2), win->frame.y+(*pressed?4:3), 1);
      return true;
    case MSG_LBUTTONDOWN:
      *pressed = true;
      invalidate_window(win);
      return true;
    case MSG_LBUTTONUP:
      *pressed = false;
      invalidate_window(win);
      return true;
  }
  return false;
}

