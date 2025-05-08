#ifndef __MAP__
#define __MAP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <cglm/struct.h>

#define OFFSET_OF(type, field) (void*)((size_t)&(((type *)0)->field))

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

#define EYE_HEIGHT 48 // Typical eye height in Doom is 41 units above floor
#define MAX_WALL_VERTICES 50000  // Adjust based on map complexity
#define P_RADIUS 12.0f        // Player radius

#define PALETTE_WIDTH 64
#define NOTEX_SIZE 64

//#define HEXEN

#define HIGHLIGHT(light) light//((light)+0.25)

enum {
  PIXEL_MID = 0 << 28,
  PIXEL_BOTTOM = 1 << 28,
  PIXEL_TOP = 2 << 28,
  PIXEL_FLOOR = 3 << 28,
  PIXEL_CEILING = 4 << 28,
  PIXEL_MASK = 7 << 28,
};

#define CHECK_PIXEL(PIXEL, TYPE, ID) ((PIXEL & PIXEL_MASK) == PIXEL_##TYPE && (PIXEL & ~PIXEL_MASK) == ID)


// Constants for rendering
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define FOV 90.0
#define NEAR_Z 0.1
#define FAR_Z 1000.0
#define MOVEMENT_SPEED 400.0
#define ROTATION_SPEED 3.0

// Type definitions to better represent the WAD format
typedef char wadid_t[4];     // "IWAD" or "PWAD"
typedef char lumpname_t[8];  // Lump name, null-terminated
typedef char texname_t[8];   // Texture name, null-terminated

// Player state
typedef struct {
  float x, y, z;
  float angle;  // in degrees, 0 is east, 90 is north
  float pitch;
  float height;
} player_t;

// Vertex structure for our buffer (xyzuv)
typedef struct {
  int16_t x, y, z;    // Position
  int16_t u, v;       // Texture coordinates
  int8_t nx, ny, nz;  // Normal
  int32_t color;
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
  uint32_t vertex_count;  // Count of vertices
  mapside_texture_t const *texture;  // Texture to use
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

// Patch header structure
typedef struct {
  int16_t width;              // Width of the patch
  int16_t height;             // Height of the patch
  int16_t leftoffset;         // Left offset of the patch
  int16_t topoffset;          // Top offset of the patch
  int32_t columnofs[1];       // Column offsets (variable size)
} patch_t;

#ifdef HEXEN
// Thing structure
typedef struct {
  int16_t tid;
  int16_t x;
  int16_t y;
  int16_t flags;
  int16_t angle;
  int16_t type;
  int16_t options;
  int8_t special;
  int8_t arg1;
  int8_t arg2;
  int8_t arg3;
  int8_t arg4;
  int8_t arg5;
} mapthing_t;
// Linedef structure
typedef struct {
  uint16_t start;             // Start vertex
  uint16_t end;               // End vertex
  uint16_t flags;             // Flags
  uint8_t special;           // Special type
  uint8_t args[5];           // Arguments for special (e.g., script number, delay)
  uint16_t sidenum[2];        // Sidedef numbers
} maplinedef_t;
#else
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
#endif

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
} mapsidedef2_t;

typedef struct {
  mapsector_t *sector;
  wall_section_t floor;
  wall_section_t ceiling;
  uint32_t frame;
} mapsector2_t;

// Palette structure
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} palette_entry_t;

extern palette_entry_t *palette;

// Map data structure using the collection macro
typedef struct {
  DEFINE_COLLECTION(mapvertex_t, vertices);
  DEFINE_COLLECTION(maplinedef_t, linedefs);
  DEFINE_COLLECTION(mapsidedef_t, sidedefs);
  DEFINE_COLLECTION(mapthing_t, things);
  DEFINE_COLLECTION(mapsector_t, sectors);
  
  struct {
    mapsidedef2_t *sections;
    wall_vertex_t vertices[MAX_WALL_VERTICES];
    uint32_t num_vertices;
    uint32_t vao, vbo;
  } walls;
  
  struct {
    mapsector2_t *sectors;
    wall_vertex_t vertices[MAX_WALL_VERTICES];
    uint32_t num_vertices;
    uint32_t vao, vbo;
  } floors;
} map_data_t;

typedef struct {
  mat4 mvp;
  vec4 frustum[6];
  vec3 viewpos;
  uint32_t frame;
  bool nowalls;
} viewdef_t;

typedef enum {
  GS_DUNGEON,
  GS_EDITOR,
  GS_WORLD
} gameState_t;

typedef struct {
  int episode;
  int level;
  gameState_t state;
  map_data_t map;
  player_t player;
} game_t;

extern game_t game;

bool init_sdl(void);
int run(void);

filelump_t *find_lump(const char* name);
void *cache_lump(const char* name);
int find_lump_num(const char* name);
char const *get_lump_name(int i);

int allocate_mapside_textures(map_data_t* map);
int allocate_flat_textures(map_data_t* map);

uint8_t* load_patch(void* patch_lump, int* width, int* height);
mapsector_t const *find_player_sector(map_data_t const* map, int x, int y);
mapside_texture_t *get_texture(const char* name);
void build_wall_vertex_buffer(map_data_t *map);
void build_floor_vertex_buffer(map_data_t *map);
void handle_game_input(float delta_time);
void draw_textured_surface(wall_section_t const *surface, float light, int mode);
void draw_textured_surface_id(wall_section_t const *surface, uint32_t id, int mode);

void update_player_position_with_sliding(map_data_t const *map, player_t *player,
                                         float move_x, float move_y);

void draw_rect(int tex, float x, float y, float w, float h);
void draw_palette(map_data_t const *map, float x, float y, int w, int h);
char const* get_texture_name(int i);
char const* get_flat_texture_name(int i);
float dist_sq(float x1, float y1, float x2, float y2);

map_data_t load_map(const char* map_name);
void find_all_maps(void);
void print_map_info(map_data_t* map);
void free_map_data(map_data_t* map);

void goto_intermisson(void);
void goto_map(const char *mapname);

bool init_wad(const char *filename);
void shutdown_wad(void);

void GetMouseInVirtualCoords(int* vx, int* vy);

#endif
