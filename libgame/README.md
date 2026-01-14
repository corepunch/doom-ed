# libgame - DOOM Game Data Library

A standalone C library for working with DOOM WAD files and map data.

## Features

- WAD file I/O (IWAD and PWAD support)
- Map data structure definitions
- Support for DOOM, DOOM II, and Hexen formats
- No dependencies on rendering or UI libraries

## Building

libgame is built as part of the main DOOM-ED project:

```bash
make libgame
```

This creates `build/libgame.a`.

## Usage Example

```c
#include <stdio.h>
#include <libgame/libgame.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wadfile>\n", argv[0]);
        return 1;
    }

    // Initialize WAD file
    if (!init_wad(argv[1])) {
        fprintf(stderr, "Failed to open WAD file\n");
        return 1;
    }

    // Load a map
    map_data_t map = load_map("E1M1");
    if (map.num_vertices == 0) {
        fprintf(stderr, "Failed to load map\n");
        shutdown_wad();
        return 1;
    }

    // Print map statistics
    printf("Map E1M1 Statistics:\n");
    printf("  Vertices:  %d\n", map.num_vertices);
    printf("  Linedefs:  %d\n", map.num_linedefs);
    printf("  Sidedefs:  %d\n", map.num_sidedefs);
    printf("  Sectors:   %d\n", map.num_sectors);
    printf("  Things:    %d\n", map.num_things);
    printf("  Nodes:     %d\n", map.num_nodes);

    // Access specific map elements
    printf("\nFirst 5 sectors:\n");
    for (int i = 0; i < 5 && i < map.num_sectors; i++) {
        printf("  Sector %d: floor=%d, ceiling=%d, light=%d\n",
               i,
               map.sectors[i].floorheight,
               map.sectors[i].ceilingheight,
               map.sectors[i].lightlevel);
    }

    // Find player start
    printf("\nPlayer starts:\n");
    for (int i = 0; i < map.num_things; i++) {
        if (map.things[i].type == 1) {
            printf("  Position: (%d, %d), Angle: %d\n",
                   map.things[i].x,
                   map.things[i].y,
                   map.things[i].angle);
        }
    }

    // Clean up
    free_map_data(&map);
    shutdown_wad();

    return 0;
}
```

Compile:
```bash
gcc -o mapinfo mapinfo.c -Ilibgame -Lbuild -lgame
```

## API Reference

### WAD File Functions

#### `bool init_wad(const char *filename)`
Initialize WAD file for reading.
- **Parameters**: Path to WAD file (IWAD or PWAD)
- **Returns**: true on success, false on failure

#### `void shutdown_wad(void)`
Close WAD file and free resources.

#### `filelump_t *find_lump(const char* name)`
Find a lump by name.
- **Parameters**: Lump name (up to 8 characters)
- **Returns**: Pointer to lump descriptor, or NULL if not found

#### `void *cache_lump(const char* name)`
Load and cache a lump.
- **Parameters**: Lump name
- **Returns**: Pointer to lump data, or NULL if not found

#### `void find_all_maps(void (*proc)(const char *, void *), void *parm)`
Iterate over all maps in the WAD.
- **Parameters**: Callback function, user data pointer

### Map Functions

#### `map_data_t load_map(const char* map_name)`
Load a map by name.
- **Parameters**: Map name (e.g., "E1M1" or "MAP01")
- **Returns**: Populated map_data_t structure

#### `void free_map_data(map_data_t* map)`
Free map data.
- **Parameters**: Pointer to map_data_t structure

#### `void print_map_info(map_data_t* map)`
Print basic map statistics to stdout.
- **Parameters**: Pointer to map_data_t structure

## Data Structures

