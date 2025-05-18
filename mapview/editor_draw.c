#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "editor.h"
#include "sprites.h"

// Globals
extern GLuint ui_prog;
extern GLuint white_tex;
extern unsigned frame;

// Initialize editor state
void init_editor(editor_state_t *editor) {
  editor->grid_size = 32;
  editor->drawing = false;
  editor->num_draw_points = 0;
  editor->scale = 1;
  
  // Create VAO and VBO for editor rendering
  glGenVertexArrays(1, &editor->vao);
  glGenBuffers(1, &editor->vbo);
  
  // Set up VAO
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  
  // Enable vertex attributes
  glEnableVertexAttribArray(0); // Position
  glEnableVertexAttribArray(1); // Texture coords
//  glEnableVertexAttribArray(3); // Color

  // Reset bindings
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  
  GLuint make_1bit_tex(void *data, int width, int height);
  extern uint8_t ed_icons[icon_count][8];
  for (int i = 0; i < icon_count; i++) {
    editor->icons[i] = make_1bit_tex(ed_icons[i], 8, 8);
  }

}

void set_editor_camera(editor_state_t *editor, int16_t x, int16_t y) {
  editor->camera[0] = x;
  editor->camera[1] = y;
}

// Draw the grid
static void draw_grid(int grid_size, player_t const *player, int view_size) {
  int start_x = ((int)player->x / grid_size - view_size) * grid_size;
  int end_x = ((int)player->x / grid_size + view_size) * grid_size;
  int start_y = ((int)player->y / grid_size - view_size) * grid_size;
  int end_y = ((int)player->y / grid_size + view_size) * grid_size;
  
  // Set grid color (dark gray)
  glUniform4f(ui_prog_color, .15f, .15f, 0.15f, 1.0f);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  
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
  
  // Draw dots at grid intersections
//  for (int y = start_y; y <= end_y; y += grid_size) {
//    for (int x = start_x; x <= end_x; x += grid_size) {
//      wall_vertex_t vert = { x, y, 0, 0, 0, 0, 0, 0 };
//      
//      // Set up vertex attributes
//      glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.x);
//      glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.u);
//
//      // Draw a single point
//      glPointSize(!((x/grid_size) % 4)&&!((y/grid_size) % 4) ? 2.0f : 1.f);
//      glDrawArrays(GL_POINTS, 0, 1);
//    }
//  }
  glDisable(GL_BLEND);
}

