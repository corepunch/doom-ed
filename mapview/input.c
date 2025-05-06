#include <SDL2/SDL.h>
#include <limits.h>

#include "map.h"
#include "editor.h"

extern SDL_Window* window;
extern bool running;
extern bool mode;

uint32_t selected_texture = 0;
uint32_t selected_floor_texture = 0;

bool point_in_sector(map_data_t const* map, int x, int y, int sector_index) {
  int inside = 0;
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t* line = &map->linedefs[i];
    for (int s = 0; s < 2; s++) {
      int sidenum = line->sidenum[s];
      if (sidenum == 0xFFFF) continue;
      if (map->sidedefs[sidenum].sector != sector_index) continue;
      
      mapvertex_t* v1 = &map->vertices[line->start];
      mapvertex_t* v2 = &map->vertices[line->end];
      
      if (((v1->y > y) != (v2->y > y)) &&
          (x < (v2->x - v1->x) * (y - v1->y) / (v2->y - v1->y) + v1->x))
        inside = !inside;
    }
  }
  return inside;
}

//mapsector_t const *find_player_sector(map_data_t const *map, int x, int y) {
//  for (int i = 0; i < map->num_sectors; i++)
//    if (point_in_sector(map, x, y, i))
//      return &map->sectors[i];
//  return NULL; // not found
//}

mapsector_t const *find_player_sector(map_data_t const *map, int x, int y) {
  mapsector_t const *highest_sector = NULL;
  int highest_floor = INT_MIN;
  
//  // Check center point and four points at player radius distance
//  int check_points[5][2] = {
//    {x, y},               // Center
//    {x - P_RADIUS, y},    // Left
//    {x + P_RADIUS, y},    // Right
//    {x, y - P_RADIUS},    // Up
//    {x, y + P_RADIUS}     // Down
//  };
//  
//  // Check each sample point
//  for (int p = 0; p < 5; p++) {
//    int px = check_points[p][0];
//    int py = check_points[p][1];
    int px = x;
    int py = y;

    for (int i = 0; i < map->num_sectors; i++) {
      if (point_in_sector(map, px, py, i)) {
        // If this is a higher sector than what we've found so far, remember it
        if (!highest_sector || map->sectors[i].floorheight > highest_floor) {
          highest_sector = &map->sectors[i];
          highest_floor = map->sectors[i].floorheight;
        }
      }
    }
//  }
  
  return highest_sector; // Returns NULL if no sector was found
}

void handle_scroll(SDL_Event event, map_data_t *map) {
  extern int pixel;
  static int buffer = 0;
  buffer += event.wheel.y;
  
  // Reset buffer if scroll direction changed
  if ((buffer ^ event.wheel.y) < 0) {
    buffer = event.wheel.y;
  }
  
  int move;
  
  // Correct rounding for both positive and negative
  if (buffer >= 0) {
    move = (buffer & ~7); // positive side: floor down
  } else {
    move = -((-buffer) & ~7); // negative side: floor up
  }
  
  if (move != 0) {
    buffer -= move;
    
    switch (pixel&PIXEL_MASK) {
      case PIXEL_FLOOR:
        map->sectors[pixel&~PIXEL_MASK].floorheight -= move;
        break;
      case PIXEL_CEILING:
        map->sectors[pixel&~PIXEL_MASK].ceilingheight -= move;
        break;
      case PIXEL_MID:
      case PIXEL_TOP:
      case PIXEL_BOTTOM:
        map->sidedefs[pixel&~PIXEL_MASK].rowoffset -= move;
        break;
      default:
        break;
    }
    
    build_wall_vertex_buffer(map);
    build_floor_vertex_buffer(map);
  }
}

/**
 * Handle player input including mouse movement for camera control
 * @param map Pointer to the map data
 * @param player Pointer to the player object
 */
