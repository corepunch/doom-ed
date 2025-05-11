#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "sprites.h"

// Base UI Colors
#define COLOR_PANEL_BG       0xff3c3c3c  // main panel or window background
#define COLOR_LIGHT_EDGE     0xff7f7f7f  // top-left edge for beveled elements
#define COLOR_DARK_EDGE      0xff1a1a1a  // bottom-right edge for bevel

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

extern int screen_width, screen_height;

window_t *windows = NULL;

void draw_panel(int x, int y, int w, int h) {
  fill_rect(COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
  fill_rect(COLOR_DARK_EDGE, x, y, w+1, h+1);
  fill_rect(COLOR_PANEL_BG, x, y, w, h);
}

void create_window(int x, int y, int w, int h, char const *title, uint32_t flags, winproc_t proc, void *lparam) {
  window_t *win = malloc(sizeof(window_t));
  memset(win, 0, sizeof(window_t));
  win->x = x;
  win->y = y;
  win->w = w;
  win->h = h;
  win->proc = proc;
  win->next = windows;
  win->flags = flags;
  strncpy(win->title, title, sizeof(win->title));
  windows = win;
  proc(win, MSG_CREATE, 0, lparam);
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
  
  glViewport(vp_x, vp_y, vp_w, vp_h);
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

static void moveToTop(window_t** head, window_t* a) {
  if (!a || !a->next) return;
  
  window_t *p = NULL, *n = *head;
  while (n != a) { p = n; n = n->next; }
  
  if (p) p->next = a->next;
  else *head = a->next;
  
  window_t* tail = *head;
  while (tail->next) tail = tail->next;
  
  tail->next = a;
  a->next = NULL;
}


static window_t *dragging=NULL;
static int drag_anchor[2];
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
        switch (event.key.keysym.scancode) {
          case SDL_SCANCODE_ESCAPE:
            SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
            break;
          default:
            break;
        }
        break;
      case SDL_MOUSEMOTION:
        if (dragging) {
          dragging->x = SCALE_POINT(event.motion.x) - drag_anchor[0];
          dragging->y = SCALE_POINT(event.motion.y) - drag_anchor[1];
        }
        break;
      case SDL_MOUSEWHEEL:
        win = find_window(SCALE_POINT(event.wheel.mouseX), SCALE_POINT(event.wheel.mouseY));
        if (win) {
          win->proc(win, MSG_WHEEL, MAKEDWORD(-event.wheel.x*SCROLL_SENSITIVITY, event.wheel.y*SCROLL_SENSITIVITY), NULL);
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        dragging = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y));
        if (dragging) {
          moveToTop(&windows, dragging);
          if (SCALE_POINT(event.button.y) >= dragging->y) {
            dragging = NULL;
          } else {
            drag_anchor[0] = SCALE_POINT(event.button.x) - dragging->x;
            drag_anchor[1] = SCALE_POINT(event.button.y) - dragging->y;
          }
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (dragging) {
          dragging = NULL;
        } else {
          win = find_window(SCALE_POINT(event.button.x), SCALE_POINT(event.button.y));
          if (win && SCALE_POINT(event.button.y) >= win->y) {
            win->proc(win, MSG_CLICK, MAKEDWORD(SCALE_POINT(event.button.x)-win->x, SCALE_POINT(event.button.y)-win->y), NULL);
          }
        }
        break;
    }
  }
}

void draw_windows(void) {
  for (window_t *win = windows; win; win = win->next) {
    if (!(win->flags&WINDOW_TRANSPARENT)) {
      draw_panel(win->x, win->y-TITLEBAR_HEIGHT, win->w, win->h+TITLEBAR_HEIGHT);
    }
    if (!(win->flags&WINDOW_NOTITLE)) {
      fill_rect(0x40000000, win->x, win->y-TITLEBAR_HEIGHT, win->w, TITLEBAR_HEIGHT);
      draw_text_gl3(win->title, win->x+4, win->y+1-TITLEBAR_HEIGHT, 1);
    }
    set_viewport(win);
    set_projection(win->w, win->h);
    win->proc(win, MSG_DRAW, 0, NULL);
    set_viewport(&(window_t){0, 0, screen_width, screen_height});
    set_projection(screen_width, screen_height);
  }
}
