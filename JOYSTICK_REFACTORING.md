# Joystick Input Refactoring

This document explains the joystick input management refactoring that decouples SDL-specific code and prepares the codebase for potential migration to GLFW.

## Overview

Joystick/gamepad input has been refactored from being scattered across the codebase into a centralized abstraction layer within the `ui/kernel/` subsystem. This achieves two key goals:

1. **Separation of Concerns**: All SDL joystick code is now isolated in the UI layer
2. **Future-Proofing**: The abstraction layer facilitates migration to GLFW or other input libraries

## Architecture

### Before Refactoring

```
mapview/renderer.c
├── Direct SDL_JoystickOpen() calls
├── Direct SDL_JoystickClose() calls
└── Exposed SDL_Joystick* as global variable

mapview/editor_input.c
├── Direct SDL_PollEvent() for joystick events
└── Direct handling of SDL_JOYBUTTONDOWN/SDL_JOYAXISMOTION

ui/kernel/event.c
└── Dispatched joystick events to windows (good!)
```

### After Refactoring

```
ui/kernel/joystick.c (NEW)
├── ui_joystick_init() - Initialize joystick subsystem
├── ui_joystick_shutdown() - Cleanup joystick resources
├── ui_joystick_available() - Check if joystick is connected
└── ui_joystick_get_name() - Get joystick device name
    └── All SDL_Joystick* code isolated here

ui/kernel/kernel.h
├── Exposes ui_joystick_*() functions (abstracted)
└── Removed SDL_Joystick* global (implementation detail)

ui/kernel/event.c
└── Routes SDL joystick events to window messages (unchanged)
    ├── SDL_JOYAXISMOTION → WM_JOYAXISMOTION
    └── SDL_JOYBUTTONDOWN → WM_JOYBUTTONDOWN

mapview/renderer.c
├── Calls ui_joystick_init() (abstracted)
└── Calls ui_joystick_shutdown() (abstracted)

mapview/windows/game.c
└── Handles WM_JOYAXISMOTION/WM_JOYBUTTONDOWN messages (unchanged)
```

## Key Design Principles

### 1. Opaque Handles

The `ui_joystick_t` type is defined as `void*`, hiding the underlying `SDL_Joystick*` implementation. This allows the backend to be swapped without changing application code.

```c
// In ui/kernel/joystick.h
typedef void* ui_joystick_t;  // Opaque handle

// In ui/kernel/joystick.c (SDL implementation)
static SDL_Joystick* g_joystick = NULL;  // Internal detail
```

### 2. Event-Driven Architecture

Joystick input flows through the existing window message system:

1. SDL generates joystick events (`SDL_JOYAXISMOTION`, `SDL_JOYBUTTONDOWN`)
2. `ui/kernel/event.c` converts them to window messages (`WM_JOYAXISMOTION`, `WM_JOYBUTTONDOWN`)
3. Window procedures handle the messages (e.g., `win_game()` in `mapview/windows/game.c`)

This design:
- Maintains separation between input backend and application logic
- Works identically whether using SDL events or GLFW polling
- Allows multiple windows to receive input events

### 3. Platform Abstraction

All SDL-specific types and functions are confined to `ui/kernel/`:

| Layer | SDL Dependency | GLFW Migration Path |
|-------|---------------|---------------------|
| Application (`mapview/`) | None | No changes needed |
| UI Messages (`ui/user/`) | None | No changes needed |
| Event Dispatch (`ui/kernel/event.c`) | SDL events → Messages | Poll GLFW → Generate messages |
| Joystick Backend (`ui/kernel/joystick.c`) | SDL_Joystick* | glfwGetGamepadState() |

## SDL to GLFW Migration Path

When migrating to GLFW, only `ui/kernel/` needs to change:

### Current SDL Implementation

```c
// ui/kernel/joystick.c (SDL backend)
bool ui_joystick_init(void) {
  SDL_InitSubSystem(SDL_INIT_JOYSTICK);
  g_joystick = SDL_JoystickOpen(0);
  SDL_JoystickEventState(SDL_ENABLE);
  return g_joystick != NULL;
}

// ui/kernel/event.c (SDL events)
void dispatch_message(SDL_Event *evt) {
  switch (evt->type) {
    case SDL_JOYAXISMOTION:
      send_message(_focused, WM_JOYAXISMOTION, 
                   MAKEDWORD(evt->jaxis.axis, evt->jaxis.value), NULL);
      break;
  }
}
```

### Future GLFW Implementation

