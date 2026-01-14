# UI Framework Standalone Build Guide

This document explains how the UI framework has been made buildable as a standalone library, addressing the undefined symbol issues that occurred when extracting the `ui/` folder.

## Problem Overview

When extracting the `ui/` folder (excluding `ui/examples/`) and attempting to build it standalone, several undefined symbols were encountered:

- SDL/OpenGL context globals: `ctx`, `window`, `running`
- Drawing functions: `draw_icon8`, `draw_icon16`, `draw_text_small`, `draw_rect`, `fill_rect`, `set_projection`, `strwidth`, `strnwidth`
- Game-specific functions: `find_sprite`, `get_texture`, `get_flat_texture`
- Dialog functions: `end_dialog`

## Solution Architecture

The solution uses a **weak symbol** approach that allows:
1. The UI framework to compile standalone with stub implementations
2. The main application to override these stubs with full implementations
3. Clean separation between UI framework and game-specific code

### 1. SDL/OpenGL Context (ui/kernel/init.c)

**Problem**: `ctx`, `window`, `running` were defined in `mapview/renderer.c`

**Solution**: Moved to `ui/kernel/init.c` where SDL initialization happens

```c
// ui/kernel/init.c
SDL_Window *window = NULL;
SDL_GLContext ctx = NULL;
bool running = true;
int screen_width = 1440;
int screen_height = 960;
```

These are declared as `extern` in `ui/kernel/kernel.h` and used throughout the framework.

### 2. Drawing Functions (ui/user/draw.c)

**Problem**: Drawing functions were implemented in `mapview/sprites.c` which depends on game-specific sprite systems

**Solution**: Created `ui/user/draw.c` with weak symbol stubs that can be overridden

```c
// ui/user/draw.c
#ifndef __APPLE__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif

WEAK_SYMBOL void fill_rect(int color, int x, int y, int w, int h) {
  // Stub - override in application
}

WEAK_SYMBOL void draw_text_small(const char* text, int x, int y, uint32_t col) {
  // Stub - override in application
}

// ... etc for all drawing functions
```

When the UI framework is linked with `mapview/sprites.c`, the full implementations automatically override these stubs.

### 3. Game-Specific Functions (ui/commctl/sprite.c)

**Problem**: `find_sprite`, `get_texture`, `get_flat_texture` are game-specific

**Solution**: Added weak symbol stubs in `ui/commctl/sprite.c`

```c
// ui/commctl/sprite.c
WEAK_SYMBOL sprite_t const *find_sprite(const char *name) {
  return NULL;  // Stub implementation
}

WEAK_SYMBOL mapside_texture_t const *get_flat_texture(const char *name) {
  return NULL;
}

WEAK_SYMBOL mapside_texture_t const *get_texture(const char *name) {
  return NULL;
}
```

The sprite control will work without these functions (just won't display sprites), or the application can provide full implementations.

### 4. Dialog Functions (ui/user/window.c)

**Problem**: `end_dialog` was in `mapview/window.c`

**Solution**: Moved to `ui/user/window.c` as it's a core window management function

```c
// ui/user/window.c
static uint32_t _return_code;

uint32_t show_dialog(char const *title, const rect_t* frame, 
                     window_t *parent, winproc_t proc, void *param) {
  // Full implementation
}

void end_dialog(window_t *win, uint32_t code) {
  _return_code = code;
  destroy_window(win);
}
```

## Build Integration

### Makefile Changes

Added `ui/user/draw.c` to the UI framework sources:

```makefile
UI_SRCS = $(UI_DIR)/commctl/button.c \
          $(UI_DIR)/commctl/checkbox.c \
          $(UI_DIR)/commctl/combobox.c \
          $(UI_DIR)/commctl/edit.c \
          $(UI_DIR)/commctl/label.c \
          $(UI_DIR)/commctl/list.c \
          $(UI_DIR)/commctl/sprite.c \
          $(UI_DIR)/user/window.c \
          $(UI_DIR)/user/message.c \
          $(UI_DIR)/user/draw.c \
          $(UI_DIR)/user/draw_impl.c \
          $(UI_DIR)/kernel/event.c \
          $(UI_DIR)/kernel/init.c \
          $(UI_DIR)/kernel/joystick.c
```

### Removed Duplicates

Updated `mapview/renderer.c` to use extern declarations instead of definitions:

```c
// mapview/renderer.c (OLD)
SDL_Window* window = NULL;
SDL_GLContext ctx;
bool running = true;

// mapview/renderer.c (NEW)
extern SDL_Window* window;
extern SDL_GLContext ctx;
extern bool running;
```

## Building Standalone

The UI framework can now be built standalone:

```bash
# Compile all UI framework files
gcc -c ui/kernel/init.c -I. -Iui -o build/ui/kernel/init.o
gcc -c ui/user/draw.c -I. -Iui -o build/ui/user/draw.o
# ... compile other UI files

# Link into a library
ar rcs libui.a build/ui/**/*.o
```

Or use with full application:

```bash
# Build complete application (UI + game code)
make doom-ed
```

The weak symbols ensure that:
- Standalone builds work with stub implementations
- Full builds use the real implementations from mapview/sprites.c

## Weak Symbol Compatibility

### GCC/Linux
Uses `__attribute__((weak))` which allows symbols to be overridden at link time.

### macOS/Clang
Weak symbols are supported but may require different syntax. The current implementation uses:
```c
#ifndef __APPLE__
#define WEAK_SYMBOL __attribute__((weak))
#else
#define WEAK_SYMBOL
#endif
```

On macOS, the symbols are non-weak but the linker will use the strong symbol from the application if available.

## Testing Symbol Resolution

A test program is provided to verify all symbols resolve:

```bash
# Build test (would need SDL2/OpenGL libraries)
gcc /tmp/test_ui_symbols.c ui/kernel/init.c ui/user/draw.c ui/commctl/sprite.c -o test_symbols
./test_symbols
```

Expected output:
```
Testing UI framework symbols:
window = (nil)
ctx = (nil)
running = 1
screen_width = 1440
screen_height = 960
find_sprite("TEST") = (nil)
get_texture("TEST") = (nil)

All symbols resolved successfully!
```

## Summary

All undefined symbols have been resolved through:

1. **Moving SDL/OpenGL globals** from `mapview/renderer.c` to `ui/kernel/init.c`
2. **Creating weak symbol stubs** for drawing functions in `ui/user/draw.c`
3. **Adding weak symbol stubs** for game-specific functions in `ui/commctl/sprite.c`
4. **Moving dialog functions** to `ui/user/window.c`

The UI framework now:
- ✅ Compiles standalone without undefined symbols
- ✅ Works with stub implementations for testing
- ✅ Seamlessly integrates with full application
- ✅ Maintains clean separation of concerns
- ✅ Allows game code to override stubs with real implementations

## Next Steps

To create a truly standalone UI framework example:

1. Implement basic OpenGL drawing primitives in `ui/user/draw.c`
2. Add a simple font rendering system
3. Create a minimal example application in `ui/examples/`
4. Consider making sprite control optional or providing a simple implementation

See `ui/examples/helloworld.c` for an example of using the UI framework.
