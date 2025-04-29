#include <SDL2/SDL.h>

#include "editor.h"

extern SDL_Window* window;

// Utility function to snap coordinates to grid
static void snap_to_grid(int *x, int *y, int grid_size) {
  *x = (*x / grid_size) * grid_size;
  *y = (*y / grid_size) * grid_size;
}

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
    else if (event.type == SDL_MOUSEBUTTONDOWN && editor->active) {
      if (event.button.button == SDL_BUTTON_LEFT) {
        int snapped_x, snapped_y;
        snap_mouse_position(editor, player, &snapped_x, &snapped_y);
        
        if (!editor->drawing) {
          // Start drawing new sector
          editor->drawing = true;
          editor->num_draw_points = 0;
        }
        
        // Check if we're clicking on the first point to close the loop
        if (editor->num_draw_points > 2 &&
            abs(snapped_x - editor->draw_points[0][0]) < editor->grid_size/2 &&
            abs(snapped_y - editor->draw_points[0][1]) < editor->grid_size/2) {
          // Finish the sector
          finish_sector(map, editor);
        } else if (editor->num_draw_points < MAX_DRAW_POINTS) {
          // Add point to current sector
          editor->draw_points[editor->num_draw_points][0] = snapped_x;
          editor->draw_points[editor->num_draw_points][1] = snapped_y;
          editor->num_draw_points++;
        }
      }
      else if (event.button.button == SDL_BUTTON_RIGHT) {
        // Cancel current drawing
        editor->drawing = false;
        editor->num_draw_points = 0;
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
                    float *world_x,
                    float *world_y)
{
  int mouse_x, mouse_y;
  // Get mouse position
  SDL_GetMouseState(&mouse_x, &mouse_y);
  
  // Convert to world coordinates
  int win_width, win_height;
  SDL_GetWindowSize(window, &win_width, &win_height);
  
  float w = SCREEN_WIDTH * 2;
  float h = SCREEN_HEIGHT * 2;
  
  *world_x = player->x + (mouse_x - win_width/2) * (w / win_width);
  *world_y = player->y + (win_height/2 - mouse_y) * (h / win_height);
}

void
snap_mouse_position(editor_state_t const *editor,
                   player_t const *player,
                   int *snapped_x,
                   int *snapped_y)
{
  float world_x, world_y;

  get_mouse_position(editor, player, &world_x, &world_y);
  
  world_x -= editor->grid_size/2;
  world_y += editor->grid_size/2;
  
  // Snap to grid
  *snapped_x = ((int)world_x / editor->grid_size) * editor->grid_size;
  *snapped_y = ((int)world_y / editor->grid_size) * editor->grid_size;
}