```c
// ui/kernel/joystick.c (GLFW backend)
bool ui_joystick_init(void) {
  // GLFW joysticks are automatically detected
  return glfwJoystickPresent(GLFW_JOYSTICK_1);
}

// ui/kernel/event.c (GLFW polling)
void poll_joystick_events(void) {
  GLFWgamepadstate state;
  if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
    // Generate WM_JOYAXISMOTION for each axis
    for (int i = 0; i < 6; i++) {
      send_message(_focused, WM_JOYAXISMOTION,
                   MAKEDWORD(i, state.axes[i] * 32767), NULL);
    }
    
    // Generate WM_JOYBUTTONDOWN for button state changes
    for (int i = 0; i < 15; i++) {
      if (state.buttons[i] && !prev_state.buttons[i]) {
        send_message(_focused, WM_JOYBUTTONDOWN, i, NULL);
      }
    }
  }
}
```

## Benefits

### 1. Clear Separation of Concerns
- All SDL joystick code is in `ui/kernel/joystick.c`
- No SDL types leak into application code
- Easy to find and modify joystick-related code

### 2. Maintainability
- Single source of truth for joystick initialization
- Consistent error handling and logging
- Clear ownership of joystick lifecycle

### 3. Testability
- Can mock `ui_joystick_*()` functions for testing
- Can implement alternative backends (e.g., for testing)
- Application code doesn't depend on SDL being available

### 4. Future-Proofing
- Migration to GLFW only requires changes to `ui/kernel/`
- Application code (`mapview/`) needs no changes
- Window message API remains stable

## Usage Examples

### Initialization

```c
// In mapview/renderer.c
bool init_sdl(void) {
  // ... window creation ...
  
  // Initialize joystick through UI layer
  ui_joystick_init();  // Abstracted - no SDL types
  
  // ... OpenGL setup ...
}
```

### Handling Input

```c
// In mapview/windows/game.c
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  game_t *game = win->userdata;
  
  switch (msg) {
    case WM_JOYAXISMOTION:
      // Extract axis index and value from wparam
      switch (LOWORD(wparam)) {
        case 0: game->player.strafe_move = 
                ((int16_t)HIWORD(wparam)) / (float)0x8000; 
                break;
        case 1: game->player.forward_move = 
                -((int16_t)HIWORD(wparam)) / (float)0x8000; 
                break;
      }
      return true;
      
    case WM_JOYBUTTONDOWN:
      // wparam contains button index
      if (wparam == 0) {
        // Handle button 0 press
      }
      return true;
  }
}
```

## Migration Checklist

For teams migrating from SDL to GLFW:

- [ ] Update `ui/kernel/joystick.c` to use GLFW gamepad API
- [ ] Update `ui/kernel/event.c` to poll GLFW and generate window messages
- [ ] Update `ui/kernel/init.c` to use GLFW initialization
- [ ] Test joystick input in all game modes
- [ ] Verify button/axis mappings match expected behavior
- [ ] Update documentation

Application code in `mapview/` requires **no changes**.

## Files Changed

### New Files
- `ui/kernel/joystick.h` - Joystick API definitions
- `ui/kernel/joystick.c` - SDL joystick implementation

### Modified Files
- `ui/kernel/kernel.h` - Added joystick API, removed SDL_Joystick* global
- `mapview/renderer.c` - Use `ui_joystick_init/shutdown()` instead of direct SDL
- `mapview/editor_input.c` - Added deprecation notes for direct SDL polling
- `Makefile` - Added `ui/kernel/joystick.c` to build

### Unchanged Files (by design)
- `ui/kernel/event.c` - Already routes events correctly
- `mapview/windows/game.c` - Already uses window messages
- All other application code

## Testing

To verify joystick functionality:

1. **Build the project**: `make clean && make`
2. **Connect a gamepad** before running
3. **Launch DOOM-ED**: `./doom-ed <wad_file>`
4. **Test in game mode** (Tab key to enter):
   - Move left stick - camera should move
   - Move right stick - camera should look around
   - Press buttons - actions should trigger
5. **Check console output** for joystick detection message

Expected output:
```
Opened joystick: Xbox Controller
```

## Notes

- Joystick initialization is optional - the editor works without a gamepad
- The first detected joystick is used (single-player focus)
- Event routing uses the existing window focus system
- SDL_INIT_JOYSTICK is still passed to SDL_Init in renderer.c (will be removed in full GLFW migration)

## References

- SDL2 Joystick API: https://wiki.libsdl.org/CategoryJoystick
- GLFW Gamepad API: https://www.glfw.org/docs/latest/input_guide.html#gamepad
- Window Messages: `ui/user/messages.h` (WM_JOYAXISMOTION, WM_JOYBUTTONDOWN)
