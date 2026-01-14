# Code Architecture - Visual Overview

## Before Refactoring

```
┌─────────────────────────────────────────────────────────────┐
│                        mapview/                             │
│  ┌────────────┬────────────┬────────────┬────────────┐     │
│  │  WAD I/O   │  Rendering │  Editor    │  UI Code   │     │
│  │  Map Data  │  BSP/Floor │  Input     │  Windows   │     │
│  │  Game Defs │  Walls/Sky │  Selection │  Inspector │     │
│  └────────────┴────────────┴────────────┴────────────┘     │
│                     All Mixed Together                      │
└─────────────────────────────────────────────────────────────┘
                              +
┌─────────────────────────────────────────────────────────────┐
│           doom/ + hexen/ (game engine code)                 │
└─────────────────────────────────────────────────────────────┘
```

**Problems:**
- ❌ Hard to reuse game data code
- ❌ Difficult to test components
- ❌ Unclear boundaries
- ❌ High coupling

## After Refactoring

```
┌──────────────────────────────────────────────────────────────┐
│                      DOOM-ED Editor                          │
│                        (mapview/)                            │
│  ┌────────────────────────────────────────────────────┐     │
│  │  Editor-Specific Code                              │     │
│  │  • Input handling (editor_input.c)                 │     │
│  │  • Selection logic (editor_sector.c)               │     │
│  │  • Inspector windows (windows/inspector/)          │     │
│  │  • Editor drawing (editor_draw.c)                  │     │
│  └────────────────────────────────────────────────────┘     │
│                           ↓ uses                            │
│  ┌──────────────────────┬──────────────────────────────┐   │
│  │   Rendering Code     │    UI Framework              │   │
│  │   • BSP traversal    │    • Windows                 │   │
│  │   • Triangulation    │    • Controls                │   │
│  │   • Floor/Wall/Sky   │    • Events                  │   │
│  │   • Textures         │    (libdesktop.a)            │   │
│  └──────────────────────┴──────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
                              ↓ uses
┌──────────────────────────────────────────────────────────────┐
│                      libgame.a (NEW!)                        │
│  ┌────────────────────────────────────────────────────┐     │
│  │  Game Data & WAD I/O                               │     │
│  │  • WAD file reading/writing (wad.c)                │     │
│  │  • Map structures (map.h)                          │     │
│  │  • DOOM/Hexen formats                              │     │
│  │  • No rendering dependencies!                      │     │
│  └────────────────────────────────────────────────────┘     │
│                  Can be used by:                            │
│          ┌──────────┬──────────┬──────────┐                │
│          │ doom-ed  │  mapinfo │  Other   │                │
│          │ (editor) │  (tool)  │  Tools   │                │
│          └──────────┴──────────┴──────────┘                │
└──────────────────────────────────────────────────────────────┘
```

**Benefits:**
- ✅ Modular design
- ✅ Reusable components
- ✅ Easy to test
- ✅ Clear boundaries
- ✅ Low coupling

## Dependency Graph

```
libgame.a (no dependencies)
    ↓
    ├→ libdesktop.a (uses SDL2, OpenGL)
    │       ↓
    └→ doom-ed (uses libgame + libdesktop)
            ↓
        Editor functionality

Alternative uses:
libgame.a
    ├→ mapinfo (standalone tool)
    ├→ wad-validator (future tool)
    ├→ map-converter (future tool)
    └→ batch-processor (future tool)
```

## File Organization

```
doom-ed/
├── libgame/              🆕 Game data library
│   ├── libgame.h         • Main header
│   ├── wad.h/c           • WAD file I/O
│   ├── map.h             • Map structures
│   └── README.md         • API documentation
│
├── ui/                   ✅ UI framework (already separated)
│   ├── commctl/          • Common controls
│   ├── user/             • User interface primitives
│   └── kernel/           • Event handling
│
├── mapview/              📝 Editor application
│   ├── editor*.c         • Editor logic
│   ├── bsp.c, floor.c    • Rendering (future: librender/)
│   ├── texture.c         • Texture management
│   └── windows/          • UI windows
│       ├── desktop.c
│       ├── inspector/    • Property inspectors
│       └── ...
│
├── examples/             🆕 Example tools
│   └── mapinfo.c         • Standalone map info tool
│
├── doom/, hexen/         ✅ Game engine headers
│
└── docs/                 📚 Documentation
    ├── README.md
    ├── ARCHITECTURE.md         🆕
    ├── CODE_SPLIT_SUMMARY.md   🆕
    └── IMPLEMENTATION_COMPLETE.md  🆕
```

