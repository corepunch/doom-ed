//
//  ui_framework.h
//  UI Framework - Generic window management and input handling
//
//  Extracted from DOOM-ED for reusability
//

#ifndef __UI_FRAMEWORK_H__
#define __UI_FRAMEWORK_H__

#include <stdint.h>
#include <stdbool.h>

// Forward declaration for SDL
typedef union SDL_Event SDL_Event;
typedef struct SDL_Window SDL_Window;

// Basic types from original code - keeping compatibility
typedef struct {
  int16_t x, y, w, h;
} rect_t;

typedef uint32_t flags_t;
typedef uint32_t result_t;

#define MAKERECT(X, Y, W, H) &(rect_t){X, Y, W, H}

// Forward declaration
struct window_s;

// Window callback types - keeping original names for compatibility
typedef result_t (*winproc_t)(struct window_s *, uint32_t, uint32_t, void *);
typedef void (*winhook_func_t)(struct window_s *win, uint32_t msg, uint32_t wparam, void *lparam, void *userdata);

// Window definition for creating child windows
typedef struct {
  const char *classname;
  const char *text;
  uint32_t id;
  int w, h;
  flags_t flags;
} windef_t;

// Toolbar button structure
typedef struct {
  int icon;
  int ident;
  bool active;
} toolbar_button_t;

// Window structure - keeping original name for compatibility
typedef struct window_s {
  rect_t frame;
  uint32_t id;
  uint16_t scroll[2];
  uint32_t flags;
  winproc_t proc;
  uint32_t child_id;
  bool hovered;
  bool editing;
  bool notabstop;
  bool pressed;
  bool value;
  bool visible;
  bool disabled;
  char title[64];
  int cursor_pos;
  int num_toolbar_buttons;
  toolbar_button_t *toolbar_buttons;
  void *userdata;
  void *userdata2;
  struct window_s *next;
  struct window_s *children;
  struct window_s *parent;
} window_t;

// Window flags - keeping original names for compatibility
#define WINDOW_NOTITLE         0x0001
#define WINDOW_TRANSPARENT     0x0002
#define WINDOW_ALWAYSINBACK    0x0004
#define WINDOW_NOTRAYBUTTON    0x0008
#define WINDOW_NOFILL          0x0010
#define WINDOW_VSCROLL         0x0020
#define WINDOW_TOOLBAR         0x0040

// Message constants - these match the original window.c implementation
#define WM_CREATE          0x0001
#define WM_DESTROY         0x0002
#define WM_PAINT           0x000F
#define WM_CLOSE           0x0010
#define WM_MOUSEMOVE       0x0200
#define WM_LBUTTONDOWN     0x0201
#define WM_LBUTTONUP       0x0202
#define WM_RBUTTONDOWN     0x0204
#define WM_RBUTTONUP       0x0205
#define WM_MOUSEWHEEL      0x020A
#define WM_KEYDOWN         0x0100
#define WM_KEYUP           0x0101
#define WM_COMMAND         0x0111

// Core UI framework functions - window management
window_t *create_window(char const *title, flags_t flags, const rect_t *rect, 
                         window_t *parent, winproc_t proc, void *param);
void show_window(window_t *win, bool visible);
void destroy_window(window_t *win);
void clear_window_children(window_t *win);
void move_window(window_t *win, int x, int y);
void resize_window(window_t *win, int new_w, int new_h);
void invalidate_window(window_t *win);
void enable_window(window_t *win, bool enable);
bool is_window(window_t *win);

// Window hierarchy and focus
void set_focus(window_t *win);
void track_mouse(window_t *win);
void set_capture(window_t *win);

// Message handling
int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
int get_message(SDL_Event *evt);
void dispatch_message(SDL_Event *evt);
void repost_messages(void);

// Window utilities
window_t *get_window_item(window_t const *win, uint32_t id);
void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...);
void load_window_children(window_t *win, windef_t const *def);
int window_title_bar_y(window_t const *win);

// Hooks
void register_window_hook(uint32_t msg, winhook_func_t func, void *userdata);

// Dialog support
void end_dialog(window_t *win, uint32_t code);
uint32_t show_dialog(char const *title, const rect_t *rect, window_t *parent, 
                      winproc_t proc, void *param);

// SDL initialization - keeping this in the framework
bool init_sdl_window(SDL_Window **window, int screen_width, int screen_height);
void cleanup_sdl(SDL_Window *window);
int run_event_loop(bool *running);

#endif // __UI_FRAMEWORK_H__
