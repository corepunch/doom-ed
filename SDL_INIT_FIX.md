# SDL Initialization Order Fix

## Problem

After extracting the editor into a separate UI module, shader compilation started failing with:
```
Radial shader compilation error: ERROR: 0:1: '' :  version '150' is not supported
ERROR: 0:1: '' : syntax error: #version
```

The OpenGL context was being created as version 2.1 instead of 3.2+, even though we were requesting 3.2.

## Root Cause

The initialization logic was split between two functions:
1. `ui_init_window()` in `ui/kernel/init.c` - created window and GL context
2. `init_sdl()` in `mapview/renderer.c` - initialized SDL subsystems

The problem was the **incorrect calling order**:
```c
// In main.c - WRONG ORDER
ui_init_window(...);  // Created window WITHOUT SDL_Init being called first
init_sdl();           // Called SDL_Init AFTER window was created
```

This violated SDL's requirements:
- `SDL_Init()` must be called BEFORE any SDL functions (including `SDL_CreateWindow`)
- `SDL_GL_SetAttribute()` must be called BEFORE `SDL_CreateWindow()`

## Solution

Restored `init_sdl()` to perform the complete initialization sequence in the correct order:

```c
bool init_sdl(void) {
  // 1. Initialize SDL subsystems FIRST
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) < 0) {
    return false;
  }
  
  // 2. Set OpenGL attributes BEFORE creating window
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  // 3. Create window
  window = SDL_CreateWindow(...);
  
  // 4. Create OpenGL context
  ctx = SDL_GL_CreateContext(window);
  
  // 5. Verify version
  printf("GL_VERSION  : %s\n", glGetString(GL_VERSION));
  printf("GLSL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  
  // ... rest of initialization
}
```

Removed the call to `ui_init_window()` from `main.c` since `init_sdl()` now handles everything.

## Why This Matters

- **GLSL #version 150** requires OpenGL 3.2+
- **GLSL #version 120** is for OpenGL 2.1 (what we were getting)
- Modern shader features require 3.2+

## Verification

Before fix:
```
GL_VERSION  : 2.1 Metal - 90.5
GLSL_VERSION: 1.20
```

After fix (expected on systems with proper GL support):
```
GL_VERSION  : 3.2+ (Core Profile) or 4.x
GLSL_VERSION: 1.50+ or 4.x
```

## Related Files

- `mapview/renderer.c` - Restored full initialization in `init_sdl()`
- `mapview/main.c` - Removed redundant `ui_init_window()` call
- `ui/kernel/init.c` - Reverted changes (function still exists for UI framework examples)

## Testing

Run existing tests to verify no regressions:
```bash
make test
```

All triangulation and BSP tests pass.