## Build Process

```
┌─────────────────────────────────────────────────────┐
│  Step 1: Build libgame                              │
│  gcc -c libgame/wad.c -o build/libgame/wad.o        │
│  ar rcs build/libgame.a build/libgame/wad.o         │
└─────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────┐
│  Step 2: Build libdesktop                           │
│  gcc -c ui/*.c -o build/ui/*.o                      │
│  ar rcs build/libdesktop.a build/ui/*.o             │
└─────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────┐
│  Step 3: Build doom-ed                              │
│  gcc mapview/*.c hexen/*.c                          │
│      -Ilibgame -Iui -Idoom -Ihexen                  │
│      -o doom-ed                                     │
│      build/libgame.a build/libdesktop.a             │
│      -lSDL2 -lGL -lm                                │
└─────────────────────────────────────────────────────┘
```

## Usage Examples

### Using libgame in doom-ed (Editor)

```c
// In mapview/main.c
#include <libgame/libgame.h>

// Load WAD and map
init_wad("doom.wad");
map_data_t map = load_map("E1M1");

// Editor extends map with rendering data
// (see mapview/map.h for extended map_data_t)

// Use map data...
draw_map(&map);
handle_input(&map);

// Clean up
free_map_data(&map);
shutdown_wad();
```

### Using libgame in mapinfo (Standalone Tool)

```c
// In examples/mapinfo.c
#include <libgame/libgame.h>

// Just load and analyze - no rendering!
init_wad("doom.wad");
map_data_t map = load_map("E1M1");

printf("Vertices: %d\n", map.num_vertices);
printf("Sectors: %d\n", map.num_sectors);

free_map_data(&map);
shutdown_wad();
```

## API Surface

### libgame Public API

```c
// WAD File Operations
bool init_wad(const char *filename);
void shutdown_wad(void);
filelump_t *find_lump(const char* name);
void *cache_lump(const char* name);
void find_all_maps(void (*proc)(const char *, void *), void *parm);

// Map Operations
map_data_t load_map(const char* map_name);
void free_map_data(map_data_t* map);
void print_map_info(map_data_t* map);

// Data Structures
typedef struct { ... } mapvertex_t;
typedef struct { ... } mapsector_t;
typedef struct { ... } mapsidedef_t;
typedef struct { ... } maplinedef_t;
typedef struct { ... } mapthing_t;
typedef struct { ... } mapnode_t;
typedef struct { ... } map_data_t;
```

## Testing Strategy

```
┌──────────────────────────────────────┐
│  Unit Tests (libgame)                │
│  • Test WAD loading                  │
│  • Test map parsing                  │
│  • Test data structures              │
│  • No rendering dependencies!        │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│  Integration Tests (doom-ed)         │
│  • Test editor with libgame          │
│  • Test rendering with game data     │
│  • Test UI with map data             │
└──────────────────────────────────────┘
              ↓
┌──────────────────────────────────────┐
│  End-to-End Tests                    │
│  • Load WAD, edit map, save          │
│  • Full editor workflow              │
└──────────────────────────────────────┘
```

## Future Enhancements

### Phase Next: Extract librender

```
librender/
├── bsp.c          # BSP traversal
├── triangulate.c  # Polygon triangulation
├── floor.c        # Floor rendering
├── walls.c        # Wall rendering
├── sky.c          # Sky rendering
└── texture.c      # Texture management

Dependencies: librender → libgame
```

### Final Architecture Goal

```
libgame.a (game data)
    ↓
librender.a (rendering)
    ↓
libdesktop.a (UI)
    ↓
doom-ed (editor)
```

## Key Principles

1. **Separation of Concerns**
   - Game data ≠ Rendering ≠ Editing

2. **Dependency Direction**
   - Low-level → High-level
   - Never circular

3. **Minimal Dependencies**
   - libgame: only libc
   - librender: libgame + OpenGL
   - editor: everything

4. **Reusability**
   - Each library usable independently
   - Clean public APIs

5. **Backward Compatibility**
   - Existing code continues to work
   - Gradual migration path

## Summary

**Before:** Monolithic editor with mixed concerns  
**After:** Modular architecture with clear boundaries  

**Impact:**
- 🎯 Better organization
- 🔧 Easier maintenance
- 🧪 Better testability
- ♻️  Code reusability
- 📚 Clear documentation

The foundation for a professional, maintainable DOOM editor is now in place! 🚀
