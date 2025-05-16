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
  if (game.state == GS_DUNGEON) {
    game.state = GS_EDITOR;
  } else if (game.state == GS_EDITOR) {
    game.state = GS_DUNGEON;
    editor->drawing = false;
    editor->num_draw_points = 0;
  }
  
  // Set mouse mode based on editor state
  SDL_SetRelativeMouseMode(game.state == GS_EDITOR ? SDL_FALSE : SDL_TRUE);
}

// Handle mouse and keyboard input for the editor
void handle_editor_input(map_data_t *map,
                         editor_state_t *editor,
                         player_t *player,
                         float delta_time) {
  SDL_Event event;
  
  static float forward_move = 0, strafe_move = 0;
  
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      extern bool running;
      running = false;
    } else if (event.type == SDL_JOYBUTTONDOWN) {
      if (event.jbutton.button == 8) {
        toggle_editor_mode(editor);
      }
    }
    else if (event.type == SDL_JOYAXISMOTION) {
      //      printf("Axis %d = %d\n", event.jaxis.axis, event.jaxis.value);
      switch (event.jaxis.axis) {
        case 0: strafe_move = event.jaxis.value/(float)0x8000; break;
        case 1: forward_move = -event.jaxis.value/(float)0x8000; break;
//        case 3: mouse_x_rel = event.jaxis.value/1200.f; break;
//        case 4: mouse_y_rel = event.jaxis.value/1200.f; break;
      }
    }
    else if (event.type == SDL_KEYUP) {
      switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
          forward_move = 0;
          break;
          // Calculate strafe direction vector (perpendicular to forward)
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_LEFT:
          strafe_move = 0;
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
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
    }
    else if (event.type == SDL_MOUSEWHEEL) {
      // Zoom in/out by adjusting view size
      // (This would require implementing zoom in the view matrix)
    }
  }
  
  // If in editor mode, handle keyboard movement
//  const Uint8* keystates = SDL_GetKeyboardState(NULL);
  float move_speed = 1000 * delta_time;

  player->y += forward_move * move_speed;
  player->x += strafe_move * move_speed;

  
//  if (keystates[SDL_SCANCODE_W] || keystates[SDL_SCANCODE_UP]) {
//    player->y += move_speed;
//  }
//  if (keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN]) {
//    player->y -= move_speed;
//  }
//  if (keystates[SDL_SCANCODE_A] || keystates[SDL_SCANCODE_LEFT]) {
//    player->x -= move_speed;
//  }
//  if (keystates[SDL_SCANCODE_D] || keystates[SDL_SCANCODE_RIGHT]) {
//    player->x += move_speed;
//  }
}


