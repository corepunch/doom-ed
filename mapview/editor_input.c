#include <SDL2/SDL.h>
#include <cglm/struct.h>

#include "editor.h"
#include "sprites.h"

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
  int win_width = editor->window->frame.w;
  int win_height = editor->window->frame.h;
  
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

void draw_window_controls(window_t *win);
void get_editor_mvp(editor_state_t const *, mat4);

#define ICON_STEP 12

static int icon_x(window_t const *win, int i) {
  return win->frame.x + 4 + i * ICON_STEP;
}

bool win_editor(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mapvertex_t sn;
  editor_state_t *editor = win->userdata;
  map_data_t *map = &game.map;
  int point = -1;
  int old_point = editor ? editor->hover.point : -1;
  switch (msg) {
    case WM_NCPAINT:
      for (int i = 0; i < 6; i++) {
        draw_icon(icon_points + i, icon_x(win, i), window_title_bar_y(win),
                  editor->sel_mode == i ? 1.0f : 0.5f);
      }
      return true;
    case WM_NCLBUTTONUP:
      if ((LOWORD(wparam) - icon_x(win, 0)) / ICON_STEP < 5 && LOWORD(wparam) >= icon_x(win, 0)) {
        editor->sel_mode = (LOWORD(wparam) - icon_x(win, 0)) / ICON_STEP;
        post_message(win, WM_NCPAINT, 0, NULL);
      }
      return true;
    case WM_CREATE:
      win->userdata = lparam;
      ((editor_state_t *)lparam)->window = win;
      return true;
    case WM_PAINT:
      draw_editor(&game.map, win->userdata, &game.player);
      return true;
    case WM_MOUSEMOVE:
      track_mouse(win);
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
        vec3 world_1;
        mat4 mvp;
        get_editor_mvp(editor, mvp);
        get_mouse_position(editor, editor->cursor, mvp, world_1);
        if (editor->sel_mode == edit_sectors) {
          mapsector_t const *sec = find_player_sector(&game.map, world_1[0], world_1[1]);
          editor->hover.sector = sec ? (int)(sec - game.map.sectors) : -1;
        } else if (editor->sel_mode == edit_things) {
          editor->hover.thing = -1;
          for (int i = 0; i < game.map.num_things; i++) {
            mapthing_t const *th = &game.map.things[i];
            sprite_t *get_thing_sprite_name(int thing_type, int angle);
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
              editor->hover.thing = i;
            }
          }
        }
      }
      invalidate_window(editor->window);
      invalidate_window(editor->inspector);
      return true;
      //    case WM_WHEEL:
      //      editor->scale *= 1.f - (int16_t)HIWORD(wparam)/50.f;
      //      invalidate_window(win);
      //      return true;
    case WM_MOUSELEAVE:
      editor->hover.point = -1;
      editor->hover.sector = -1;
      editor->hover.linedef = -1;
      editor->hover.thing = -1;
      invalidate_window(editor->window);
      invalidate_window(editor->inspector);
      return true;
    case WM_WHEEL: {
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
    case WM_LBUTTONUP:
      if (editor->move_camera == 2) {
        editor->move_camera = 1;
      } switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->dragging) {
            extern mapvertex_t sn;
            editor->dragging = false;
            map->vertices[editor->hover.point] = sn;
            // Rebuild vertex buffers
            build_wall_vertex_buffer(map);
            build_floor_vertex_buffer(map);
          }
          return true;
      }
      return true;
    case WM_LBUTTONDOWN:
      if (editor->move_camera > 0) {
        editor->move_camera = 2;
        return true;
      } else switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->drawing) {
            // Cancel current drawing
            editor->drawing = false;
            editor->num_draw_points = 0;
          } else if (editor->hover.linedef != 0xFFFF) {
            editor->hover.point = split_linedef(map, editor->hover.linedef, sn.x, sn.y);
          } else if (point_exists(sn, map, &point)) {
            editor->hover.point = point;
            editor->dragging = true;
          }
          break;
        case edit_things:
          editor->selected.thing = editor->hover.thing;
          invalidate_window(win);
          break;
        case edit_sectors:
          editor->selected.sector = editor->hover.sector;
          invalidate_window(win);
          break;
      }
      return true;
    case WM_RBUTTONDOWN:
      switch (editor->sel_mode) {
        case edit_vertices:
          if (editor->dragging) {
            editor->dragging = false;
            editor->hover.point = -1;
          } else if (point_exists(sn, map, &point)) {
            editor->hover.point = point;
          } else if (editor->hover.linedef != 0xFFFF) {
            editor->hover.point = split_linedef(map, editor->hover.linedef, sn.x, sn.y);
          } else {
            editor->hover.point = add_vertex(map, sn);
          }
          if (editor->drawing) {
            mapvertex_t a = map->vertices[old_point];
            mapvertex_t b = map->vertices[editor->hover.point];
            uint16_t sec = find_point_sector(map, vertex_midpoint(a, b));
            uint16_t line = -1;
            if (sec != 0xFFFF) {
              line = add_linedef(map, old_point, editor->hover.point, add_sidedef(map, sec), add_sidedef(map, sec));
            } else {
              line = add_linedef(map, old_point, editor->hover.point, 0xFFFF, 0xFFFF);
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
          break;
      }
      return true;
    case WM_RBUTTONUP:
      return true;

    case WM_KEYDOWN:
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
    case WM_KEYUP:
      if (wparam == SDL_SCANCODE_LGUI) {
        editor->move_camera = 0;
      }
      return false;
  }
  return false;
}