### `map_data_t`
Main container for map data:
```c
typedef struct {
    mapvertex_t *vertices;    // Array of vertices
    int num_vertices;         // Number of vertices
    
    maplinedef_t *linedefs;   // Array of linedefs
    int num_linedefs;         // Number of linedefs
    
    mapsidedef_t *sidedefs;   // Array of sidedefs
    int num_sidedefs;         // Number of sidedefs
    
    mapsector_t *sectors;     // Array of sectors
    int num_sectors;          // Number of sectors
    
    mapthing_t *things;       // Array of things
    int num_things;           // Number of things
    
    mapnode_t *nodes;         // BSP nodes
    int num_nodes;            // Number of nodes
    
    mapsubsector_t *subsectors; // BSP subsectors
    int num_subsectors;         // Number of subsectors
    
    mapseg_t *segs;           // Line segments
    int num_segs;             // Number of segments
} map_data_t;
```

### `mapvertex_t`
2D vertex:
```c
typedef struct {
    int16_t x;  // X coordinate
    int16_t y;  // Y coordinate
} mapvertex_t;
```

### `mapsector_t`
Sector (floor/ceiling area):
```c
typedef struct {
    int16_t floorheight;      // Floor height
    int16_t ceilingheight;    // Ceiling height
    texname_t floorpic;       // Floor texture
    texname_t ceilingpic;     // Ceiling texture
    int16_t lightlevel;       // Light level (0-255)
    int16_t special;          // Special behavior
    int16_t tag;              // Tag for triggers
} mapsector_t;
```

### `maplinedef_t`
Line definition:
```c
typedef struct {
    uint16_t start;      // Start vertex index
    uint16_t end;        // End vertex index
    uint16_t flags;      // Line flags
    uint16_t special;    // Special action
    uint16_t tag;        // Tag number
    uint16_t sidenum[2]; // Front/back sidedef indices
} maplinedef_t;
```

### `mapsidedef_t`
Side definition (wall texturing):
```c
typedef struct {
    int16_t textureoffset;    // X texture offset
    int16_t rowoffset;        // Y texture offset
    texname_t toptexture;     // Upper texture
    texname_t bottomtexture;  // Lower texture
    texname_t midtexture;     // Middle texture
    uint16_t sector;          // Sector reference
} mapsidedef_t;
```

### `mapthing_t`
Thing (monster, item, player start):
```c
typedef struct {
    int16_t x;       // X position
    int16_t y;       // Y position
    int16_t angle;   // Facing angle (0-359)
    int16_t type;    // Thing type ID
    int16_t flags;   // Spawn flags
} mapthing_t;
```

## Building Tools with libgame

libgame is designed to be used by various DOOM-related tools:

### Map Analyzers
```c
// Count enemies in a map
int count_enemies(map_data_t *map) {
    int count = 0;
    for (int i = 0; i < map->num_things; i++) {
        if (is_enemy(map->things[i].type)) {
            count++;
        }
    }
    return count;
}
```

### WAD Validators
```c
// Validate map structure
bool validate_map(map_data_t *map) {
    // Check all linedefs reference valid vertices
    for (int i = 0; i < map->num_linedefs; i++) {
        if (map->linedefs[i].start >= map->num_vertices ||
            map->linedefs[i].end >= map->num_vertices) {
            return false;
        }
    }
    return true;
}
```

### Batch Processors
```c
// Process all maps in a WAD
void process_all_maps(void) {
    find_all_maps(process_one_map, NULL);
}

void process_one_map(const char *name, void *userdata) {
    map_data_t map = load_map(name);
    // ... process map ...
    free_map_data(&map);
}
```

## Conditional Compilation

libgame supports both DOOM and Hexen formats via `#ifdef HEXEN`:

- **DOOM**: Standard DOOM/DOOM II format
- **HEXEN**: Extended format with additional thing/linedef fields

## Dependencies

libgame has minimal dependencies:
- Standard C library (stdio, stdlib, string, stdint, stdbool)
- No SDL, OpenGL, or UI libraries required

This makes it suitable for:
- Command-line tools
- Server applications
- Embedded use in other editors
- Batch processing scripts

## License

Same as DOOM-ED (GPL v3)
