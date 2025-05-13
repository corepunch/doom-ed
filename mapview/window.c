#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "sprites.h"

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
#define TITLEBAR_HEIGHT 12

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

void draw_panel(int x, int y, int w, int h) {
  fill_rect(COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
  fill_rect(COLOR_DARK_EDGE, x, y, w+1, h+1);
  fill_rect(COLOR_LIGHT_EDGE, x+w-RESIZE_HANDLE+1, y+h-RESIZE_HANDLE+1, RESIZE_HANDLE, RESIZE_HANDLE);
  fill_rect(COLOR_PANEL_BG, x, y, w, h);
}

void invalidate_window(window_t *win) {
  post_message(win, MSG_NCPAINT, 0, NULL);
  post_message(win, MSG_PAINT, 0, NULL);
}

void create_window(int x, int y, int w, int h, char const *title, uint32_t flags, winproc_t proc, void *lparam) {
  window_t *win = malloc(sizeof(window_t));
  memset(win, 0, sizeof(window_t));
  win->x = x;
  win->y = y;
  win->w = w;
  win->h = h;
  win->proc = proc;
  win->flags = flags;
  strncpy(win->title, title, sizeof(win->title));
  if (!windows) {
    windows = win;
  } else {
    window_t *p= windows;
    while (p->next) p = p->next;
    p->next = win;
  }
  active = win;
  proc(win, MSG_CREATE, 0, lparam);
  invalidate_window(win);
}

void set_viewport(window_t const *win) {
  extern SDL_Window *window;
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);

  float scale_x = (float)w / screen_width;
  float scale_y = (float)h / screen_height;
  
  int vp_x = (int)(win->x * scale_x);
  int vp_y = (int)((screen_height - win->y - win->h) * scale_y); // flip Y
  int vp_w = (int)(win->w * scale_x);
  int vp_h = (int)(win->h * scale_y);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(vp_x, vp_y, vp_w, vp_h);
  glScissor(vp_x, vp_y, vp_w, vp_h);
}

#define CONTAINS(x, y, x1, y1, w1, h1) \
((x1) <= (x) && (y1) <= (y) && (x1) + (w1) > (x) && (y1) + (h1) > (y))

window_t *find_window(int x, int y) {
  window_t *last = NULL;
  for (window_t *win = windows; win; win = win->next) {
    if CONTAINS(x, y, win->x, win->y-TITLEBAR_HEIGHT, win->w, win->h+TITLEBAR_HEIGHT) {
      last = win;
    }
  }
  return last;
}

static void move_to_top(window_t* win) {
  if (active) {
    invalidate_window(active);
  }

  active = win;
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
  a->x < b->x + b->w && a->x + a->w > b->x &&
  a->y < b->y + b->h && a->y + a->h > b->y;
}

void move_window(window_t *win, int x, int y) {
  win->x = x;
  win->y = y;
  
  for (window_t *t = windows; t; t = t->next) {
    if (t != win && do_windows_overlap(t, win)) {
      invalidate_window(t);
    }
  }
  
  invalidate_window(win);
}

void resize_window(window_t *win, int new_w, int new_h) {
  if (new_w > 0) resizing->w = new_w;
  if (new_h > 0) resizing->h = new_h;
  invalidate_window(win);
  post_message(win, MSG_RESIZE, 0, NULL);
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
          int new_w = SCALE_POINT(event.motion.x) - resizing->x;
          int new_h = SCALE_POINT(event.motion.y) - resizing->y;
          resize_window(resizing, new_w, new_h);
        } else if ((SDL_GetRelativeMouseMode() && (win = active))||
                   (win = find_window(SCALE_POINT(event.motion.x),
                                      SCALE_POINT(event.motion.y))))
        {
          int16_t x = SCALE_POINT(event.motion.x) - win->x;
          int16_t y = SCALE_POINT(event.motion.y) - win->y;
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
        if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          int x = SCALE_POINT(event.button.x) - win->x;
          int y = SCALE_POINT(event.button.y) - win->y;
          if (x >= win->w - RESIZE_HANDLE && y >= win->h - RESIZE_HANDLE) {
            resizing = win;
          } else if (SCALE_POINT(event.button.y) < win->y) {
            dragging = win;
            drag_anchor[0] = SCALE_POINT(event.button.x) - win->x;
            drag_anchor[1] = SCALE_POINT(event.button.y) - win->y;
          } else if (win == active) {
            switch (event.button.button) {
              case 1: send_message(win, MSG_LBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
              case 3: send_message(win, MSG_RBUTTONDOWN, MAKEDWORD(x, y), NULL); break;
            }
          }
        }
        break;
        
      case SDL_MOUSEBUTTONUP:
        if (dragging) {
          move_to_top(dragging);
          dragging = NULL;
        } else if (resizing) {
          move_to_top(resizing);
          resizing = NULL;
        } else if ((win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y)))) {
          if (win != active) {
            move_to_top(win);
          } else if (SCALE_POINT(event.button.y) >= win->y) {
            int x = SCALE_POINT(event.button.x) - win->x;
            int y = SCALE_POINT(event.button.y) - win->y;
            switch (event.button.button) {
              case 1: send_message(win, MSG_LBUTTONUP, MAKEDWORD(x, y), NULL); break;
              case 3: send_message(win, MSG_RBUTTONUP, MAKEDWORD(x, y), NULL); break;
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
    if ((win->flags & WINDOW_RICH) && !rich) {
      continue;
    }
//    send_message(win, MSG_PAINT, 0, NULL);
    invalidate_window(win);
  }
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(screen_width, screen_height);
}

void send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (win) {
    switch (msg) {
      case MSG_NCPAINT:
        set_viewport(&(window_t){0, 0, screen_width, screen_height});
        set_projection(screen_width, screen_height);
        break;
      case MSG_PAINT:
        set_viewport(win);
        set_projection(win->w, win->h);
        break;
    }
    if (win->proc(win, msg, wparam, lparam))
      return;
    switch (msg) {
      case MSG_NCPAINT:
        if (!(win->flags&WINDOW_TRANSPARENT)) {
          if (active == win) {
            fill_rect(COLOR_FOCUSED, win->x-2, win->y-TITLEBAR_HEIGHT-2, win->w+4, win->h+TITLEBAR_HEIGHT+4);
          }
          draw_panel(win->x, win->y-TITLEBAR_HEIGHT, win->w, win->h+TITLEBAR_HEIGHT);
        }
        if (!(win->flags&WINDOW_NOTITLE)) {
          fill_rect(0x40000000, win->x, win->y-TITLEBAR_HEIGHT, win->w, TITLEBAR_HEIGHT);
          //      fill_rect(0x40ffffff, win->x+win->w-TITLEBAR_HEIGHT, win->y-TITLEBAR_HEIGHT, TITLEBAR_HEIGHT, TITLEBAR_HEIGHT);
          draw_text_gl3(win->title, win->x+4, win->y+1-TITLEBAR_HEIGHT, 1);
        }
        break;
    }
  }
}

void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  queue.messages[queue.write++] = (msg_t) {
    .target = win,
    .msg = msg,
    .wparam = wparam,
    .lparam = lparam,
  };
}
