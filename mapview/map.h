#ifndef __MAP__
#define __MAP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <cglm/struct.h>

#include "../ui/ui.h"

// We define our own extended version of map_data_t with rendering data
#define MAP_DATA_T_DEFINED

// Include game data structures from libgame
// Note: Some types are redefined below for backward compatibility
// TODO: Remove duplicates in future refactoring
#include "../libgame/libgame.h"

#define OFFSET_OF(type, field) (void*)((size_t)&(((type *)0)->field))

// Macro to define a collection of elements (pointer and count)
// Defined in libgame/map.h - kept here for backward compatibility
#ifndef DEFINE_COLLECTION
#define DEFINE_COLLECTION(type, name) \
type* name;                   \
int num_##name
#endif

// Macro to free a collection and reset its values
// Defined in libgame/map.h - kept here for backward compatibility
#ifndef CLEAR_COLLECTION
#define CLEAR_COLLECTION(map, name) \
if ((map)->name) free((map)->name); \
(map)->name = NULL;               \
(map)->num_##name = 0
#endif

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
#define PALETTE_WIDTH 24
#define NOTEX_SIZE 64
#define SCROLL_SENSITIVITY 5
#define SPRITE_SCALE 2
#define SCALE_POINT(x) ((x)/2)
#define HEXEN
#define HIGHLIGHT(light) ((light)+0.25)
#define CONSOLE_PADDING 2
#define LINE_HEIGHT 8
#define VGA_WIDTH 320
#define VGA_HEGHT 200

#define PLAYER_FOV 90

#define THUMBNAIL_SIZE 64

#define sensitivity_x 0.125f // Adjust sensitivity as needed
#define sensitivity_y 0.175f // Adjust sensitivity as needed

typedef enum {
  icon16_select,
  icon16_points,
  icon16_lines,
  icon16_sectors,
  icon16_things,
  icon16_sounds,
  icon16_appicon,
  icon16_count,
} ed_icon16_t;

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
#define FOV 90.0
#define NEAR_Z 0.1
#define FAR_Z 1000.0
//#define MOVEMENT_SPEED 300.0
#define ROTATION_SPEED 3.0

#define ACCELERATION 1000.0f
#define FRICTION     1200.0f
#define MAX_SPEED    300.0f

// NOTE: The following types are now defined in libgame/map.h and libgame/wad.h
// ML_* enum, wadid_t, lumpname_t, texname_t, NF_SUBSECTOR, mapnode_t

