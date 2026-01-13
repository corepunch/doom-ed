// Window system stub - actual implementation moved to ui/ framework
// This file remains only to provide compatibility with existing code

#include <SDL2/SDL.h>
#include "gl_compat.h"

#include "map.h"
#include "sprites.h"
#include "editor.h"
#include "../ui/ui.h"

// SDL window (defined here for compatibility)
extern SDL_Window* window;

// All window management functionality is now in:
// - ui/user/window.c - Window lifecycle and management
// - ui/user/message.c - Message queue and dispatch
// - ui/user/draw_impl.c - Drawing primitives
// - ui/kernel/event.c - SDL event handling
//
// Controls are in:
// - ui/commctl/*.c - Button, checkbox, edit, label, list, combobox, sprite

// Dialog handling (still in mapview as it's app-specific)
static uint32_t _return_code;

extern bool running;

uint32_t show_dialog(char const *title, const rect_t* frame, window_t *parent, winproc_t proc, void *param) {
  SDL_Event e;
  window_t *dlg = create_window(title, WINDOW_DIALOG, frame, parent, proc, param);
  dlg->visible = true;
  while (running && is_window(dlg)) {
    while (get_message(&e)) {
      dispatch_message(&e);
    }
    repost_messages();
  }
  return _return_code;
}

// Dialog result handling
void end_dialog(window_t *win, uint32_t code) {
  _return_code = code;
  destroy_window(win);
}
