#include <SDL2/SDL.h>
#include <cglm/struct.h>

#include "editor.h"

extern SDL_Window* window;

// Utility function to snap coordinates to grid
//static void snap_to_grid(int *x, int *y, int grid_size) {
//  *x = (*x / grid_size) * grid_size;
//  *y = (*y / grid_size) * grid_size;
//}

// Toggle editor mode on/off
void toggle_editor_mode(editor_state_t *editor) {
  editor->active = !editor->active;
  
  // Reset drawing state when exiting editor
  if (!editor->active) {
    editor->drawing = false;
    editor->num_draw_points = 0;
  }
  
  // Set mouse mode based on editor state
  SDL_SetRelativeMouseMode(editor->active ? SDL_FALSE : SDL_TRUE);
}

// Handle mouse and keyboard input for the editor
void handle_editor_input(map_data_t *map, editor_state_t *editor, player_t *player) {
  SDL_Event event;
  
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      extern bool running;
      running = false;
    }
    else if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_TAB:
          toggle_editor_mode(editor);
          break;
        case SDL_SCANCODE_ESCAPE:
          if (editor->drawing) {
            // Cancel current drawing
            editor->drawing = false;
            editor->num_draw_points = 0;
          } else if (editor->active) {
            // Exit editor mode
            toggle_editor_mode(editor);
          }
          break;
        case SDL_SCANCODE_G:
          // Toggle grid size (8, 16, 32, 64, 128)
          editor->grid_size *= 2;
          if (editor->grid_size > 128) editor->grid_size = 8;
          break;
        default:
          break;
      }
    }
    else if (event.type == SDL_MOUSEWHEEL) {
//      if (event.wheel.y) {
        editor->scale -= event.wheel.y/30.f;
//      }
    }
    else if (event.type == SDL_MOUSEBUTTONUP && editor->dragging) {
      extern mapvertex_t sn;
      editor->dragging = false;
      map->vertices[editor->current_point] = sn;
      // Rebuild vertex buffers
      build_wall_vertex_buffer(map);
      build_floor_vertex_buffer(map);
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN && editor->active) {
      extern mapvertex_t sn;
      extern int splitting_line;
      int point = -1;
      int old_point = editor->current_point;
      if (event.button.button == SDL_BUTTON_LEFT) {
        if (point_exists(sn, map, &point)) {
          editor->current_point = point;
        } else if (splitting_line>=0) {
          editor->current_point = split_linedef(map, splitting_line, sn.x, sn.y);
        } else {
          editor->current_point = add_vertex(map, sn);
        }
        if (editor->drawing) {
          mapvertex_t a = map->vertices[old_point];
          mapvertex_t b = map->vertices[editor->current_point];
          uint16_t sec = find_point_sector(map, vertex_midpoint(a, b));
          uint16_t line = -1;
          if (sec != 0xFFFF) {
            line = add_linedef(map, old_point, editor->current_point, add_sidedef(map, sec), add_sidedef(map, sec));
          } else {
            line = add_linedef(map, old_point, editor->current_point, 0xFFFF, 0xFFFF);
          }
          uint16_t vertices[256];
          int num_vertices = check_closed_loop(map, line, vertices);
          if (num_vertices) {
            uint16_t sector = add_sector(map);
            set_loop_sector(map, sector, vertices, num_vertices);
            editor->drawing = false;
          }
        } else {
          editor->drawing = true;
        }
      } else if (event.button.button == SDL_BUTTON_RIGHT) {
        extern int splitting_line;
        if (editor->drawing) {
          // Cancel current drawing
          editor->drawing = false;
          editor->num_draw_points = 0;
        } else if (editor->dragging) {
          editor->dragging = false;
          editor->current_point = -1;
        } else if (splitting_line>=0) {
          split_linedef(map, splitting_line, sn.x, sn.y);
        } else if (point_exists(sn, map, &point)) {
          editor->dragging = true;
          editor->current_point = point;
        }
      }
    }
    else if (event.type == SDL_MOUSEWHEEL && editor->active) {
      // Zoom in/out by adjusting view size
      // (This would require implementing zoom in the view matrix)
    }
  }
  
  // If in editor mode, handle keyboard movement
  if (editor->active) {
    const Uint8* keystates = SDL_GetKeyboardState(NULL);
    float move_speed = 32.0f;
    
    if (keystates[SDL_SCANCODE_W] || keystates[SDL_SCANCODE_UP]) {
      player->y += move_speed;
    }
    if (keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN]) {
      player->y -= move_speed;
    }
    if (keystates[SDL_SCANCODE_A] || keystates[SDL_SCANCODE_LEFT]) {
      player->x -= move_speed;
    }
    if (keystates[SDL_SCANCODE_D] || keystates[SDL_SCANCODE_RIGHT]) {
      player->x += move_speed;
    }
  }
}


void
get_mouse_position(editor_state_t const *editor,
                   player_t const *player,
                   mat4 view_proj_matrix,
                   vec2 world)
{
  float z_plane = 0;
  
  int mouse_x, mouse_y;
  // Get mouse position
  SDL_GetMouseState(&mouse_x, &mouse_y);
  
  // Convert to world coordinates
  int win_width, win_height;
  SDL_GetWindowSize(window, &win_width, &win_height);
  
  // Convert to normalized device coordinates (-1 to 1)
  float ndc_x = (2.0f * mouse_x) / win_width - 1.0f;
  float ndc_y = 1.0f - (2.0f * mouse_y) / win_height;
  
  vec4 clip_near = { ndc_x, ndc_y, -1.0f, 1.0f };
  vec4 clip_far  = { ndc_x, ndc_y,  1.0f, 1.0f };
  
  // Inverse view-projection matrix (you need to pass it in or compute it here)
  mat4 inv_view_proj;
  glm_mat4_inv(view_proj_matrix, inv_view_proj);
  
  vec4 ray_origin, ray_direction;
  
  // Unproject to world space
  vec4 world_near, world_far;
  glm_mat4_mulv(inv_view_proj, clip_near, world_near);
  glm_mat4_mulv(inv_view_proj, clip_far,  world_far);
  
  // Perspective divide
  glm_vec4_scale(world_near, 1.0f / world_near[3], world_near);
  glm_vec4_scale(world_far,  1.0f / world_far[3],  world_far);
  
  // Ray origin and direction
  glm_vec3(world_near, ray_origin);
  glm_vec3_sub(world_far, world_near, ray_direction);
  glm_vec3_normalize(ray_direction);
  
  // Intersection with z = z_plane
  float t = (z_plane - ray_origin[2]) / ray_direction[2];
  
  glm_vec3_scale(ray_direction, t, world);
  glm_vec3_add(ray_origin, world, world);
}

void
snap_mouse_position(editor_state_t const *editor,
                    vec2 const world,
                    mapvertex_t *snapped)
{
  float world_x, world_y;
  
  world_x = world[0] + editor->grid_size/2;
  world_y = world[1] - editor->grid_size/2;
  
  // Snap to grid
  snapped->x = floorf(world_x / editor->grid_size) * editor->grid_size;
  snapped->y = ceilf(world_y / editor->grid_size) * editor->grid_size;
}
