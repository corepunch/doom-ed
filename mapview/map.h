#ifndef __MAP__
#define __MAP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Macro to define a collection of elements (pointer and count)
#define DEFINE_COLLECTION(type, name) \
type* name;                   \
int num_##name

// Macro to free a collection and reset its values
#define CLEAR_COLLECTION(map, name) \
if ((map)->name) free((map)->name); \
(map)->name = NULL;               \
(map)->num_##name = 0

// Helper macros for min/max if not already defined
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define BRIGHTNESS 1.1f
#define EYE_HEIGHT 41 // Typical eye height in Doom is 41 units above floor
#define MAX_WALL_VERTICES 50000  // Adjust based on map complexity

// Constants for rendering
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FOV 90.0
#define NEAR_Z 0.1
#define FAR_Z 1000.0
#define MOVEMENT_SPEED 10.0
#define ROTATION_SPEED 3.0

// Type definitions to better represent the WAD format
typedef char wadid_t[4];     // "IWAD" or "PWAD"
typedef char lumpname_t[8];  // Lump name, null-terminated
typedef char texname_t[8];   // Texture name, null-terminated

// Player state
typedef struct {
  float x;
  float y;
  float angle;  // in degrees, 0 is east, 90 is north
  float pitch;
  float height;
} player_t;

// Vertex structure for our buffer (xyzuv)
typedef struct {
  float x, y, z;  // Position
  float u, v;     // Texture coordinates
} wall_vertex_t;

// Struct to represent a texture with OpenGL
typedef struct {
  texname_t name;
  uint32_t texture;
  uint16_t width;
  uint16_t height;
} mapside_texture_t;

// Helper struct for tracking wall sections
typedef struct {
  uint32_t vertex_start;  // Starting index in the vertex buffer
  mapside_texture_t *texture;  // Texture to use
} wall_section_t;

// WAD header structure
typedef struct {
  wadid_t identification;     // Should be "IWAD" or "PWAD"
  uint32_t numlumps;           // Number of lumps in the WAD
  uint32_t infotableofs;       // Pointer to the location of the directory
} wadheader_t;

// WAD directory entry structure
typedef struct {
  uint32_t filepos;           // File offset of the lump
  uint32_t size;              // Size of the lump in bytes
  lumpname_t name;           // Name of the lump, null-terminated
} filelump_t;

// Thing structure
typedef struct {
  int16_t x;                 // X position
  int16_t y;                 // Y position
  int16_t angle;             // Angle facing
  int16_t type;              // Thing type
  int16_t flags;             // Flags
} mapthing_t;

// Linedef structure
typedef struct {
  uint16_t start;             // Start vertex
  uint16_t end;               // End vertex
  uint16_t flags;             // Flags
  uint16_t special;           // Special type
  uint16_t tag;               // Tag number
  uint16_t sidenum[2];        // Sidedef numbers
} maplinedef_t;

// Vertex structure
typedef struct {
  int16_t x;                 // X coordinate
  int16_t y;                 // Y coordinate
} mapvertex_t;

// Sidedef structure
typedef struct {
  int16_t textureoffset;     // X offset for texture
  int16_t rowoffset;         // Y offset for texture
  texname_t toptexture;      // Name of upper texture
  texname_t bottomtexture;   // Name of lower texture
  texname_t midtexture;      // Name of middle texture
  uint16_t sector;           // Sector number this sidedef faces
} mapsidedef_t;

// Sector structure
typedef struct {
  int16_t floorheight;       // Floor height
  int16_t ceilingheight;     // Ceiling height
  texname_t floorpic;        // Name of floor texture
  texname_t ceilingpic;      // Name of ceiling texture
  int16_t lightlevel;        // Light level
  int16_t special;           // Special behavior
  int16_t tag;               // Tag number
} mapsector_t;

// Wall section references
typedef struct {
  mapsidedef_t *def;
  mapsector_t *sector;
  wall_section_t upper_section;
  wall_section_t lower_section;
  wall_section_t mid_section;
} mapsidedef_sections_t;

// Map data structure using the collection macro
typedef struct {
  DEFINE_COLLECTION(mapvertex_t, vertices);
  DEFINE_COLLECTION(maplinedef_t, linedefs);
  DEFINE_COLLECTION(mapsidedef_t, sidedefs);
  DEFINE_COLLECTION(mapthing_t, things);
  DEFINE_COLLECTION(mapsector_t, sectors);
  
  struct {
    mapsidedef_sections_t *sections;
    wall_vertex_t vertices[MAX_WALL_VERTICES];
    uint32_t num_vertices;
    uint32_t vao, vbo;
  } walls;
} map_data_t;

bool init_sdl(void);
int run(map_data_t const *map);

int find_lump(filelump_t* directory, int num_lumps, const char* name);
int allocate_mapside_textures(map_data_t* map, FILE* wad_file,
                              filelump_t* directory, int num_lumps);
int allocate_flat_textures(map_data_t* map, FILE* wad_file,
                           filelump_t* directory, int num_lumps);
mapsector_t const *find_player_sector(map_data_t const* map, int x, int y);
mapside_texture_t *get_texture(const char* name);
void build_wall_vertex_buffer(map_data_t *map);
void handle_input(player_t *player);

#endif
