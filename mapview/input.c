#include <SDL2/SDL.h>

#include "map.h"

extern SDL_Window* window;
extern bool running;
extern bool mode;

/**
 * Handle player input including mouse movement for camera control
 * @param player Pointer to the player object
 */
void handle_input(player_t *player) {
  SDL_Event event;
  const Uint8* keystates = SDL_GetKeyboardState(NULL);
  
  // Variables for mouse movement
  int mouse_x_rel = 0;
  int mouse_y_rel = 0;
  
  // Center position for relative mouse mode
  int window_width, window_height;
  SDL_GetWindowSize(window, &window_width, &window_height); // Assuming 'window' is accessible
  
  // Process SDL events
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      running = false;
    }
    else if (event.type == SDL_MOUSEMOTION) {
      // Get relative mouse movement
      mouse_x_rel = event.motion.xrel;
      mouse_y_rel = event.motion.yrel;
    }
    else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        // Toggle mouse capture with Escape key
        if (SDL_GetRelativeMouseMode()) {
          SDL_SetRelativeMouseMode(SDL_FALSE);
        } else {
          SDL_SetRelativeMouseMode(SDL_TRUE);
        }
      }
    }
  }
  
  // Apply mouse rotation if relative mode is enabled
  if (SDL_GetRelativeMouseMode()) {
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
  float forward_x = -cos(angle_rad) * MOVEMENT_SPEED;
  float forward_y = sin(angle_rad) * MOVEMENT_SPEED; // Y is flipped in DOOM
  
  // Calculate strafe direction vector (perpendicular to forward)
  float strafe_x = sin(angle_rad) * MOVEMENT_SPEED;
  float strafe_y = cos(angle_rad) * MOVEMENT_SPEED;
  
  // Handle keyboard movement
  if (keystates[SDL_SCANCODE_W] || keystates[SDL_SCANCODE_UP]) {
    // Move forward
    player->x += forward_x;
    player->y += forward_y;
  }
  if (keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN]) {
    // Move backward
    player->x -= forward_x;
    player->y -= forward_y;
  }
  if (keystates[SDL_SCANCODE_D]) {
    // Strafe right
    player->x += strafe_x;
    player->y += strafe_y;
  }
  if (keystates[SDL_SCANCODE_A]) {
    // Strafe left
    player->x -= strafe_x;
    player->y -= strafe_y;
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
    static Uint32 last_toggle = 0;
    Uint32 current_time = SDL_GetTicks();
    
    // Add debounce to prevent multiple toggles
    if (current_time - last_toggle > 250) {
      mode = !mode;
      last_toggle = current_time;
    }
  }
}

