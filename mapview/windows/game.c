#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>

#include "../map.h"
#include "../sprites.h"
#include "../console.h"
#include "../editor.h"

bool init_sky(map_data_t const*);
const char *get_map_name(const char *name);

extern GLuint world_prog, ui_prog;
extern SDL_Window* window;
extern unsigned frame;

bool win_perf(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_statbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
bool win_editor(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Initialize player position based on map data
void init_player(map_data_t const *map, player_t *player) {
  memset(player, 0, sizeof(player_t));
  player->height = 41.0;  // Default DOOM player eye height
  
  // Find player start position (thing type 1)
  for (int i = 0; i < map->num_things; i++) {
    if (map->things[i].type == 1) {
      player->x = map->things[i].x;
      player->y = map->things[i].y;
      player->angle = map->things[i].angle;
      break;
    }
  }
  
  create_window(0, 0, 128, 64, "FPS", WINDOW_NOTITLE|WINDOW_TRANSPARENT, win_perf, NULL);
  create_window((screen_width-VGA_WIDTH)/2, (screen_height-VGA_HEGHT), VGA_WIDTH, VGA_HEGHT, "Statbar", WINDOW_NOTITLE|WINDOW_TRANSPARENT, win_statbar, NULL);
  //  create_window(32, 32, 512, 256, "Console", 0, win_console, NULL);
  extern editor_state_t editor;
  create_window(32, 32, 512, 256, "Game", 0, win_game, NULL);
  create_window(64, 64, 320, 320, "Editor", 0, win_editor, &editor);
}

void goto_map(const char *mapname) {
  game.map = load_map(mapname);
  
  if (game.map.num_vertices > 0) {
    print_map_info(&game.map);
    allocate_mapside_textures(&game.map);
    allocate_flat_textures(&game.map);
    init_sky(&game.map);
    init_player(&game.map, &game.player);
    build_wall_vertex_buffer(&game.map);
    build_floor_vertex_buffer(&game.map);
    
    game.state = GS_DUNGEON;
    
    conprintf("Successfully loaded map %s", get_map_name(mapname));
  } else {
    conprintf("Failed to load map %s", mapname);
  }
}

//#define ISOMETRIC

void get_view_matrix(map_data_t const *map, player_t const *player, float aspect, mat4 out) {
  // Convert angle to radians for direction calculation
#ifdef ISOMETRIC
  float angle_rad = (player->angle + 45*3) * M_PI / 180.0f + 0.001f;
  float pitch_rad = 60/*player->pitch*/ * M_PI / 180.0f;
#else
  float angle_rad = player->angle * M_PI / 180.0f + 0.001f;
  float pitch_rad = player->pitch * M_PI / 180.0f;
#endif
  
  // Calculate looking direction vector
  float look_dir_x = -cos(angle_rad);
  float look_dir_y = sin(angle_rad);
  float look_dir_z = sin(pitch_rad);
  
  float camera_dist = 500;
  
  // Scale the horizontal component of the look direction by the cosine of the pitch
  // This prevents the player from moving faster when looking up or down
  float cos_pitch = cos(pitch_rad);
  look_dir_x *= cos_pitch;
  look_dir_y *= cos_pitch;
  
  // Calculate look-at point
  float look_x = player->x + look_dir_x * camera_dist;
  float look_y = player->y + look_dir_y * camera_dist;
  float look_z = player->z + look_dir_z * camera_dist;
  
  // Create projection matrix
  mat4 proj;
  glm_perspective(glm_rad(90.0f), aspect, 1.0f, 2000.0f, proj);
  
  // Create view matrix
  mat4 view;
  vec3 eye = {player->x, player->y, player->z};
  vec3 center = {look_x, look_y, look_z}; // Look vector now includes pitch
  vec3 up = {0.0f, 0.0f, 1.0f};           // Z is up in Doom
  
  // Add a small offset to prevent precision issues with axis-aligned views
  if (fabs(player->pitch) > 89.5f) {
    // Adjust up vector when looking almost straight up or down
    up[0] = -sin(angle_rad);
    up[1] = -cos(angle_rad);
    up[2] = 0.0f;
  }
  
#ifdef ISOMETRIC
  glm_lookat(center, eye, up, view);
#else
  glm_lookat(eye, center, up, view);
#endif
  
  // Combine view and projection into MVP
  glm_mat4_mul(proj, view, out);
}

void draw_floors(map_data_t const *, mapsector_t const *, viewdef_t const *);
void draw_sky(map_data_t const *map, player_t const *player, mat4 mvp);
void draw_bsp(map_data_t const *map, viewdef_t const *viewdef);

int pixel = 0;

mapsector_t const *update_player_height(map_data_t const *map, player_t *player) {
  mapsector_t const *sector;
  if (player->sector != -1 && point_in_sector(map, player->x, player->y, player->sector)) {
    sector = &map->sectors[player->sector];
  } else {
    sector = find_player_sector(map, player->x, player->y);
  }
  if (sector) {
    player->z = sector->floorheight + EYE_HEIGHT;
    player->sector = (int)(sector-map->sectors);
  }
  return sector;
}

void draw_floor_ids(map_data_t const *, mapsector_t const *, viewdef_t const *);

static void
read_center_pixel(window_t const *win,
                  map_data_t const *map,
                  mapsector_t const *sector,
                  viewdef_t const *viewdef)
{
  glUseProgram(ui_prog);
  glUniformMatrix4fv(ui_prog_mvp, 1, GL_FALSE, viewdef->mvp[0]);
  
  draw_floor_ids(map, sector, viewdef);
  
  int fb_width, fb_height;
  int window_width, window_height;
  
  SDL_GL_GetDrawableSize(window, &fb_width, &fb_height);
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &window_width, &window_height);

  int x = (win->x + win->w / 2) * fb_width / screen_width;
  int y = fb_height - (win->y + win->h / 2) * fb_height / screen_height;

  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
}

void draw_dungeon(window_t const *win) {
  void draw_minimap(map_data_t const *, player_t const *);
  void draw_things(map_data_t const *, viewdef_t const *, bool);
  
  if (game.map.num_vertices == 0)
    return;

  map_data_t const *map = &game.map;
  mapsector_t const *sector = update_player_height(map, &game.player);
  player_t *player = &game.player;
  mat4 mvp;
  
  get_view_matrix(map, player, (float)win->w/(float)win->h, mvp);
  
  viewdef_t viewdef={0};
  memcpy(viewdef.mvp, mvp, sizeof(mat4));
  memcpy(viewdef.viewpos, &player->x, sizeof(vec3));
  viewdef.frame = frame++;
  glm_frustum_planes(mvp, viewdef.frustum);
  
  read_center_pixel(win, map, sector, &viewdef);
  
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  
  draw_sky(map, player, mvp);
  
  glUseProgram(world_prog);
  glUniformMatrix4fv(world_prog_mvp, 1, GL_FALSE, (const float*)mvp);
  
  extern int sectors_drawn;
  sectors_drawn = 0;
  
  viewdef.frame = frame++;
  
//  draw_bsp(&game.map, &viewdef);
  
  draw_floors(map, sector, &viewdef);
  
  draw_things(map, &viewdef, true);
  
  draw_weapon((float)win->w/(float)win->h);
  
  draw_crosshair((float)win->w/(float)win->h);

  mapside_texture_t const *tex1 = get_texture(get_selected_texture());
  if (tex1) {
    draw_rect(tex1->texture, 8, 8, tex1->width/2, tex1->height/2);
  }

  mapside_texture_t const *tex2 = get_flat_texture(get_selected_flat_texture());
  if (tex2) {
    float w = tex2->width/2, h = tex2->height/2;
    draw_rect(tex2->texture, win->w*0.75-8-w, 8, w, h);
  }

  extern bool mode;
  if (mode) {
    draw_minimap(map, player);
  }
}

extern editor_state_t editor;
extern bool mode;

void paint_face(map_data_t *map, bool eyedropper) {
  extern int pixel;
  if ((pixel&~PIXEL_MASK) < map->num_sidedefs) {
    switch (pixel&PIXEL_MASK) {
      case PIXEL_MID:
        if (eyedropper) {
          set_selected_texture(map->sidedefs[pixel&~PIXEL_MASK].midtexture);
        } else {
          memcpy(map->sidedefs[pixel&~PIXEL_MASK].midtexture, get_selected_texture(), sizeof(texname_t));
        }
        break;
      case PIXEL_BOTTOM:
        if (eyedropper) {
          set_selected_texture(map->sidedefs[pixel&~PIXEL_MASK].bottomtexture);
        } else {
          memcpy(map->sidedefs[pixel&~PIXEL_MASK].bottomtexture, get_selected_texture(), sizeof(texname_t));
        }
        break;
      case PIXEL_TOP:
        if (eyedropper) {
          set_selected_texture(map->sidedefs[pixel&~PIXEL_MASK].toptexture);
        } else {
          memcpy(map->sidedefs[pixel&~PIXEL_MASK].toptexture, get_selected_texture(), sizeof(texname_t));
        }
        break;
      case PIXEL_FLOOR:
        if (eyedropper) {
          set_selected_flat_texture(map->sectors[pixel&~PIXEL_MASK].floorpic);
        } else {
          memcpy(map->sectors[pixel&~PIXEL_MASK].floorpic, get_selected_flat_texture(), sizeof(texname_t));
        }
        break;
      case PIXEL_CEILING:
        if (eyedropper) {
          set_selected_flat_texture(map->sectors[pixel&~PIXEL_MASK].ceilingpic);
        } else {
          memcpy(map->sectors[pixel&~PIXEL_MASK].ceilingpic, get_selected_flat_texture(), sizeof(texname_t));
        }
        break;
    }
    build_wall_vertex_buffer(map);
    build_floor_vertex_buffer(map);
  }

}

bool win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  if (SDL_GetRelativeMouseMode()) {
    switch (msg) {
      case MSG_PAINT:
        draw_dungeon(win);
        return true;
      case MSG_KEYDOWN:
        switch (wparam) {
          case SDL_SCANCODE_ESCAPE:
            SDL_SetRelativeMouseMode(SDL_FALSE);
            break;
          case SDL_SCANCODE_W:
          case SDL_SCANCODE_UP:
            game.player.forward_move = 1;
            break;
          case SDL_SCANCODE_S:
          case SDL_SCANCODE_DOWN:
            game.player.forward_move = -1;
            break;
            // Calculate strafe direction vector (perpendicular to forward)
          case SDL_SCANCODE_D:
          case SDL_SCANCODE_RIGHT:
            game.player.strafe_move = 1;
            break;
          case SDL_SCANCODE_A:
          case SDL_SCANCODE_LEFT:
            game.player.strafe_move = -1;
            break;
          case SDL_SCANCODE_LSHIFT:
            mode = true;
            break;
        }
        return true;
      case MSG_KEYUP:
        switch (wparam) {
          case SDL_SCANCODE_W:
          case SDL_SCANCODE_UP:
          case SDL_SCANCODE_S:
          case SDL_SCANCODE_DOWN:
            game.player.forward_move = 0;
            break;
            // Calculate strafe direction vector (perpendicular to forward)
          case SDL_SCANCODE_D:
          case SDL_SCANCODE_RIGHT:
          case SDL_SCANCODE_A:
          case SDL_SCANCODE_LEFT:
            game.player.strafe_move = 0;
            break;
          case SDL_SCANCODE_LSHIFT:
            mode = false;
            break;
          default:
            break;
        }
        return true;
      case MSG_MOUSEMOVE:
        game.player.angle += ((int16_t)LOWORD((intptr_t)lparam)) * sensitivity_x;
        game.player.pitch -= ((int16_t)HIWORD((intptr_t)lparam)) * sensitivity_y;
        // Keep angle within 0-360 range
        if (game.player.angle < 0) game.player.angle += 360;
        if (game.player.angle >= 360) game.player.angle -= 360;
        // Clamp pitch to prevent flipping over
        if (game.player.pitch > 89.0f) game.player.pitch = 89.0f;
        if (game.player.pitch < -89.0f) game.player.pitch = -89.0f;
        return true;
      case MSG_LBUTTONUP:
        paint_face(&game.map, SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LALT]);
        return true;
      case MSG_JOYBUTTONDOWN:
        if (wparam == 0) {
          paint_face(&game.map, false);
        } else if (wparam == 1) {
          paint_face(&game.map, true);
        }
        return true;
      case MSG_JOYAXISMOTION:
        switch (LOWORD(wparam)) {
          case 0: game.player.strafe_move = ((int16_t)HIWORD(wparam))/(float)0x8000; break;
          case 1: game.player.forward_move = -((int16_t)HIWORD(wparam))/(float)0x8000; break;
          case 3: game.player.mouse_x_rel = ((int16_t)HIWORD(wparam))/1200.f; break;
          case 4: game.player.mouse_y_rel = ((int16_t)HIWORD(wparam))/1200.f; break;
        }
        return true;
    }
  } else {
    switch (msg) {
      case MSG_PAINT:
        draw_dungeon(win);
        return true;
      case MSG_LBUTTONUP:
        if (!SDL_GetRelativeMouseMode()) {
          SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        return true;
//      case MSG_KEYDOWN:
//        switch (wparam) {
//          case SDL_SCANCODE_ESCAPE:
//            SDL_SetRelativeMouseMode(SDL_FALSE);
//            //SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
//            break;
//        }
    }
  }
  return false;
}
