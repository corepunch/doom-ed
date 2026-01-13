# UI Framework Extraction - Architecture Notes

## Overview

This document explains the architecture of the extracted UI framework and why certain design decisions were made.

## What Was Extracted

The UI framework (`ui_framework/`) contains:

1. **Window Management** (`ui_window.c`)
   - Window creation, destruction, and hierarchy
   - Message queue and routing
   - Focus and capture management
   - Window properties and state

2. **SDL Integration** (`ui_sdl.c`)
   - SDL window initialization
   - Event loop management
   - Basic event polling

## What Remains in mapview

The following **must** remain in mapview because they are application-specific or rendering-specific:

1. **OpenGL Rendering**
   - Shader compilation and management
   - OpenGL context creation (via SDL_GL_CreateContext)
   - Texture management
   - 3D rendering pipeline

2. **Drawing Primitives**
   - `fill_rect()` - fills rectangles with colors
   - `draw_text_gl3()` - text rendering
   - `draw_icon8()`, `draw_icon16()` - icon rendering
   - These are implemented using OpenGL and are DOOM-ED specific

3. **Window Procedures**
   - `win_button()`, `win_checkbox()`, etc.
   - These use the drawing primitives above
   - Application-specific control behavior

4. **SDL Dependencies**
   - OpenGL context management (SDL_GLContext, SDL_GL_*)
   - Window size queries (SDL_GetWindowSize, SDL_GL_GetDrawableSize)
   - Timer functions (SDL_GetTicks)
   - Direct OpenGL integration

## Why mapview Still Links Against SDL

**Conclusion: mapview MUST link against SDL directly.**

### Reasons:

1. **OpenGL Context**: mapview creates and manages an OpenGL context using SDL:
   ```c
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   ctx = SDL_GL_CreateContext(window);
   ```

2. **OpenGL Surface Management**: Uses SDL_GL_GetDrawableSize for rendering
   
3. **Direct SDL API Usage**: Many files use SDL functions directly:
   - `sprites.c`: SDL_GetWindowSize, SDL_GetTicks
   - `wi_stuff.c`: SDL_SetRelativeMouseMode
   - `input.c`: SDL_GetTicks
   - `window.c`: SDL_GL_GetDrawableSize

4. **Rendering Loop**: The main rendering loop in `renderer.c` directly manages SDL

### What the Framework Provides

The framework provides an **abstraction layer** for window management while still allowing the application to use SDL for rendering:

- **Separation of Concerns**: Window management logic is separate from rendering
- **Reusability**: The window management code can be used in other SDL+OpenGL applications
- **Clean Interface**: Applications interact with windows through the framework API

## Architecture Diagram

```
┌─────────────────────────────────────────┐
│           mapview Application           │
│                                         │
│  ┌───────────┐  ┌──────────────────┐  │
│  │  OpenGL   │  │ DOOM-ED Specific │  │
│  │ Rendering │  │   Game Logic     │  │
│  └─────┬─────┘  └────────┬─────────┘  │
│        │                 │             │
│        └────────┬────────┘             │
│                 │                       │
│         ┌───────▼────────┐             │
│         │ UI Framework   │             │
│         │ Window Mgmt    │             │
│         └───────┬────────┘             │
│                 │                       │
└─────────────────┼─────────────────────┘
                  │
          ┌───────▼────────┐
          │      SDL2      │
          │  (Windowing &  │
          │  Input/OpenGL) │
          └────────────────┘
```

## Benefits of This Approach

1. **Code Organization**: Clear separation between window management and rendering
2. **Reusability**: The UI framework can be used in other projects that use SDL+OpenGL
3. **Maintainability**: Window management bugs can be fixed in one place
4. **Testing**: Window management can be tested independently
5. **Future Expansion**: Framework can be extended with more features

## Limitations and Trade-offs

1. **SDL Still Required**: Both framework and application link against SDL
   - This is necessary because SDL is used for both windowing AND OpenGL
   - Alternative would require replacing SDL entirely (major undertaking)

2. **Rendering Not Abstracted**: Drawing functions remain application-specific
   - Could be abstracted in the future through callbacks
   - Current approach keeps POC simple and focused

3. **Partial Extraction**: Some window-related code still in mapview
   - Window procedures that depend on rendering stay in mapview
   - Event translation from SDL to window messages partially in both

## Future Improvements

1. **Rendering Callbacks**: Define a rendering interface that applications implement
2. **Complete Event Abstraction**: Move all SDL event handling into framework
3. **Platform Abstraction**: Support non-SDL backends (GLFW, native, etc.)
4. **Control Library**: Move common controls into framework with rendering callbacks
5. **Layout System**: Add automatic layout management

## Conclusion

This POC successfully demonstrates:
- ✅ UI framework as separate library (Makefile)
- ✅ Generic window management extracted
- ✅ mapview links against UI framework
- ✅ Framework links against SDL

The framework **does not eliminate SDL dependency from mapview** because mapview fundamentally relies on SDL for OpenGL integration. This is correct and expected for an SDL+OpenGL application. The framework provides a reusable window management layer on top of SDL that can be used by other similar applications.
