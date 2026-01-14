# Code Structure Refactoring

## Overview

This document describes the refactoring of DOOM-ED to separate game-related code from editor-related code into distinct libraries.

## Architecture

### Library Structure

```
doom-ed/
├── libgame/         # Game data structures and WAD I/O (NEW)
│   ├── libgame.h    # Main header
│   ├── wad.h        # WAD file format definitions
│   ├── wad.c        # WAD file I/O implementation
│   ├── map.h        # Map data structures
│   └── (future: thing definitions, sprite data)
│
├── ui/              # UI framework (libdesktop - already separated)
│   ├── commctl/     # Common controls
│   ├── user/        # User interface primitives
│   └── kernel/      # Event handling
│
├── mapview/         # Editor application (main app)
│   ├── editor*.c    # Editor-specific logic
│   ├── rendering    # Rendering code (bsp, triangulate, floor, walls, etc.)
│   └── windows/     # Inspector windows and UI
│
├── doom/            # DOOM game engine headers
├── hexen/           # Hexen game engine code
└── (future: librender/) # Rendering library
```

### Build Dependencies

```
libgame.a (independent)
    ↓
libdesktop.a (depends on SDL2, OpenGL)
    ↓
doom-ed (depends on libgame.a, libdesktop.a, hexen code, mapview code)
```

## libgame Library

### Purpose
Provides core game data structures and WAD file I/O functionality for DOOM engine games. 
Independent of rendering and editor functionality.

### Contents

#### wad.h / wad.c
- WAD file format definitions (wadheader_t, filelump_t)
- Type definitions (wadid_t, lumpname_t, texname_t)
- WAD file I/O functions:
  - `init_wad()` / `shutdown_wad()`
  - `find_lump()` / `cache_lump()`
  - `load_map()` / `free_map_data()`

#### map.h
- Map data structure definitions:
  - `mapvertex_t`, `mapsector_t`, `mapsidedef_t`
  - `maplinedef_t`, `mapthing_t`
  - `mapnode_t`, `mapsubsector_t`, `mapseg_t`
- Map data container: `map_data_t`
- Helper macros: `DEFINE_COLLECTION`, `CLEAR_COLLECTION`

### Design Notes

1. **No rendering dependencies**: libgame has no OpenGL or SDL dependencies
2. **Conditional compilation**: Supports both DOOM and Hexen formats via `#ifdef HEXEN`
3. **Minimal dependencies**: Only requires standard C library
4. **Extensible**: The editor extends `map_data_t` with rendering buffers

## Future Work

### Phase 1: libgame (CURRENT)
- [x] Create libgame directory structure
- [x] Extract WAD I/O code
- [x] Extract map data structures
- [x] Create Makefile targets for libgame
- [ ] Update mapview to use libgame headers
- [ ] Test build system

### Phase 2: librender
- [ ] Create librender directory
- [ ] Move rendering code (bsp.c, triangulate.c, floor.c, walls.c, sky.c)
- [ ] Move texture/sprite code
- [ ] Create librender.a target

### Phase 3: Editor reorganization
- [ ] Consider renaming mapview/ to editor/
- [ ] Clarify editor-specific vs reusable code
- [ ] Update documentation

### Phase 4: Integration
- [ ] Update build dependencies
- [ ] Verify all tests pass
- [ ] Update README.md and BUILD.md
- [ ] Update Xcode project

## Benefits

1. **Modularity**: Clear separation between game data, rendering, and editing
2. **Reusability**: libgame can be used by other DOOM tools
3. **Testability**: Each library can be tested independently
4. **Maintainability**: Easier to understand component boundaries
5. **Future-proofing**: Easier to add support for other DOOM engine variants

## Migration Guide

### For developers

When adding new code:

1. **Game data structures** → libgame/
2. **Rendering code** → mapview/ (later librender/)
3. **Editor logic** → mapview/
4. **UI components** → ui/

### Include paths

After refactoring:
```c
#include <libgame/libgame.h>  // For game data structures and WAD I/O
#include "map.h"               // For editor-extended map structures
#include "../ui/ui.h"          // For UI framework
```

## Compatibility

The refactoring maintains backward compatibility:
- Existing code continues to work
- Build system supports gradual migration
- No changes to external APIs
