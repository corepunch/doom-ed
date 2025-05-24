//
//  editor.h
//  mapview
//
//  Created by Igor Chernakov on 28.04.25.
//

#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "map.h"

// Add to map.h at the end before the closing #endif
#define MAX_DRAW_POINTS 64

#define ED_SCROLL 16

typedef enum {
  icon16_points,
  icon16_lines,
  icon16_sectors,
  icon16_things,
  icon16_sounds,
  icon16_count,
} ed_icon16_t;

typedef enum {
  icon8_minus,
  icon8_collapse,
  icon8_maximize,
  icon8_dropdown,
  icon8_points,
  icon8_lines,
  icon8_sectors,
  icon8_things,
  icon8_sounds,
  icon8_checkbox,
  icon8_count,
} ed_icon_t;

enum {
  edit_vertices,
  edit_lines,
  edit_sectors,
  edit_things,
  edit_sounds,
  edit_modes
};

typedef struct {
  uint16_t point, line, sector, thing;
} editor_selection_t;

typedef struct {
  window_t *window;
  window_t *inspector;
  window_t *game;
  int16_t cursor[2];
  vec2 camera;
  int grid_size;         // Grid size (8 units by default)
  bool drawing;          // Currently drawing a sector?
  bool dragging;          // Currently dragging a vertex?
  int move_camera;
  mapvertex_t draw_points[MAX_DRAW_POINTS]; // Points for current sector being drawn
  int num_draw_points;   // Number of points in current sector
  editor_selection_t hover, selected;
  int sel_mode;
  float scale;
  uint32_t vao, vbo;
  int icons8[icon8_count];
  int icons16[icon16_count];
} editor_state_t;

void init_editor(editor_state_t *editor);
void set_editor_camera(editor_state_t *editor, int16_t x, int16_t y);
void toggle_editor_mode(editor_state_t *editor);
void draw_editor(map_data_t const *map, editor_state_t const *editor, player_t const *player);
void handle_editor_input(map_data_t *map, editor_state_t *editor, player_t *player, float delta_time);
void finish_sector(map_data_t *map, editor_state_t *editor);
bool point_exists(mapvertex_t point, map_data_t *map, int *index);
int split_linedef(map_data_t *map, int linedef_id, float x, float y);

/**
 * Calculate the closest point on a line segment to a given point
 *
 * @param point_x X coordinate of point
 * @param point_y Y coordinate of point
 * @param line_x1 X coordinate of line start
 * @param line_y1 Y coordinate of line start
 * @param line_x2 X coordinate of line end
 * @param line_y2 Y coordinate of line end
 * @param closest_x Pointer to store closest point X coordinate
 * @param closest_y Pointer to store closest point Y coordinate
 * @param t_param Pointer to store parametric value (0-1) along line
 * @return Distance squared from point to line
 */
float closest_point_on_line(float point_x, float point_y,
                            float line_x1, float line_y1,
                            float line_x2, float line_y2,
                            float *closest_x, float *closest_y,
                            float *t_param);


uint16_t add_vertex(map_data_t *map, mapvertex_t vertex);
uint16_t add_linedef(map_data_t *map, uint16_t start, uint16_t end,
                     uint16_t front_side, uint16_t back_side);
uint16_t add_sidedef(map_data_t *map, uint16_t sector_index);
uint16_t add_sector(map_data_t *map);

mapvertex_t vertex_midpoint(mapvertex_t v1, mapvertex_t v2);
uint16_t find_point_sector(map_data_t const *map, mapvertex_t vertex);

int check_closed_loop(map_data_t *map, uint16_t line, uint16_t vertices[]);
int find_existing_linedef(map_data_t const *map, uint16_t v1, uint16_t v2);
bool set_loop_sector(map_data_t *map, uint16_t sector, uint16_t *vertices, uint16_t num_vertices);


mapvertex_t compute_centroid(map_data_t const *map, uint16_t vertices[], int count);


#endif
