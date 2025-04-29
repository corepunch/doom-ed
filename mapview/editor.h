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

#endif
