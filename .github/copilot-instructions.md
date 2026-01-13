# GitHub Copilot Instructions for DOOM-ED

## Project Overview

DOOM-ED is a modern open-source level editor for classic DOOM games, built with SDL2 and C. The project provides a full-featured level editor that supports DOOM, DOOM II, Heretic, and other DOOM engine games with PWAD/IWAD file formats.

## Key Components

- **Map Rendering**: BSP (Binary Space Partitioning) tree traversal for efficient sector rendering
- **Level Editor**: 2D and 3D real-time preview with vertex, line, sector, and thing editing modes
- **WAD File Support**: Reading and writing DOOM WAD files (PWAD/IWAD formats)
- **Rendering Engine**: OpenGL-based rendering with texture management
- **Geometry Processing**: Polygon triangulation and BSP tree traversal

## Build System

- **Primary**: Xcode project (`mapview.xcodeproj`) for macOS development
- **Platform**: Currently macOS-focused; cross-platform support is a goal
- **Dependencies**: SDL2, OpenGL
- **Language**: C (not C++, despite what README may suggest)

## Testing

### Building and Running Tests

Tests are located in the `mapview/` directory. Build and run them as follows:

**Triangulation Tests:**
```bash
cd mapview
gcc -DTEST_MODE -o triangulate_test triangulate_test.c triangulate_standalone.c -I. -lm
./triangulate_test
```

**BSP Tests:**
```bash
cd mapview
gcc -o bsp_test bsp_test.c -I. -lm
./bsp_test
```

### Test Philosophy

- Tests verify core algorithms (triangulation, BSP traversal, geometry)
- All tests must pass before changes are merged
- Test files: `triangulate_test.c`, `bsp_test.c`
- See `mapview/TEST_README.md` for detailed test documentation

## Coding Conventions

### Style Guidelines

- **Indentation**: Use 2 spaces for indentation (no tabs)
- **Naming**: Use snake_case for functions and variables
- **Headers**: Include header guards in the format `#ifndef __FILENAME_H__`
- **Comments**: Use meaningful comments for complex algorithms; avoid obvious comments
- **Constants**: Use `#define` for compile-time constants in SCREAMING_SNAKE_CASE

### Code Patterns

- **Memory Management**: Use `malloc`/`free` explicitly; check return values
- **Error Handling**: Return error codes or NULL on failure; document expected behavior
- **Fixed-Point Math**: DOOM engine uses fixed-point arithmetic for coordinates (16.16 fixed point)
- **Type Definitions**: Follow existing type naming (e.g., `map_data_t`, `editor_state_t`, `mapvertex_t`)

### Common Types

- `mapvertex_t`: Map vertex with x, y coordinates
- `mapsector_t`: Sector with floor/ceiling heights and textures
- `maplinedef_t`: Line definition connecting two vertices
- `mapsidedef_t`: Side definition with texture information
- `mapthing_t`: Thing (object/monster/item) placement
- `subsector_t`: BSP subsector
- `node_t`: BSP node

## Project Structure

**Note**: The actual repository structure differs from what's described in README.md. The structure below reflects what currently exists. When adding new features, follow the existing structure rather than the README's aspirational layout.

```
/
├── mapview/              # Main editor source code
│   ├── *.c              # Implementation files
│   ├── *.h              # Header files
│   ├── *_test.c         # Test files
│   └── windows/         # Window/UI subsystem code
│       └── inspector/   # Property inspector windows
├── doom/                # DOOM game engine headers and some source
├── hexen/               # Hexen game engine headers
├── gldoom/              # OpenGL DOOM headers
├── screenshots/         # Screenshot images
└── mapview.xcodeproj/   # Xcode project files
```

The README.md mentions `src/`, `include/`, `resources/`, `lua/`, and `docs/` directories that don't currently exist. New code should follow the existing mapview/ structure.

## Important Implementation Details

### BSP Rendering

- The project uses BSP tree traversal for efficient rendering (see `bsp.c`)
- BSP data is loaded from WAD files and must be validated before use
- Automatic fallback to portal rendering if BSP data is unavailable
- Front-to-back traversal order for proper visibility determination
- See `BSP_RENDERING.md` for detailed algorithm documentation

### Polygon Triangulation

- Ear clipping algorithm for sector triangulation (see `triangulate.c`)
- Handles both convex and concave polygons
- Supports both clockwise and counter-clockwise winding orders
- Uses epsilon comparisons for floating-point robustness
- See `TRIANGULATION_IMPROVEMENTS.md` for implementation details

### WAD File Format

- WAD files contain lumps (data chunks) with 8-character names
- Map data consists of: THINGS, LINEDEFS, SIDEDEFS, VERTEXES, SEGS, SSECTORS, NODES, SECTORS, etc.
- All data is stored in little-endian format
- Fixed-point coordinates use FRACUNIT (65536) as the scale factor

### Editor State

- Editor maintains separate states for different edit modes (vertex, line, sector, thing)
- Undo/redo system tracks changes
- Real-time preview updates as geometry changes
- Selection system supports multi-select with modifier keys

## Common Tasks

### Adding a New Map Element

1. Update the appropriate array in `map_data_t`
2. Implement add/remove functions (e.g., `add_vertex`, `add_linedef`)
3. Update rendering code to display the new element
4. Add undo/redo support for the operation
5. Update serialization code for WAD file I/O

### Modifying Rendering

1. Check if change affects BSP traversal or portal rendering
2. Update appropriate rendering function (`bsp.c`, `floor.c`, `walls.c`, etc.)
3. Test with maps both with and without BSP data
4. Verify performance impact on large maps
5. Add visual tests if possible

### Adding Tests

1. Follow existing test patterns in `*_test.c` files
2. Include edge cases and boundary conditions
3. Test both valid and invalid inputs
4. Verify expected output format
5. Update `TEST_README.md` with new test documentation

## Dependencies and Libraries

- **SDL2**: Window management, input, and 2D graphics
- **OpenGL**: 3D rendering and texture management
- **Standard C Library**: Math (`-lm`), standard I/O, memory management

## Platform-Specific Considerations

- **macOS**: Primary platform; uses Xcode project; SDL2 dylib included in repository
- **Linux/Windows**: README.md describes CMake build instructions, but no CMakeLists.txt files exist in the repository currently
- **Byte Order**: All WAD data is little-endian; handle conversions if needed on big-endian systems

## Performance Considerations

- BSP traversal is O(log n) for visibility determination
- Triangulation is O(n²) worst case; optimize for large sectors if needed
- Texture caching important for rendering performance
- Frame tracking prevents duplicate sector rendering

## Documentation

- Keep `README.md` updated with user-facing changes
- Document algorithms in separate markdown files (e.g., `BSP_RENDERING.md`, `TRIANGULATION_IMPROVEMENTS.md`)
  - Existing algorithm docs: `BSP_RENDERING.md`, `TRIANGULATION_IMPROVEMENTS.md`, `PR_SUMMARY.md`
- Update `mapview/TEST_README.md` when adding or modifying tests
- Add comments in code for complex algorithms or non-obvious logic

## When Making Changes

1. **Understand the context**: DOOM-ED works with 30-year-old WAD file formats; maintain compatibility
2. **Test thoroughly**: Run existing tests and add new ones for new features
3. **Preserve behavior**: The editor must correctly read/write standard DOOM WAD files
4. **Consider performance**: Large maps can have thousands of sectors/linedefs
5. **Follow conventions**: Match existing code style and patterns
6. **Document complexity**: Add comments for DOOM engine quirks or complex algorithms