void
get_mouse_position(editor_state_t const *editor,
                   int16_t const screen[2],
                   mat4 view_proj_matrix,
                   vec3 world)
{
  float z_plane = 0;
  
  // Convert to world coordinates
  int win_width = editor->window->w, win_height = editor->window->h;
  
  // Convert to normalized device coordinates (-1 to 1)
  float ndc_x = (2.0f * screen[0]) / win_width - 1.0f;
  float ndc_y = 1.0f - (2.0f * screen[1]) / win_height;
  
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
void get_editor_mvp(editor_state_t const *, mat4);
bool win_editor(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mapvertex_t sn;
  extern int splitting_line;
  editor_state_t *editor = win->userdata;
  map_data_t *map = &game.map;
  int point = -1;
  int old_point = editor ? editor->current_point : -1;
  switch (msg) {
    case MSG_CREATE:
      win->userdata = lparam;
      ((editor_state_t *)lparam)->window = win;
      return true;
    case MSG_PAINT:
      draw_editor(&game.map, win->userdata, &game.player);
      return true;
    case MSG_MOUSEMOVE:
      if (editor->move_camera == 2) {
        int16_t move_cursor[2] = { LOWORD(wparam), HIWORD(wparam) };
        vec3 world_1, world_2;
        mat4 mvp;
        get_editor_mvp(editor, mvp);
        get_mouse_position(editor, editor->cursor, mvp, world_1);
        get_mouse_position(editor, move_cursor, mvp, world_2);
        editor->camera[0] += world_1[0] - world_2[0];
        editor->camera[1] += world_1[1] - world_2[1];
        editor->cursor[0] = LOWORD(wparam);
        editor->cursor[1] = HIWORD(wparam);
      } else {
        editor->cursor[0] = LOWORD(wparam);
        editor->cursor[1] = HIWORD(wparam);
      }
      invalidate_window(win);
      return true;
//    case MSG_WHEEL:
//      editor->scale *= 1.f - (int16_t)HIWORD(wparam)/50.f;
//      invalidate_window(win);
//      return true;
    case MSG_WHEEL: {
      mat4 mvp;
      vec3 world_before, world_after, delta;
      get_editor_mvp(editor, mvp);
      get_mouse_position(editor, editor->cursor, mvp, world_before);
      editor->scale *= MAX(1.f - (int16_t)HIWORD(wparam) / 50.f, 0.1f);
      get_editor_mvp(editor, mvp);
      get_mouse_position(editor, editor->cursor, mvp, world_after);
      glm_vec2_sub(world_before, world_after, delta);
      glm_vec2_add(editor->camera, delta, editor->camera);
      invalidate_window(win);
      return true;
    }
    case MSG_LBUTTONUP:
      if (editor->move_camera == 2) {
        editor->move_camera = 1;
      } else if (editor->dragging) {
        extern mapvertex_t sn;
        editor->dragging = false;
        map->vertices[editor->current_point] = sn;
        // Rebuild vertex buffers
        build_wall_vertex_buffer(map);
        build_floor_vertex_buffer(map);
        return true;
      }
      return true;
    case MSG_LBUTTONDOWN:
      if (editor->drawing) {
        // Cancel current drawing
        editor->drawing = false;
        editor->num_draw_points = 0;
      } else if (editor->move_camera > 0) {
        editor->move_camera = 2;
        return true;
      } else if (splitting_line>=0) {
        editor->current_point = split_linedef(map, splitting_line, sn.x, sn.y);
      } else if (point_exists(sn, map, &point)) {
        editor->current_point = point;
        editor->dragging = true;
//      } else {
//        editor->current_point = add_vertex(map, sn);
      }
      return true;
    case MSG_RBUTTONDOWN:
      if (editor->dragging) {
        editor->dragging = false;
        editor->current_point = -1;
      } else if (point_exists(sn, map, &point)) {
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
      return true;
    case MSG_RBUTTONUP:
      return true;

    case MSG_KEYDOWN:
      switch (wparam) {
//        case SDL_SCANCODE_TAB:
//          toggle_editor_mode(editor);
//          break;
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
          // forward_move = 1;
          editor->camera[1] += ED_SCROLL;
          invalidate_window(win);
          return true;
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
          // forward_move = -1;
          editor->camera[1] -= ED_SCROLL;
          invalidate_window(win);
          return true;
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_RIGHT:
          // strafe_move = 1;
          editor->camera[0] += ED_SCROLL;
          invalidate_window(win);
          return true;
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_LEFT:
          editor->camera[0] -= ED_SCROLL;
          // strafe_move = -1;
          invalidate_window(win);
          return true;
        case SDL_SCANCODE_ESCAPE:
          if (editor->drawing) {
            // Cancel current drawing
            editor->drawing = false;
            editor->num_draw_points = 0;
          } else if (game.state == GS_EDITOR) {
            // Exit editor mode
            toggle_editor_mode(editor);
          }
          return true;
        case SDL_SCANCODE_G:
          // Toggle grid size (8, 16, 32, 64, 128)
          editor->grid_size *= 2;
          if (editor->grid_size > 128) editor->grid_size = 8;
          invalidate_window(win);
          return true;
        case SDL_SCANCODE_SPACE: {
          mat4 mvp;
          get_editor_mvp(editor, mvp);
          get_mouse_position(editor, editor->cursor, mvp, &game.player.x);
          invalidate_window(win);
          return true;
        }
        case SDL_SCANCODE_LGUI:
          editor->move_camera = 1;
          return true;
      }
      break;
    case MSG_KEYUP:
      if (wparam == SDL_SCANCODE_LGUI) {
        editor->move_camera = 0;
      }
      return false;
  }
  return false;
}
