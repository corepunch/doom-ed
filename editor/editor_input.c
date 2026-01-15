#include <SDL2/SDL.h>
#include <cglm/struct.h>

#include <editor/editor.h>
#include <mapview/sprites.h>

#define SNAP_SIZE 10

extern SDL_Window* window;

// Utility function to snap coordinates to grid
//static void snap_to_grid(int *x, int *y, int grid_size) {
//  *x = (*x / grid_size) * grid_size;
//  *y = (*y / grid_size) * grid_size;
//}

/**
 * DEPRECATED: This function contains legacy direct SDL event polling.
 * 
 * Modern input handling is done through the window message system:
 * - Joystick events are routed via WM_JOYAXISMOTION and WM_JOYBUTTONDOWN
 * - See mapview/windows/game.c win_game() for proper joystick handling
 * - See mapview/editor_input.c win_editor() for proper editor input handling
 * 
 * This function should not be called. Input is now handled through window
 * procedures responding to window messages, which properly decouples SDL
 * and prepares for potential GLFW migration.
 */
void handle_editor_input(map_data_t *map,
                         editor_state_t *editor,
                         player_t *player,
                         float delta_time) {
  SDL_Event event;
  
  static float forward_move = 0, strafe_move = 0;
  
  // NOTE: This direct SDL_PollEvent is deprecated.
  // Input should be handled through window messages (WM_KEYDOWN, WM_JOYAXISMOTION, etc.)
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      extern bool running;
      running = false;
    }
    // REMOVED: Direct joystick event handling
    // Joystick events are now routed through the UI message system:
    // - WM_JOYBUTTONDOWN for button presses
    // - WM_JOYAXISMOTION for axis movement
    // See win_game() in mapview/windows/game.c for example usage
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
get_mouse_position(window_t *win,
                   editor_state_t const *editor,
                   int16_t const screen[2],
                   mat4 view_proj_matrix,
                   vec3 world)
{
  float z_plane = 0;
  
  // Convert to world coordinates
  int win_width = win->frame.w;
  int win_height = win->frame.h;
  
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
  
  snapped->x = world[0];
  snapped->y = world[1];
  
  world_x = world[0] + editor->grid_size/2;
  world_y = world[1] - editor->grid_size/2;
  
  // Snap to grid
  snapped->x = floorf(world_x / editor->grid_size) * editor->grid_size;
  snapped->y = ceilf(world_y / editor->grid_size) * editor->grid_size;
}

void draw_window_controls(window_t *win);
void get_editor_mvp(editor_state_t const *, mat4);

//#define ICON_STEP 12
//static int icon_x(window_t const *win, int i) {
//  return win->frame.x + 4 + i * ICON_STEP;
//}