void handle_input(map_data_t *map, player_t *player, float delta_time) {
  extern editor_state_t editor;
  // If in editor mode, use the editor input handler
  if (editor.active) {
    handle_editor_input(map, &editor, player);
    return;
  }
  
  SDL_Event event;
  const Uint8* keystates = SDL_GetKeyboardState(NULL);
  
  // Variables for mouse movement
  static int mouse_x_rel = 0;
  static int mouse_y_rel = 0;
  static float forward_move = 0;
  static float strafe_move = 0;

  if (SDL_GetRelativeMouseMode()) {
    mouse_x_rel = 0;
    mouse_y_rel = 0;
  }
  
  // Center position for relative mouse mode
  int window_width, window_height;
  SDL_GetWindowSize(window, &window_width, &window_height); // Assuming 'window' is accessible
  
  // Process SDL events
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      running = false;
    }
    else if (event.type == SDL_JOYAXISMOTION) {
      printf("Axis %d = %d\n", event.jaxis.axis, event.jaxis.value);
      switch (event.jaxis.axis) {
        case 3: mouse_x_rel = event.jaxis.value/1200.f; break;
        case 4: mouse_y_rel = event.jaxis.value/1200.f; break;
        case 0: strafe_move = event.jaxis.value/(float)0x8000; break;
        case 1: forward_move = -event.jaxis.value/(float)0x8000; break;
      }
//    } else if (event.type == SDL_JOYBUTTONDOWN) {
//      printf("Button %d pressed\n", event.jbutton.button);
//    } else if (event.type == SDL_JOYBUTTONUP) {
//      printf("Button %d released\n", event.jbutton.button);
    } else if (event.type == SDL_JOYHATMOTION) {
      printf("Hat %d moved to %d\n", event.jhat.hat, event.jhat.value);
    }
    else if (event.type == SDL_MOUSEMOTION) {
      if (SDL_GetRelativeMouseMode()) {
        // Get relative mouse movement
        mouse_x_rel = event.motion.xrel;
        mouse_y_rel = event.motion.yrel;
      }
    }
    else if (event.type == SDL_MOUSEWHEEL) {
      handle_scroll(event, map);
    }
    else if (event.type == SDL_MOUSEBUTTONUP || (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button==0)) {
      extern int pixel;
      if ((pixel&~PIXEL_MASK) < map->num_sidedefs) {
        switch (pixel&PIXEL_MASK) {
          case PIXEL_MID:
            memcpy(map->sidedefs[pixel&~PIXEL_MASK].midtexture,
                   get_texture_name(selected_texture),
                   sizeof(texname_t));
            break;
          case PIXEL_BOTTOM:
            memcpy(map->sidedefs[pixel&~PIXEL_MASK].bottomtexture,
                   get_texture_name(selected_texture),
                   sizeof(texname_t));
            break;
          case PIXEL_TOP:
            memcpy(map->sidedefs[pixel&~PIXEL_MASK].toptexture,
                   get_texture_name(selected_texture),
                   sizeof(texname_t));
            break;
          case PIXEL_FLOOR:
            memcpy(map->sectors[pixel&~PIXEL_MASK].floorpic,
                   get_flat_texture_name(selected_floor_texture),
                   sizeof(texname_t));
            break;
          case PIXEL_CEILING:
            memcpy(map->sectors[pixel&~PIXEL_MASK].ceilingpic,
                   get_flat_texture_name(selected_floor_texture),
                   sizeof(texname_t));
            break;
        }
        build_wall_vertex_buffer(map);
        build_floor_vertex_buffer(map);
      }
    }
    else if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_ESCAPE:
          SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
          break;
        case SDL_SCANCODE_Z:
          selected_texture--;
          break;
        case SDL_SCANCODE_X:
          selected_texture++;
          break;
        case SDL_SCANCODE_C:
          selected_floor_texture--;
          break;
        case SDL_SCANCODE_V:
          selected_floor_texture++;
          break;
        case SDL_SCANCODE_TAB:
          toggle_editor_mode(&editor);
          break;
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
          forward_move = 1;
          break;
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
          forward_move = -1;
          break;
          // Calculate strafe direction vector (perpendicular to forward)
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_RIGHT:
          strafe_move = 1;
          break;
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_LEFT:
          strafe_move = -1;
          break;
        default:
          break;
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
  }
  
  // Apply mouse rotation if relative mode is enabled
  if (true) {
    // Horizontal mouse movement controls yaw (left/right rotation)
    float sensitivity_x = 0.15f; // Adjust sensitivity as needed
    player->angle += mouse_x_rel * sensitivity_x;
    
    // Keep angle within 0-360 range
    if (player->angle < 0) player->angle += 360;
    if (player->angle >= 360) player->angle -= 360;
    
    // Vertical mouse movement controls pitch (up/down looking)
    float sensitivity_y = 0.25f; // Adjust sensitivity as needed
    player->pitch -= mouse_y_rel * sensitivity_y;
    
    // Clamp pitch to prevent flipping over
    if (player->pitch > 89.0f) player->pitch = 89.0f;
    if (player->pitch < -89.0f) player->pitch = -89.0f;
  }
  
  // Convert player angle to radians for movement calculations
  float angle_rad = player->angle * M_PI / 180.0;
  
  // Calculate forward/backward direction vector
  float forward_x = -forward_move * cos(angle_rad) * MOVEMENT_SPEED; // Negative because DOOM's coordinate system
  float forward_y = forward_move * sin(angle_rad) * MOVEMENT_SPEED;
  float strafe_x = strafe_move * sin(angle_rad) * MOVEMENT_SPEED;
  float strafe_y = strafe_move * cos(angle_rad) * MOVEMENT_SPEED;
  
  // Combine movement vectors
  float move_x = forward_x + strafe_x;
  float move_y = forward_y + strafe_y;
  
  // If there's any movement, apply it with collision detection and sliding
  if (move_x != 0.0f || move_y != 0.0f) {
//    update_player_position_with_sliding(map, player, move_x, move_y);
    player->x += move_x * delta_time;
    player->y += move_y * delta_time;
  }
  
  // Additional vertical movement if needed (flying up/down)
  if (keystates[SDL_SCANCODE_E]) {
    //    player->z += MOVEMENT_SPEED; // Move up
  }
  if (keystates[SDL_SCANCODE_Q]) {
    //    player->z -= MOVEMENT_SPEED; // Move down
  }
  
  // Toggle mode with Shift
  if (keystates[SDL_SCANCODE_LSHIFT]) {
    mode = true;
  }
}