// Draw the current sector being created
static void draw_current_sector(editor_state_t const *editor) {
  if (editor->num_draw_points < 2) return;
  
  // Set line color (yellow)
  glUniform4f(ui_prog_color, 1.0f, 1.0f, 0.0f, 1.0f);
  
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

extern GLuint no_tex;
//
//static void draw_floors_ids_editor(map_data_t const *map) {
//  glBindTexture(GL_TEXTURE_2D, white_tex);
//  glBindTexture(GL_TEXTURE_2D, white_tex);
//  glUniform1i(ui_prog_tex0, 0);
//  glUniform2f(ui_prog_tex0_size, 1, 1);
//  for (int i = 0; i < map->num_sectors; i++) {
//    int id = i;// | PIXEL_FLOOR;
//    uint8_t *c = (uint8_t *)&id;
//    glUniform4f(ui_prog_color, c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
//    glDrawArrays(GL_TRIANGLES,
//                 map->floors.sectors[i].floor.vertex_start,
//                 map->floors.sectors[i].floor.vertex_count);
//  }
//}

static void draw_floors_editor(map_data_t const *map) {
  GLuint tex0_size = glGetUniformLocation(ui_prog, "tex0_size");
  
  glUniform4f(ui_prog_color, 1.0f, 1.0f, 1.0f, 1.0f);
  glBindVertexArray(map->floors.vao);
  
  for (int i = 0; i < map->num_sectors; i++) {
    if (map->floors.sectors[i].floor.texture) {
      mapside_texture_t const *tex = map->floors.sectors[i].floor.texture;
      glBindTexture(GL_TEXTURE_2D, tex->texture);
      glUniform2f(tex0_size, tex->width, tex->height);
    } else {
      glBindTexture(GL_TEXTURE_2D, no_tex);
    }
    glDrawArrays(GL_TRIANGLES,
                 map->floors.sectors[i].floor.vertex_start,
                 map->floors.sectors[i].floor.vertex_count);
  }
}

// Draw walls with different colors based on whether they're one-sided or two-sided
static void draw_walls_editor(map_data_t const *map) {
#if 1
  glBindTexture(GL_TEXTURE_2D, white_tex); // Use default 1x1 white texture
  glUniform4f(ui_prog_color, 1.0f, 1.0f, 1.0f, 1.0f);
  glBindVertexArray(map->walls.vao);
  glDrawArrays(GL_LINES, 0, map->walls.num_vertices);
  
  glPointSize(3.0f);
  glDrawArrays(GL_POINTS, 0, map->walls.num_vertices);
  glPointSize(1.0f);
#else
  // Draw each linedef
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    
    // Determine if this is a one-sided or two-sided wall
    bool two_sided = linedef->sidenum[1] != 0xFFFF;
    extern int splitting_line;
    // Set color - white for one-sided (outer) walls, red for two-sided (inner) walls
    if (splitting_line == i) {
      glUniform4f(ui_prog_color, 1.0f, 1.0f, 0.0f, 1.0f);
    } else if (!two_sided) {
      glUniform4f(ui_prog_color, 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      glUniform4f(ui_prog_color, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    int16_t z = 0;
    
    if (linedef->sidenum[0] != 0xFFFF) {
      z = map->sectors[map->sidedefs[linedef->sidenum[0]].sector].floorheight;
    }
    
    // Create and draw vertices
    wall_vertex_t verts[2] = {
      { v1->x, v1->y, z, 0, 0, 0, 0, 0 },
      { v2->x, v2->y, z, 0, 0, 0, 0, 0 }
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
#endif
}

// Draw cursor position
static void draw_cursor(int x, int y) {
  // Set cursor color (green)
  glUniform4f(ui_prog_color, 0.0f, 1.0f, 0.0f, 1.0f);

//  wall_vertex_t verts = { x, y, 0, 0, 0, 0, 0, 0 };
//  glPointSize(3.0f);
//  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts.x);
//  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts.u);
//  glDrawArrays(GL_POINTS, 0, 1);

  // Draw crosshair
  int size = 16;
  wall_vertex_t verts[4] = {
    { x - size, y, 0, 0, 0, 0, 0, 0 },
    { x + size, y, 0, 0, 0, 0, 0, 0 },
    { x, y - size, 0, 0, 0, 0, 0, 0 },
    { x, y + size, 0, 0, 0, 0, 0, 0 }
  };
  
  glBindTexture(GL_TEXTURE_2D, white_tex);
  
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glDrawArrays(GL_LINES, 0, 4);
}

int splitting_line = 0;
mapvertex_t sn;

void get_mouse_position(editor_state_t const *, int16_t const *screen, mat4 const, vec3);
void snap_mouse_position(editor_state_t const *, vec2 const, mapvertex_t *);
void get_editor_mvp(editor_state_t const *editor, mat4 mvp) {
  // Set up orthographic projection for 2D view
  float w = editor->window->frame.w * editor->scale;
  float h = editor->window->frame.h * editor->scale;
  
  // Create projection and view matrices
  mat4 proj, view;
  glm_ortho(-w, w, -h, h, -1000, 1000, proj);
  
  // Center view on player
  glm_translate_make(view, (vec3) {
    -editor->camera[0],
    -editor->camera[1],
    0.0f
  });
  
  // Combine matrices
  glm_mat4_mul(proj, view, mvp);
}

void draw_line_ex(editor_state_t const *editor, map_data_t const *map, int x1, int y1, int x2, int y2) {
  wall_vertex_t verts[2] = { {x1,y1}, {x2,y2} };
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(wall_vertex_t), &verts[0].color);
  glDrawArrays(GL_LINES, 0, 2);
}

void draw_line(editor_state_t const *editor, map_data_t const *map, int i) {
  draw_line_ex(editor, map,
    map->vertices[map->linedefs[i].start].x,
    map->vertices[map->linedefs[i].start].y,
    map->vertices[map->linedefs[i].end].x,
    map->vertices[map->linedefs[i].end].y);
}

// Draw the editor UI
void
draw_editor(map_data_t const *map,
            editor_state_t const *editor,
            player_t const *player)
{
  mat4 mvp;
  get_editor_mvp(editor, mvp);

//  void get_view_matrix(map_data_t const *map, player_t const *player, mat4 out);
//  get_view_matrix(map, player, mvp);
  
  // Clear the screen to black
//  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
//  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//  void draw_minimap(map_data_t const *map, player_t const *player);
//  draw_minimap(map, player);
//  glBindVertexArray(map->walls.vao);
//  glDrawArrays(GL_LINES, 0, map->walls.num_vertices);
  
  
  extern GLuint world_prog;
  glUseProgram(world_prog);
  glUniformMatrix4fv(world_prog_mvp, 1, GL_FALSE, (const float*)mvp);
  glUniform3f(world_prog_viewPos, 0,0,-10001);//player->x, player->y, player->z);

  void draw_things(map_data_t const *map, viewdef_t const *viewdef, bool rotate);

  viewdef_t viewdef={.frame=frame++};
  memcpy(viewdef.mvp, mvp, sizeof(mat4));
  memcpy(viewdef.viewpos, &player->x, sizeof(vec3));
  viewdef.viewpos[2] = -100000;
  glm_frustum_planes(mvp, viewdef.frustum);
  
  // Set up rendering
  glUseProgram(ui_prog);
  glUniformMatrix4fv(ui_prog_mvp, 1, GL_FALSE, (const float*)mvp);
  
  glDisable(GL_DEPTH_TEST);
  
  // Draw grid
//  draw_grid(editor->grid_size, player, 64);

  draw_floors_editor(map);

  // Draw existing walls
  draw_walls_editor(map);

  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);

  // Draw sector being created
//  draw_current_sector(editor);

  draw_things(map, &viewdef, false);
  
  glUseProgram(ui_prog);
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  glDisable(GL_DEPTH_TEST);
  
  extern window_t *active;

  if (editor->window != active) {
    goto draw_player;
  }
  
  vec3 world;
  get_mouse_position(editor, editor->cursor, mvp, world);
  
  sn.x = world[0];
  sn.y = world[1];
  float dist = 100000;
  for (int i = 0; i < map->num_linedefs; i++) {
    float x, y, z;
    float d = closest_point_on_line(world[0], world[1],
                                    map->vertices[map->linedefs[i].start].x,
                                    map->vertices[map->linedefs[i].start].y,
                                    map->vertices[map->linedefs[i].end].x,
                                    map->vertices[map->linedefs[i].end].y,
                                    &x, &y, &z);
    if (dist > d) {
      sn.x = x;
      sn.y = y;
      dist = d;
      splitting_line = i;
    }
  }
  
//  static int s = -1;
//  if (s!=splitting_line) {
//    printf("%d\n", splitting_line);
//    s =splitting_line;
//  }

  if (editor->sel_mode == edit_vertices) {
    for (int i = 0; i < map->num_vertices; i++) {
      float dx = world[0] - map->vertices[i].x;
      float dy = world[1] - map->vertices[i].y;
      float d = dx*dx + dy*dy;
      if (editor->grid_size * 8.0f > d) {
        sn.x = map->vertices[i].x;
        sn.y = map->vertices[i].y;
        dist = d;
        splitting_line = -1;
      }
    }
  }

  if (dist < editor->grid_size * 8.0f) {
    if (editor->sel_mode == edit_vertices) {
      draw_cursor(sn.x, sn.y);
    }
  } else {
    splitting_line = -1;
    if (editor->sel_mode == edit_vertices) {
      // Snap to grid
      snap_mouse_position(editor, world, &sn);
      // Draw cursor at the snapped position
      draw_cursor(sn.x, sn.y);
    }
  }

  glUniform4f(ui_prog_color, 1.0f, 1.0f, 0.0f, 0.5f);
  glBindTexture(GL_TEXTURE_2D, white_tex);
  
  switch (editor->sel_mode) {
    case edit_vertices:
    case edit_lines:
      if (splitting_line != -1) {
        draw_line(editor, map, splitting_line);
      }
      
      // If currently drawing, show line from last point to cursor
      if (editor->dragging || editor->drawing) {
        float x = map->vertices[editor->current_point].x;
        float y = map->vertices[editor->current_point].y;
        draw_line_ex(editor, map, x, y, sn.x, sn.y);
      }
      break;
    case edit_sectors:
      if (editor->current_sector != -1) {
        for (int i = 0; i < map->num_linedefs; i++) {
          maplinedef_t const *ld = &map->linedefs[i];
          if ((ld->sidenum[0] != 0xFFFF && map->sidedefs[ld->sidenum[0]].sector == editor->current_sector) ||
              (ld->sidenum[1] != 0xFFFF && map->sidedefs[ld->sidenum[1]].sector == editor->current_sector))
          {
            draw_line(editor, map, i);
          }
        }
      }
      break;
  }
  
draw_player:
  
  glUniform4f(ui_prog_color, 1.0f, 1.0f, 0.0f, 0.5f);
  
  float angle_rad = glm_rad(player->angle);
  float input_x = -50 * cos(angle_rad);
  float input_y =  50 * sin(angle_rad);

  wall_vertex_t verts[2] = { { player->x, player->y }, { player->x+input_x, player->y+input_y } };

//  wall_vertex_t verts[3] = {
//    { player->x + player->points[1][0], player->y + player->points[1][1] },
//    { player->x, player->y },
//    { player->x + player->points[0][0], player->y + player->points[0][1] },
//  };

  glBindTexture(GL_TEXTURE_2D, white_tex);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(wall_vertex_t), &verts[0].color);
  glDrawArrays(GL_LINE_STRIP, 0, 2);
  glPointSize(4);
  glDrawArrays(GL_POINTS, 0, 1);
}