//const char *modes[] = { "vertices", "things", "lines", "sectors" };
//uint32_t colors[] = {
//  0xFF3C3C90,
//  0xFF4D9944,
//  0xFF8870AA,
//  0xFF88AA66,
//  0xFF7A4C88,
//};

bool win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

enum {
  ID_THING_TYPE = 1000,
  ID_THING_POS_X,
  ID_THING_POS_Y,
  ID_THING_SPRITE,
  ID_THING_EASY,
  ID_THING_NORMAL,
  ID_THING_HARD,
  ID_THING_FIGHTER,
  ID_THING_CLERIC,
  ID_THING_MAGE,
  ID_THING_GSINGLE,
  ID_THING_GCOOP,
  ID_THING_GDEATHMATCH,
  ID_THING_AMBUSH,
  ID_THING_DORMANT,
};

#define MTF_EASY    1
#define MTF_NORMAL    2
#define MTF_HARD    4
#define MTF_AMBUSH    8
#define MTF_DORMANT    16
#define MTF_FIGHTER    32
#define MTF_CLERIC    64
#define MTF_MAGE    128
#define MTF_GSINGLE    256
#define MTF_GCOOP    512
#define MTF_GDEATHMATCH  1024

windef_t thing_layout[] = {
  { "TEXT", "Type:", -1, 4, 13, 50, 10 },
  { "TEXT", "Position X:", -1, 4, 33, 50, 10 },
  { "TEXT", "Position Y:", -1, 4, 53, 50, 10 },
  { "BUTTON", "Click me", ID_THING_TYPE, 64, 10, 50, 10 },
  { "EDITTEXT", "", ID_THING_POS_X, 64, 30, 50, 10 },
  { "EDITTEXT", "", ID_THING_POS_Y, 64, 50, 50, 10 },
  { "SPRITE", "", ID_THING_SPRITE, 4, 70, 64, 64 },
  { "CHECKBOX", "Easy", ID_THING_EASY, 4, 140, 20, 10 },
  { "CHECKBOX", "Medium", ID_THING_NORMAL, 4, 155, 20, 10 },
  { "CHECKBOX", "Hard", ID_THING_HARD, 4, 170, 20, 10 },
  { "CHECKBOX", "Fighter", ID_THING_FIGHTER, 4, 190, 20, 10 },
  { "CHECKBOX", "Cleric", ID_THING_CLERIC, 4, 205, 20, 10 },
  { "CHECKBOX", "Mage", ID_THING_MAGE, 4, 220, 20, 10 },
  { "CHECKBOX", "Single", ID_THING_GSINGLE, 64, 140, 20, 10 },
  { "CHECKBOX", "Coop", ID_THING_GCOOP, 64, 155, 20, 10 },
  { "CHECKBOX", "Deathmatch", ID_THING_GDEATHMATCH, 64, 170, 20, 10 },
  { "CHECKBOX", "Ambush", ID_THING_AMBUSH, 64, 190, 20, 10 },
  { "CHECKBOX", "Dormant", ID_THING_DORMANT, 64, 205, 20, 10 },
  { NULL }
};

uint32_t thing_checkboxes[] = {
  ID_THING_EASY,
  ID_THING_NORMAL,
  ID_THING_HARD,
  ID_THING_AMBUSH,
  ID_THING_DORMANT,
  ID_THING_FIGHTER,
  ID_THING_CLERIC,
  ID_THING_MAGE,
  ID_THING_GSINGLE,
  ID_THING_GCOOP,
  ID_THING_GDEATHMATCH,
};

mapthing_t *selected_thing(editor_state_t *editor) {
  if (editor->hover.thing != 0xFFFF) {
    return &game.map.things[editor->hover.thing];
  } else if (editor->selected.thing != 0xFFFF) {
    return &game.map.things[editor->selected.thing];
  } else {
    return NULL;
  }
}

mapsector_t *selected_sector(editor_state_t *editor) {
  if (editor->hover.sector != 0xFFFF) {
    return &game.map.sectors[editor->hover.sector];
  } else if (editor->selected.sector != 0xFFFF) {
    return &game.map.sectors[editor->selected.sector];
  } else {
    return NULL;
  }
}

