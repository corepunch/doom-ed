# Quick Start Guide - UI Framework

## Build the Framework

### Using Makefile

```bash
# Build UI framework only
make ui_framework

# Build everything (framework + mapview)
make all

# Clean all build artifacts
make clean
```

Output:
- `build/libui_framework.a` - Static library
- `build/ui_framework/*.o` - Object files

### Using Xcode

See `XCODE_SETUP.md` for detailed instructions on adding the UI framework as a separate target.

## Use the Framework in Your Project

### 1. Include the Header

```c
#include "ui_framework.h"
```

### 2. Link Against the Library

**Makefile:**
```makefile
CFLAGS += -Ipath/to/ui_framework
LDFLAGS += -Lpath/to/build -lui_framework -lSDL2
```

**Xcode:**
1. Add `libui_framework.a` to "Link Binary With Libraries"
2. Add `ui_framework/` to "Header Search Paths"
3. Add SDL2 framework to project

### 3. Initialize SDL and Create Windows

```c
#include "ui_framework.h"

bool running = true;

// Window message handler
result_t my_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      printf("Window created: %s\n", win->title);
      return 0;
      
    case WM_LBUTTONDOWN:
      printf("Mouse click at (%d, %d)\n", LOWORD(wparam), HIWORD(wparam));
      return 0;
      
    case WM_DESTROY:
      running = false;
      return 0;
  }
  return 0;
}

int main() {
  // Initialize SDL window
  SDL_Window *sdl_window;
  if (!init_sdl_window(&sdl_window, 800, 600)) {
    return 1;
  }
  
  // Create UI window using framework
  window_t *win = create_window(
    "My Application",              // title
    WINDOW_TOOLBAR,                // flags
    MAKERECT(50, 50, 700, 500),    // rect
    NULL,                          // parent
    my_window_proc,                // message handler
    NULL                           // user data
  );
  
  show_window(win, true);
  
  // Run event loop
  run_event_loop(&running);
  
  // Cleanup
  cleanup_sdl(sdl_window);
  
  return 0;
}
```

## Window Flags

Combine flags with `|`:

```c
window_t *win = create_window("Title", 
                               WINDOW_NOTITLE | WINDOW_TRANSPARENT,
                               rect, parent, proc, data);
```

Available flags:
- `WINDOW_NOTITLE` - No title bar
- `WINDOW_TRANSPARENT` - Transparent background
- `WINDOW_VSCROLL` - Vertical scrolling
- `WINDOW_NOFILL` - Don't fill background
- `WINDOW_ALWAYSINBACK` - Keep behind other windows
- `WINDOW_TOOLBAR` - Include toolbar area
- `WINDOW_NOTRAYBUTTON` - Don't show in tray

## Common Messages

Handle these in your window procedure:

- `WM_CREATE` - Window just created
- `WM_DESTROY` - Window being destroyed
- `WM_PAINT` - Repaint needed
- `WM_MOUSEMOVE` - Mouse moved
- `WM_LBUTTONDOWN` - Left mouse button pressed
- `WM_LBUTTONUP` - Left mouse button released
- `WM_KEYDOWN` - Key pressed
- `WM_KEYUP` - Key released
- `WM_COMMAND` - Command message

## Window Management Functions

```c
// Create and show
window_t *win = create_window(...);
show_window(win, true);

// Move and resize
move_window(win, 100, 100);
resize_window(win, 640, 480);

// Focus and capture
set_focus(win);
set_capture(win);

// Send messages
send_message(win, WM_COMMAND, 123, NULL);
post_message(win, WM_PAINT, 0, NULL);

// Destroy
destroy_window(win);
```

## Child Windows

```c
// Create parent window
window_t *parent = create_window("Parent", 0, 
                                  MAKERECT(0, 0, 400, 300),
                                  NULL, parent_proc, NULL);

// Create child window
window_t *child = create_window("Child", 0,
                                 MAKERECT(10, 10, 100, 50),
                                 parent,  // parent window
                                 child_proc, NULL);
```

## Message Queue

```c
// Send message immediately (calls window proc)
send_message(win, WM_COMMAND, 123, data);

// Post message to queue (processed later)
post_message(win, WM_PAINT, 0, NULL);

// Process queued messages
repost_messages();
```

## Window Hooks

Register global hooks for messages:

```c
void my_hook(window_t *win, uint32_t msg, uint32_t wparam, 
             void *lparam, void *userdata) {
  printf("Hook called for message %d\n", msg);
}

// Register hook for all messages
register_window_hook(0, my_hook, NULL);

// Register hook for specific message
register_window_hook(WM_LBUTTONDOWN, my_hook, NULL);
```

## Important Notes

1. **SDL Required**: Both the framework and your application need SDL2
2. **Rendering**: Framework doesn't provide drawing functions - implement your own
3. **OpenGL**: Framework doesn't create OpenGL context - do this separately
4. **Message Handlers**: Return 0 from message handlers for default behavior

## Example: Simple Window

```c
#include "ui_framework.h"
#include <stdio.h>

bool running = true;

result_t window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (msg == WM_DESTROY) {
    running = false;
  }
  return 0;
}

int main() {
  SDL_Window *sdl_win;
  init_sdl_window(&sdl_win, 640, 480);
  
  window_t *ui_win = create_window("Hello", 0, 
                                     MAKERECT(10, 10, 300, 200),
                                     NULL, window_proc, NULL);
  show_window(ui_win, true);
  
  run_event_loop(&running);
  
  cleanup_sdl(sdl_win);
  return 0;
}
```

## Documentation

- `ui_framework/README.md` - Framework overview and API
- `FRAMEWORK_ARCHITECTURE.md` - Architecture and design decisions
- `XCODE_SETUP.md` - Xcode integration guide
- `FRAMEWORK_POC_SUMMARY.md` - Complete POC summary

## Getting Help

1. Check the example usage in `ui_framework/README.md`
2. Review architecture docs in `FRAMEWORK_ARCHITECTURE.md`
3. Look at the original DOOM-ED code in `mapview/window.c` for advanced usage
4. Check message constants in `ui_framework.h`