// Player state (editor-specific, not in libgame)
typedef struct {
  float x, y, z;
  float angle;  // in degrees, 0 is east, 90 is north
  float pitch;
  float height;
  float vel_x, vel_y; // Player velocity
  int sector;
  int mouse_x_rel;
  int mouse_y_rel;
  float forward_move;
  float strafe_move;
//  float points[2][2];
//  float points2[2][2];
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

// NOTE: The following types are now defined in libgame/wad.h and libgame/map.h
// wadheader_t, filelump_t, patch_t, mapthing_t, maplinedef_t, mapvertex_t,
// mapsidedef_t, mapsector_t, mapsubsector_t, palette_entry_t, mapseg_t, texdef_t

// Wall section references (editor-specific extensions to game data)
typedef struct {
  mapsidedef_t *def;
  mapsector_t *sector;
  wall_section_t upper_section;
  wall_section_t lower_section;
  wall_section_t mid_section;
//  uint32_t frame;
} mapsidedef2_t;

typedef struct {
  mapsector_t *sector;
  wall_section_t floor;
  wall_section_t ceiling;
  uint32_t frame;
} mapsector2_t;

// NOTE: mapsubsector_t, palette_entry_t, mapseg_t, texdef_t are now defined in libgame

extern palette_entry_t *palette;

// Map data structure using the collection macro
// Extended version with rendering data (matches libgame base + extensions)
typedef struct map_data_s {
  DEFINE_COLLECTION(mapvertex_t, vertices);
  DEFINE_COLLECTION(maplinedef_t, linedefs);
  DEFINE_COLLECTION(mapsidedef_t, sidedefs);
  DEFINE_COLLECTION(mapthing_t, things);
  DEFINE_COLLECTION(mapsector_t, sectors);
  DEFINE_COLLECTION(mapnode_t, nodes);
  DEFINE_COLLECTION(mapsubsector_t, subsectors);
  DEFINE_COLLECTION(mapseg_t, segs);

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
  player_t player;
  uint32_t frame;
  uint16_t portal;
  uint32_t time;
} viewdef_t;

// Collection to store loaded textures
typedef struct texture_cache_s {
  int num_textures;
  texname_t selected;
  mapside_texture_t textures[1];
} texture_cache_t;

enum {
  edit_select,
  edit_vertices,
  edit_lines,
  edit_sectors,
  edit_things,
  edit_sounds,
  edit_modes
};

typedef enum {
  obj_none,
  obj_point,
  obj_line,
  obj_sector,
  obj_thing,
} objtype_t;

typedef struct {
  objtype_t type;
  uint16_t index;
} editor_selection_t;

#define has_selection(s, t) (s.type == t && s.index != 0xFFFF)

typedef struct {
  struct window_s *window;
  int16_t cursor[2];
  vec2 camera;
  int grid_size;         // Grid size (8 units by default)
  bool drawing;          // Currently drawing a sector?
  bool dragging;          // Currently dragging a vertex?
  int move_camera;
  int move_thing;
  int num_draw_points;   // Number of points in current sector
  editor_selection_t hover, selected;
  int sel_mode;
  int selected_thing_type;
  float scale;
  uint32_t vao, vbo;
  mapvertex_t sn;
} editor_state_t;

typedef struct {
  int episode;
  int level;
  uint32_t last_time;
  map_data_t map;
  player_t player;
  editor_state_t state;
} game_t;


#define WINDOW_PADDING 4
#define LINE_PADDING 5
#define CONTROL_HEIGHT 10
#define LABEL_WIDTH 54
#define WIDTH_FILL -1
#define THING_SIZE 48

struct window_s;

typedef struct toolbar_button_s toolbar_button_t;

window_t *create_window(char const *, flags_t, const rect_t*, struct window_s *, winproc_t, void *param);
void show_window(window_t *win, bool visible);
void destroy_window(window_t *win);
void clear_window_children(window_t *win);
void register_window_hook(uint32_t msg, winhook_func_t func, void *userdata);
void load_window_children(window_t *win, windef_t const *def);
int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
void invalidate_window(window_t *win);
void set_window_item_text(window_t *win, uint32_t id, const char *fmt, ...);
int window_title_bar_y(window_t const *win);
window_t *get_window_item(window_t const *win, uint32_t id);
void track_mouse(window_t *win);
void set_capture(window_t *win);
void set_focus(window_t* win);
void enable_window(window_t *win, bool enable);

typedef union SDL_Event SDL_Event;
int get_message(SDL_Event *evt);
void dispatch_message(SDL_Event *evt);
void repost_messages(void);
bool is_window(window_t *win);
void end_dialog(window_t *win, uint32_t code);
uint32_t show_dialog(char const *, const rect_t*, struct window_s *, winproc_t, void *param);

extern window_t *g_inspector;
extern game_t *g_game;

bool init_sdl(void);
int run(void);

filelump_t *find_lump(const char* name);
void *cache_lump(const char* name);
void *cache_lump_num(uint16_t i);
int find_lump_num(const char* name);
char const *get_lump_name(int i);

int allocate_mapside_textures(void);
int allocate_flat_textures(void);

uint8_t* load_patch(void* patch_lump, int* width, int* height);
bool point_in_sector(map_data_t const* map, int x, int y, int sector_index);
void assign_thing_sector(map_data_t const *map, mapthing_t *thing);
mapsector_t const *find_player_sector(map_data_t const* map, int x, int y);
mapside_texture_t const *get_texture(const char* name);
mapside_texture_t const *get_flat_texture(const char* name);
void build_wall_vertex_buffer(map_data_t *map);
void build_floor_vertex_buffer(map_data_t *map);
void draw_textured_surface(wall_section_t const *surface, float light, int mode);
void draw_textured_surface_id(wall_section_t const *surface, uint32_t id, int mode);
void draw_bsp(map_data_t const *map, viewdef_t const *viewdef);

void update_player_position_with_sliding(map_data_t const *map, player_t *player,
                                         float move_x, float move_y);

void fill_rect(int color, int x, int y, int w, int h);
void draw_rect(int tex, int x, int y, int w, int h);
void draw_rect_ex(int tex, int x, int y, int w, int h, int type, float alpha);
void draw_icon8(int icon, int x, int y, uint32_t col);
void draw_icon16(int icon, int x, int y, uint32_t col);
void draw_palette(map_data_t const *map);
char const* get_texture_name(int i);
char const* get_flat_texture_name(int i);
int get_texture_index(char const* name);
int get_flat_texture_index(char const* name);
float dist_sq(float x1, float y1, float x2, float y2);

map_data_t load_map(const char* map_name);
void find_all_maps(void (*proc)(const char *, void *), void *parm);
void print_map_info(map_data_t* map);
void free_map_data(map_data_t* map);

void goto_intermisson(void);
void new_map(void);
void open_map(const char *mapname);

bool init_wad(const char *filename);
void shutdown_wad(void);

void draw_windows(bool rich);
void handle_windows(void);

void GetMouseInVirtualCoords(int* vx, int* vy);

const char *get_selected_texture(void);
void set_selected_texture(const char *);
const char *get_selected_flat_texture(void);
void set_selected_flat_texture(const char *str);

extern int world_prog_mvp;
extern int world_prog_viewPos;
extern int world_prog_tex0_size;
extern int world_prog_tex0;
extern int world_prog_light;

extern int ui_prog_mvp;
extern int ui_prog_tex0_size;
extern int ui_prog_tex0;
extern int ui_prog_color;


#endif
