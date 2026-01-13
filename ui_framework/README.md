# UI Framework

A lightweight UI framework extracted from DOOM-ED for reuse in other projects.

## Overview

This framework provides:
- Window management (creation, destruction, hierarchy)
- Event handling (mouse, keyboard)
- Message passing system
- SDL integration for windowing and input

## Architecture

The framework is split into two main components:

1. **ui_window.c** - Core window management
   - Window creation and lifecycle
   - Message queue and dispatching
   - Window hierarchy and focus management
   
2. **ui_sdl.c** - SDL integration
   - SDL initialization
   - Event loop management
   - Window creation with SDL

## Dependencies

- SDL2 (required) - for windowing and input
- Standard C library

## Building

### As a static library (Makefile)

```bash
make ui_framework
```

This creates `build/libui_framework.a`

### Linking against the framework

```makefile
# In your Makefile
CFLAGS += -Iui_framework
LDFLAGS += -Lui_framework -lui_framework -lSDL2
```

### Xcode Integration

The framework can be built as a separate Xcode target and linked into the main application.

## API Usage

```c
#include "ui_framework.h"

// Initialize SDL window
SDL_Window *window;
if (!init_sdl_window(&window, 800, 600)) {
    return 1;
}

// Create a window
window_t *win = create_window("My Window", 
                               0,                    // flags
                               MAKERECT(100, 100, 400, 300),
                               NULL,                 // parent
                               my_window_proc,       // message handler
                               NULL);                // user data

show_window(win, true);

// Event loop
bool running = true;
run_event_loop(&running);

// Cleanup
cleanup_sdl(window);
```

## Window Flags

- `WINDOW_NOTITLE` - Window without title bar
- `WINDOW_TRANSPARENT` - Transparent background
- `WINDOW_ALWAYSINBACK` - Keep window in back
- `WINDOW_NOTRAYBUTTON` - Don't show in tray
- `WINDOW_NOFILL` - Don't fill background
- `WINDOW_VSCROLL` - Enable vertical scrolling
- `WINDOW_TOOLBAR` - Include toolbar area

## Message Constants

- `WM_CREATE` - Window created
- `WM_DESTROY` - Window being destroyed
- `WM_PAINT` - Repaint request
- `WM_MOUSEMOVE` - Mouse moved
- `WM_LBUTTONDOWN` - Left mouse button down
- `WM_LBUTTONUP` - Left mouse button up
- `WM_KEYDOWN` - Key pressed
- `WM_KEYUP` - Key released
- `WM_COMMAND` - Command message

## Design Philosophy

The framework separates core window management from rendering. The application
is responsible for:
- Rendering (OpenGL, etc.)
- Drawing primitives (text, rectangles, etc.)
- Application-specific window procedures

The framework provides:
- Window lifecycle management
- Event routing and message passing
- SDL integration
- Focus and input capture

## Future Enhancements

- More complete event handling
- Built-in common controls (buttons, lists, etc.)
- Better dialog support
- Multi-window support improvements
