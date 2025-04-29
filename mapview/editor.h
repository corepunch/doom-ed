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

typedef struct {
  bool active;           // Is editor mode active?
  int grid_size;         // Grid size (8 units by default)
  bool drawing;          // Currently drawing a sector?
  int draw_points[MAX_DRAW_POINTS][2]; // Points for current sector being drawn
  int num_draw_points;   // Number of points in current sector
  uint32_t vao, vbo;
} editor_state_t;

void init_editor(editor_state_t *editor);
void toggle_editor_mode(editor_state_t *editor);
void draw_editor(map_data_t const *map, editor_state_t const *editor, player_t const *player);
void handle_editor_input(map_data_t *map, editor_state_t *editor, player_t *player);
void finish_sector(map_data_t *map, editor_state_t *editor);
bool point_exists(int x, int y, map_data_t *map);
void snap_mouse_position(editor_state_t const *editor, player_t const *player, int *snapped_x, int *snapped_y);
void get_mouse_position(editor_state_t const *editor, player_t const *player, float *world_x, float *world_y);
void split_linedef(map_data_t *map, int linedef_id, float x, float y);

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

#endif