bool win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = win->userdata;
  mapthing_t *thing;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      ((editor_state_t*)lparam)->inspector = win;
      load_window_children(win, thing_layout);
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_things && (thing = selected_thing(editor))) {
        sprite_t *get_thing_sprite_name(int thing_type, int angle);
        sprite_t *spr = get_thing_sprite_name(thing->type, 0);
        set_window_item_text(win, ID_THING_SPRITE, spr?spr->name:"");
        set_window_item_text(win, ID_THING_TYPE, "%d", thing->height);
        set_window_item_text(win, ID_THING_POS_X, "%d", thing->x);
        set_window_item_text(win, ID_THING_POS_Y, "%d", thing->y);
        for (int i = 0; i < sizeof(thing_checkboxes)/sizeof(*thing_checkboxes); i++) {
          window_t *checkbox = get_window_item(win, thing_checkboxes[i]);
          uint32_t value = thing->options&(1<<i);
          send_message(checkbox, BM_SETCHECK, value, NULL);
        }
      }
      return false;
    case WM_COMMAND:
      if (editor->sel_mode == edit_things && (thing = selected_thing(editor))) {
        for (int i = 0; i < sizeof(thing_checkboxes)/sizeof(*thing_checkboxes); i++) {
          if (wparam == MAKEDWORD(thing_checkboxes[i], BN_CLICKED)) {
            if (send_message(lparam, BM_GETCHECK, 0, NULL)) {
              thing->options |= 1 << i;
            } else {
              thing->options &= ~(1 << i);
            }
          }
        }
        if (wparam == MAKEDWORD(ID_THING_POS_X, EN_UPDATE)) {
          thing->x = atoi(((window_t *)lparam)->title);
          invalidate_window(editor->window);
        }
        if (wparam == MAKEDWORD(ID_THING_POS_Y, EN_UPDATE)) {
          thing->y = atoi(((window_t *)lparam)->title);
          invalidate_window(editor->window);
        }
      }
      return true;
  }
  return false;
}

enum {
  ID_SECTOR_FLOOR_HEIGHT = 1000,
  ID_SECTOR_FLOOR_IMAGE,
  ID_SECTOR_CEILING_HEIGHT,
  ID_SECTOR_CEILING_IMAGE,
};

windef_t sector_layout[] = {
//  { "TEXT", "Type:", -1, 4, 13, 50, 10 },
  { "TEXT", "Floor Hgt:", -1, 4, 13, 50, 10 },
  { "TEXT", "Ceiling Hgt:", -1, 4, 33, 50, 10 },
  { "EDITTEXT", "", ID_SECTOR_FLOOR_HEIGHT, 64, 10, 50, 10 },
  { "EDITTEXT", "", ID_SECTOR_CEILING_HEIGHT, 64, 30, 50, 10 },
  { "SPRITE", "", ID_SECTOR_FLOOR_IMAGE, 4, 52, 64, 64 },
  { "SPRITE", "", ID_SECTOR_CEILING_IMAGE, 76, 52, 64, 64 },
  { NULL }
};

bool win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = win->userdata;
  mapsector_t *sector;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      ((editor_state_t*)lparam)->inspector = win;
      load_window_children(win, sector_layout);
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_sectors && (sector = selected_sector(editor))) {
//        sprite_t *spr = get_thing_sprite_name(thing->type, 0);
        set_window_item_text(win, ID_SECTOR_FLOOR_HEIGHT, "%d", sector->floorheight);
        set_window_item_text(win, ID_SECTOR_FLOOR_IMAGE, sector->floorpic);
        set_window_item_text(win, ID_SECTOR_CEILING_HEIGHT, "%d", sector->ceilingheight);
        set_window_item_text(win, ID_SECTOR_CEILING_IMAGE, sector->ceilingpic);
//        set_window_item_text(win, ID_THING_POS_Y, "%d", thing->y);
//        for (int i = 0; i < sizeof(checkboxes)/sizeof(*checkboxes); i++) {
//          window_t *checkbox = get_window_item(win, checkboxes[i]);
//          uint32_t value = thing->options&(1<<i);
//          send_message(checkbox, BM_SETCHECK, value, NULL);
//        }
      }
      return false;
//    case WM_COMMAND:
//      if (editor->selected.thing != 0xFFFF) {
//        mapthing_t *thing = &game.map.things[editor->selected.thing];
//        for (int i = 0; i < sizeof(checkboxes)/sizeof(*checkboxes); i++) {
//          if (wparam == MAKEDWORD(checkboxes[i], BN_CLICKED)) {
//            if (send_message(lparam, BM_GETCHECK, 0, NULL)) {
//              thing->options |= 1 << i;
//            } else {
//              thing->options &= ~(1 << i);
//            }
//          }
//        }
//        if (wparam == MAKEDWORD(ID_THING_POS_X, EN_UPDATE)) {
//          thing->x = atoi(((window_t *)lparam)->title);
//          invalidate_window(editor->window);
//        }
//        if (wparam == MAKEDWORD(ID_THING_POS_Y, EN_UPDATE)) {
//          thing->y = atoi(((window_t *)lparam)->title);
//          invalidate_window(editor->window);
//        }
//      }
//      return true;
  }
  return false;
}
