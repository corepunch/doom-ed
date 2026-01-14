# DOOM-ED Architecture

## Overview

DOOM-ED is a level editor for classic DOOM games with a clean separation between editor functionality and game-specific code. This document describes the architectural organization of the codebase.

## Directory Structure

```
mapview/                  # Main editor source code
├── game/                 # Game-specific code (DOOM/Hexen data)
│   ├── thing_info.h      # Interface for game thing data
│   └── thing_info.c      # DOOM/Hexen thing implementation
├── windows/              # UI windows and dialogs
│   └── inspector/        # Property inspector windows
├── *.c, *.h              # Editor core functionality
ui/                       # UI framework (common controls, window management)
doom/                     # DOOM game data and headers
hexen/                    # Hexen game data and headers
tests/                    # Test files
```

## Architectural Principles

### Separation of Concerns

The codebase maintains a clear separation between:

1. **Editor Code** - Generic map editing functionality
   - Geometry editing (vertices, linedefs, sectors)
   - WAD file I/O
   - Rendering system
   - UI framework and windows
   - BSP traversal and collision detection

2. **Game Code** - DOOM/Hexen-specific data
   - Thing type definitions (`mobjinfo`, `states`, `sprnames`)
   - Sprite lookups and animation frames
   - Game-specific constants (player start types, etc.)

### Game Abstraction Layer

The `mapview/game/` module provides an abstraction layer that isolates game-specific dependencies:

```c
// Interface (thing_info.h)
sprite_t *game_get_thing_sprite(uint16_t thing_type, uint16_t angle);
int game_get_num_sprites(void);
const char *game_get_sprite_name(int sprite_index);
bool game_is_player_start(uint16_t thing_type);
bool game_init_thing_info(void);
```

This allows editor code to work with thing data without directly depending on DOOM/Hexen headers.

## Key Components

### Editor Core (`mapview/`)

**Pure Editor Files** (no game dependencies):
- `editor_draw.c`, `editor_input.c`, `editor_sector.c` - Editing logic
- `wad.c` - WAD file format I/O
- `bsp.c`, `triangulate.c` - Geometry algorithms
- `renderer.c`, `walls.c`, `floor.c`, `sky.c` - Map rendering
- `collision.c` - Collision detection
- `texture.c` - Texture management
- `windows/inspector/*.c` - Property editors

**Files Using Game Interface**:
- `things.c` - Thing rendering (uses `game_get_thing_sprite()`)
- `windows/things.c` - Thing picker UI (uses game interface)

### Game Module (`mapview/game/`)

This module contains all DOOM/Hexen-specific code:

- `thing_info.c` - Implements game abstraction interface
  - Loads sprite frames from `sprnames[]`
  - Looks up thing metadata from `mobjinfo[]` and `states[]`
  - Provides game-specific constants

### Build System

The build system uses conditional compilation (`#ifdef HEXEN`) to select between DOOM and Hexen:

- **DOOM mode**: Compiles with `doom/info.c` data
- **Hexen mode**: Compiles with `hexen/info.c`, `hexen/actions.c`, `hexen/hu_stuff.c` data

The `HEXEN` macro is defined in `mapview/map.h` (line 44).

## Data Flow

### Thing Rendering Example

1. Editor requests sprite for thing type
2. `things.c` calls `game_get_thing_sprite(type, angle)`
3. `game/thing_info.c` looks up sprite in DOOM/Hexen data tables
4. Returns sprite structure to editor
5. Editor renders sprite without knowing game details

```
Editor (things.c)
    ↓ game_get_thing_sprite()
Game Module (game/thing_info.c)
    ↓ accesses mobjinfo[], states[], sprnames[]
DOOM/Hexen Data (info.c)
```

## Benefits

1. **Maintainability** - Game-specific code is isolated and easy to find
2. **Testability** - Editor code can be tested independently of game data
3. **Extensibility** - New games can be supported by implementing the game interface
4. **Clarity** - Clear boundary between editor and game concerns

## Future Improvements

Potential enhancements to the architecture:

1. Move `#ifdef HEXEN` from `map.h` to build system only
2. Create DOOM-specific `ed_config.h` for thing categories (currently Hexen-only)
3. Make game module fully pluggable (runtime selection vs compile-time)
4. Extract more game-specific constants to the game interface
5. Create separate game modules for DOOM, Hexen, Heretic support

## Related Documentation

- [BSP_RENDERING.md](BSP_RENDERING.md) - BSP tree traversal algorithm
- [TRIANGULATION_IMPROVEMENTS.md](TRIANGULATION_IMPROVEMENTS.md) - Polygon triangulation
- [BUILD.md](BUILD.md) - Build instructions
- [tests/TEST_README.md](tests/TEST_README.md) - Test documentation