result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_line(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_vertex(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_dummy(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void set_selection_mode(editor_state_t *editor, int mode) {
  switch ((editor->sel_mode = mode)) {
    case edit_things: g_inspector->proc = win_thing; break;
    case edit_sectors: g_inspector->proc = win_sector; break;
    case edit_lines: g_inspector->proc = win_line; break;
    default:
      return;
  }
  clear_window_children(g_inspector);
  send_message(g_inspector, WM_CREATE, 0, editor);
  invalidate_window(g_inspector);
}

static void update_inspector(editor_state_t *editor, objtype_t type) {
  extern window_t *_focused;
  winproc_t proc = NULL;
  window_t *old_focus = _focused;
  switch (type) {
    case obj_thing: proc = win_thing; break;
    case obj_sector: proc = win_sector; break;
    case obj_line: proc = win_line; break;
    case obj_point: proc = win_vertex; break;
    default: proc = win_dummy; break;
  }
  if (g_inspector->proc != proc) {
    g_inspector->proc = proc;
    clear_window_children(g_inspector);
    send_message(g_inspector, WM_CREATE, 0, editor);
    invalidate_window(g_inspector);
    set_focus(old_focus);
  }
}

static void editor_reset_input(editor_state_t *editor) {
  editor->drawing = false;
  editor->dragging = false;
  editor->move_camera = 0;
  editor->move_thing = 0;
  editor->num_draw_points = 0;
}

static void hover_sector(game_t *const *game, editor_selection_t *hover, float *world_1) {
  mapsector_t const *sec = find_player_sector(&(*game)->map, world_1[0], world_1[1]);
  if (sec) {
    hover->index = (int)(sec - (*game)->map.sectors);
    hover->type = obj_sector;
  }
}

static void hover_thing(game_t *game, editor_selection_t *hover, float *world_1) {
  for (int i = 0; i < game->map.num_things; i++) {
    mapthing_t const *th = &game->map.things[i];
    sprite_t *spr = get_thing_sprite_name(th->type, 0);
    if (!spr) continue;
    //            if (world_1[0] >= th->x - spr->offsetx &&
    //                world_1[1] >= th->y - spr->offsety &&
    //                world_1[0] < th->x - spr->offsetx + spr->width &&
    //                world_1[1] < th->y - spr->offsety + spr->height)
    if (world_1[0] >= th->x - spr->width/2 &&
        world_1[1] >= th->y - spr->height/2 &&
        world_1[0] < th->x + spr->width/2 &&
        world_1[1] < th->y + spr->height/2)
    {
      hover->type = obj_thing;
      hover->index = i;
    }
  }
}

float closest_point_on_line2(float point_x, float point_y,
                             map_data_t const *map,
                             uint16_t i,
                             float *x, float *y, float *z)
{
  return closest_point_on_line(point_x, point_y,
                               map->vertices[map->linedefs[i].start].x,
                               map->vertices[map->linedefs[i].start].y,
                               map->vertices[map->linedefs[i].end].x,
                               map->vertices[map->linedefs[i].end].y,
                               x, y, z);
}

static void hover_line(game_t *game, editor_selection_t *hover, float *world) {
  float dist = 100000;
  for (int i = 0; i < game->map.num_linedefs; i++) {
    float x, y, z;
    float d = closest_point_on_line2(world[0], world[1], &game->map, i, &x, &y, &z);
    if (dist > d) {
      dist = d;
      if (dist < SNAP_SIZE*SNAP_SIZE) {
        game->state.sn.x = x;
        game->state.sn.y = y;
        hover->index = i;
        hover->type = obj_line;
      }
    }
  }
}

static void hover_vertex(game_t *game, editor_selection_t *hover, float *world) {
  for (int i = 0; i < game->map.num_vertices; i++) {
    float dx = world[0] - game->map.vertices[i].x;
    float dy = world[1] - game->map.vertices[i].y;
    float d = dx*dx + dy*dy;
    if (d < SNAP_SIZE*SNAP_SIZE) {
      game->state.sn.x = game->map.vertices[i].x;
      game->state.sn.y = game->map.vertices[i].y;
      hover->index = i;
      hover->type = obj_point;
    }
  }
}
  
result_t win_editor(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  game_t *game = win->userdata;
  editor_state_t *editor = game ? &game->state : NULL;
  int point = -1;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      ((game_t *)lparam)->state.window = win;
      return true;
    case WM_DESTROY:
      free_map_data(&game->map);
      return true;
    case WM_PAINT:
      draw_editor(win, &game->map, editor, &game->player);
      return true;
    case WM_MOUSEMOVE:
      track_mouse(win);
      editor->hover.type = obj_none;
      editor->hover.index = 0xFFFF;
      if (editor->move_camera == 2 || editor->move_thing) {
        int16_t move_cursor[2] = { LOWORD(wparam), HIWORD(wparam) };
        vec3 world_1, world_2;
        mat4 mvp;
        get_editor_mvp(editor, mvp);
        get_mouse_position(win, editor, editor->cursor, mvp, world_1);
        get_mouse_position(win, editor, move_cursor, mvp, world_2);
        if (editor->move_thing && has_selection(editor->hover, obj_thing)) {
          uint16_t thing = editor->hover.index;
          game->map.things[thing].x -= world_1[0] - world_2[0];
          game->map.things[thing].y -= world_1[1] - world_2[1];
          assign_thing_sector(&game->map, &game->map.things[thing]);
        } else {
          editor->camera[0] += world_1[0] - world_2[0];
          editor->camera[1] += world_1[1] - world_2[1];
        }
        editor->cursor[0] = LOWORD(wparam);
        editor->cursor[1] = HIWORD(wparam);
      } else {
        editor->cursor[0] = LOWORD(wparam);
        editor->cursor[1] = HIWORD(wparam);
        vec3 world_1;
        mat4 mvp;
        get_editor_mvp(editor, mvp);
        get_mouse_position(win, editor, editor->cursor, mvp, world_1);
        editor->sn.x = world_1[0];
        editor->sn.y = world_1[1];
        snap_mouse_position(editor, world_1, &editor->sn);
        if (editor->sel_mode == edit_select) {
          hover_sector(&game, &editor->hover, world_1);
          hover_line(game, &editor->hover, world_1);
          hover_vertex(game, &editor->hover, world_1);
          hover_thing(game, &editor->hover, world_1);
//          update_inspector(editor, editor->hover.type);
        } else if (editor->sel_mode == edit_vertices) {
          hover_line(game, &editor->hover, world_1);
          hover_vertex(game, &editor->hover, world_1);
          if (editor->hover.type != obj_line) {
            editor->hover.type = obj_none;
            editor->hover.index = 0xFFFF;
          }
        }
      }
      invalidate_window(win);
      invalidate_window(g_inspector);
      return true;
      //    case WM_WHEEL:
      //      editor->scale *= 1.f - (int16_t)HIWORD(wparam)/50.f;
      //      invalidate_window(win);
      //      return true;
    case WM_MOUSELEAVE:
//      editor->hover.point = -1;
//      editor->hover.sector = -1;
//      editor->hover.line = -1;
//      editor->hover.thing = -1;
      invalidate_window(win);
      invalidate_window(g_inspector);
      return true;
    case WM_WHEEL: {
      mat4 mvp;
      vec3 world_before, world_after, delta;
      get_editor_mvp(editor, mvp);
      get_mouse_position(win, editor, editor->cursor, mvp, world_before);
      editor->scale *= MAX(1.f - (int16_t)HIWORD(wparam) / 50.f, 0.1f);
      get_editor_mvp(editor, mvp);
      get_mouse_position(win, editor, editor->cursor, mvp, world_after);
      glm_vec2_sub(world_before, world_after, delta);
      glm_vec2_add(editor->camera, delta, editor->camera);
      invalidate_window(win);
      return true;
    }
    case WM_LBUTTONUP:
      if (editor->move_camera == 2) {
        editor->move_camera = 1;
      } else if (editor->sel_mode == edit_select) {
        memcpy(&editor->selected, &editor->hover, sizeof(editor_selection_t));
        update_inspector(editor, editor->selected.type);
      } else {
//        editor->move_thing = 0;
      }
      return true;
    case WM_LBUTTONDOWN:
      if (editor->move_camera > 0) {
        editor->move_camera = 2;
        return true;
      } else switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->dragging) {
            editor->dragging = false;
            editor->hover.index = -1;
          } else if (point_exists(editor->sn, &game->map, &point)) {
            editor->hover.index = point;
            editor->hover.type = obj_point;
          } else if (has_selection(editor->hover, obj_line)) {
            editor->hover.index = split_linedef(&game->map, editor->hover.index, editor->sn.x, editor->sn.y);
            editor->hover.type = obj_point;
          } else {
            editor->hover.index = add_vertex(&game->map, editor->sn);
            editor->hover.type = obj_point;
          }
          if (editor->drawing &&
              has_selection(editor->selected, obj_point) &&
              has_selection(editor->hover, obj_point))
          {
            mapvertex_t a = game->map.vertices[editor->selected.index];
            mapvertex_t b = game->map.vertices[editor->hover.index];
            uint16_t sec = find_point_sector(&game->map, vertex_midpoint(a, b));
            uint16_t sd = sec != 0xFFFF ? add_sidedef(&game->map, sec) : 0xFFFF;
            uint16_t line = add_linedef(&game->map, editor->selected.index, editor->hover.index, sd, sd);
            uint16_t vertices[256];
            int num_vertices = check_closed_loop(&game->map, line, vertices);
            if (num_vertices) {
              uint16_t sector = add_sector(&game->map);
              set_loop_sector(&game->map, sector, vertices, num_vertices);
              editor->drawing = false;
            }
          } else {
            editor->drawing = true;
          }
          memcpy(&editor->selected, &editor->hover, sizeof(editor->selected));
          break;
        case edit_things:
          editor->selected.index = editor->hover.index;
          editor->selected.type = editor->hover.type;
          if (!has_selection(editor->hover, obj_point)) {
            int16_t cursor[2] = { LOWORD(wparam), HIWORD(wparam) };
            vec3 world;
            mat4 mvp;
            get_editor_mvp(editor, mvp);
            get_mouse_position(win, editor, cursor, mvp, world);
            add_thing(&game->map, (mapthing_t){
              .x = world[0],
              .y = world[1],
              .type = editor->selected_thing_type,
            });
          } else {
            editor->move_thing = 1;
          }
          invalidate_window(win);
          break;
        case edit_sectors:
          editor->selected.index = editor->hover.index;
          editor->selected.type = editor->hover.type;
          invalidate_window(win);
          break;
        case edit_lines:
          editor->selected.index = editor->hover.index;
          editor->selected.type = editor->hover.type;
          invalidate_window(win);
          break;
      }
      return true;
    case WM_RBUTTONDOWN:
      switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->drawing) {
            // Cancel current drawing
            editor->drawing = false;
            editor->num_draw_points = 0;
          } else if (has_selection(editor->hover, obj_line)) {
            editor->hover.index = split_linedef(&game->map, editor->hover.index, editor->sn.x, editor->sn.y);
            editor->hover.type = obj_point;
          } else if (point_exists(editor->sn, &game->map, &point)) {
            editor->hover.index = point;
            editor->hover.type = obj_point;
            editor->dragging = true;
          }
          break;
      }
      return true;
    case WM_RBUTTONUP:
      switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->dragging && has_selection(editor->hover, obj_point)) {
            editor->dragging = false;
            game->map.vertices[editor->hover.index] = editor->sn;
            // Rebuild vertex buffers
            build_wall_vertex_buffer(&game->map);
            build_floor_vertex_buffer(&game->map);
          }
          return true;
      }
      return true;
    case WM_KILLFOCUS:
      editor_reset_input(editor);
      return true;
    case WM_KEYDOWN:
      switch (wparam) {
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
          get_mouse_position(win, editor, editor->cursor, mvp, &game->player.x);
          invalidate_window(win);
          return true;
        }
        case SDL_SCANCODE_LGUI:
          editor->move_camera = 1;
          return true;
        case SDL_SCANCODE_TAB:
          editor_reset_input(editor);
          win->proc = win_game;
          set_capture(win);
          SDL_SetRelativeMouseMode(SDL_TRUE);
          invalidate_window(win);
          return true;
      }
      break;
    case WM_KEYUP:
      if (wparam == SDL_SCANCODE_LGUI) {
        editor->move_camera = 0;
      }
      return false;
  }
  return false;
}
