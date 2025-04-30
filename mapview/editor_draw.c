#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "editor.h"

// Globals
extern GLuint ui_prog;

// Initialize editor state
void init_editor(editor_state_t *editor) {
  editor->active = false;
  editor->grid_size = 32;
  editor->drawing = false;
  editor->num_draw_points = 0;
  
  // Create VAO and VBO for editor rendering
  glGenVertexArrays(1, &editor->vao);
  glGenBuffers(1, &editor->vbo);
  
  // Set up VAO
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  
  // Enable vertex attributes
  glEnableVertexAttribArray(0); // Position
  glEnableVertexAttribArray(1); // Texture coords
  
  // Reset bindings
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Draw the grid
static void draw_grid(int grid_size, player_t const *player, int view_size) {
  int start_x = ((int)player->x / grid_size - view_size) * grid_size;
  int end_x = ((int)player->x / grid_size + view_size) * grid_size;
  int start_y = ((int)player->y / grid_size - view_size) * grid_size;
  int end_y = ((int)player->y / grid_size + view_size) * grid_size;
  
  // Set grid color (dark gray)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 0.3f, 0.3f, 0.3f, 1.0f);
  
  // Draw vertical grid lines
  for (int x = start_x; x <= end_x; x += grid_size) {
    // Create two vertices for each grid line
    wall_vertex_t verts[2] = {
      { x, start_y, 0, 0, 0, 0, 0, 0 },
      { x, end_y, 0, 0, 0, 0, 0, 0 }
    };
    
    // Draw the line
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
  
  // Draw horizontal grid lines
  for (int y = start_y; y <= end_y; y += grid_size) {
    // Create two vertices for each grid line
    wall_vertex_t verts[2] = {
      { start_x, y, 0, 0, 0, 0, 0, 0 },
      { end_x, y, 0, 0, 0, 0, 0, 0 }
    };
    
    // Draw the line
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
}

// Draw the current sector being created
static void draw_current_sector(editor_state_t const *editor) {
  if (editor->num_draw_points < 2) return;
  
  // Set line color (yellow)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 0.0f, 1.0f);
  
  // Draw lines connecting all points
  for (int i = 0; i < editor->num_draw_points - 1; i++) {
    wall_vertex_t verts[2] = {
      { editor->draw_points[i].x, editor->draw_points[i].y, 0, 0, 0, 0, 0, 0 },
      { editor->draw_points[i+1].x, editor->draw_points[i+1].y, 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
  
  // Highlight vertices
  glPointSize(5.0f);
  for (int i = 0; i < editor->num_draw_points; i++) {
    wall_vertex_t vert = {
      editor->draw_points[i].x,
      editor->draw_points[i].y,
      0, 0, 0, 0, 0, 0
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.u);
    glDrawArrays(GL_POINTS, 0, 1);
  }
  glPointSize(1.0f);
}

// Draw walls with different colors based on whether they're one-sided or two-sided
static void draw_walls_editor(map_data_t const *map) {
  // Draw each linedef
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    
    // Determine if this is a one-sided or two-sided wall
    bool two_sided = (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs);
    extern int splitting_line;
    // Set color - white for one-sided (outer) walls, red for two-sided (inner) walls
    if (splitting_line == i) {
      glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 0.0f, 1.0f);
    } else if (!two_sided) {
      glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    // Create and draw vertices
    wall_vertex_t verts[2] = {
      { v1->x, v1->y, 0, 0, 0, 0, 0, 0 },
      { v2->x, v2->y, 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
    
    // Draw vertices as points
    glPointSize(3.0f);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glDrawArrays(GL_POINTS, 0, 1);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[1].x);
    glDrawArrays(GL_POINTS, 0, 1);
    glPointSize(1.0f);
  }
}

// Draw cursor position
static void draw_cursor(int x, int y) {
  // Set cursor color (green)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 0.0f, 1.0f, 0.0f, 1.0f);
  
  // Draw crosshair
  int size = 16;
  wall_vertex_t verts[4] = {
    { x - size, y, 0, 0, 0, 0, 0, 0 },
    { x + size, y, 0, 0, 0, 0, 0, 0 },
    { x, y - size, 0, 0, 0, 0, 0, 0 },
    { x, y + size, 0, 0, 0, 0, 0, 0 }
  };
  
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glDrawArrays(GL_LINES, 0, 2);
  
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[2].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[2].u);
  glDrawArrays(GL_LINES, 0, 2);
}

int splitting_line = 0;
float sn_x = 0, sn_y = 0;

// Draw the editor UI
void draw_editor(map_data_t const *map, editor_state_t const *editor, player_t const *player) {
  if (!editor->active) return;
  
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  glDisable(GL_DEPTH_TEST);
  
  // Set up orthographic projection for 2D view
  mat4 mvp;
  float scale = 1;
  float w = SCREEN_WIDTH * scale;
  float h = SCREEN_HEIGHT * scale;
  
  // Create projection and view matrices
  mat4 proj, view;
  glm_ortho(-w, w, -h, h, -1000, 1000, proj);
  
  // Center view on player
  glm_translate_make(view, (vec3){ -player->x, -player->y, 0.0f });
  
  // Combine matrices
  glm_mat4_mul(proj, view, mvp);
  
  // Set up rendering
  glUseProgram(ui_prog);
  glUniformMatrix4fv(glGetUniformLocation(ui_prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  glBindTexture(GL_TEXTURE_2D, 1); // Use default 1x1 white texture
  
  // Clear the screen to black
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//  void draw_minimap(map_data_t const *map, player_t const *player);
//  draw_minimap(map, player);
//  glBindVertexArray(map->walls.vao);
//  glDrawArrays(GL_LINES, 0, map->walls.num_vertices);
  
  // Draw grid
  draw_grid(editor->grid_size, player, 100);
  
  // Draw existing walls
  draw_walls_editor(map);
  
  // Draw sector being created
  draw_current_sector(editor);

  float world_x, world_y;
  get_mouse_position(editor, player, &world_x, &world_y);
  
  sn_x = world_x;
  sn_y = world_y;
  float dist = 100000;
  for (int i = 0; i < map->num_linedefs; i++) {
    float x, y, z;
    float d = closest_point_on_line(world_x, world_y,
                                    map->vertices[map->linedefs[i].start].x,
                                    map->vertices[map->linedefs[i].start].y,
                                    map->vertices[map->linedefs[i].end].x,
                                    map->vertices[map->linedefs[i].end].y,
                                    &x, &y, &z);
    if (dist > d)
    {
      sn_x = x;
      sn_y = y;
      dist = d;
      splitting_line = i;
    }
  }
  for (int i = 0; i < map->num_vertices; i++) {
    float dx = world_x - map->vertices[i].x;
    float dy = world_y - map->vertices[i].y;
    float d = dx*dx + dy*dy;
    if (editor->grid_size * 8.0f > d)
    {
      sn_x = map->vertices[i].x;
      sn_y = map->vertices[i].y;
      dist = d;
      splitting_line = -1;
    }
  }
  int snapped_x = sn_x, snapped_y = sn_y;

  if (dist < editor->grid_size * 8.0f) {
    draw_cursor(sn_x, sn_y);
  } else {
    splitting_line = -1;
    // Snap to grid
    snap_mouse_position(editor, player, &snapped_x, &snapped_y);
    sn_x = snapped_x;
    sn_y = snapped_y;
    // Draw cursor at the snapped position
    draw_cursor(snapped_x, snapped_y);
  }
  
  // If currently drawing, show line from last point to cursor
  if (editor->dragging || (editor->drawing && editor->num_draw_points > 0)) {
    glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 0.0f, 0.5f);
    float x = editor->dragging ? map->vertices[editor->num_drag_point].x : editor->draw_points[editor->num_draw_points-1].x;
    float y = editor->dragging ? map->vertices[editor->num_drag_point].y : editor->draw_points[editor->num_draw_points-1].y;
    wall_vertex_t verts[2] = {
      { x, y, 0, 0, 0, 0, 0, 0 },
      { snapped_x, snapped_y, 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
}
