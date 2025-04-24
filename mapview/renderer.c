#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"
#include "sprites.h"

// Constants for rendering
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FOV 90.0
#define NEAR_Z 0.1
#define FAR_Z 1000.0
#define MOVEMENT_SPEED 10.0
#define ROTATION_SPEED 3.0

const char* vs_src = "#version 150 core\n"
"in vec3 pos;\n"
"in vec2 uv;\n"
"out vec2 tex;\n"
"uniform vec2 tex0_size;\n"
"uniform mat4 mvp;\n"
"void main() {\n"
"  tex = uv / tex0_size;\n"
"  gl_Position = mvp * vec4(pos, 1.0);\n"
"}";

const char* fs_src = "#version 150 core\n"
"in vec2 tex;\nout vec4 outColor;\n"
"uniform vec3 color;\n"
"uniform sampler2D tex0;\n"
"void main() {\n"
"  outColor = texture(tex0, tex);\n"
"  outColor *= vec4(color + smoothstep(1.0,0.5,gl_FragCoord.z), 1.0) * outColor.w;\n"
"}";

GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  return s;
}

// Player state
typedef struct {
  float x;
  float y;
  float angle;  // in degrees, 0 is east, 90 is north
  float pitch;
  float height;
} player_t;

// Global variables
GLuint prog;
GLuint error_tex;

SDL_Window* window = NULL;
SDL_GLContext ctx;
bool running = true;
bool mode = false;

void init_floor_shader(void);

// Initialize SDL and create window/renderer
bool init_sdl(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  window = SDL_CreateWindow("DOOM Wireframe Renderer",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_OPENGL);
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  ctx = SDL_GL_CreateContext(window);
    
  GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
  prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glBindAttribLocation(prog, 0, "pos");
  glBindAttribLocation(prog, 1, "uv");
  glLinkProgram(prog);
  glUseProgram(prog);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);

//  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  
  // 1x1 white texture
  unsigned char pix[] = { 255, 0, 0, 255 };
  glGenTextures(1, &error_tex);
  glBindTexture(GL_TEXTURE_2D, error_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    
  init_floor_shader();
    
  return true;
}

// Clean up SDL resources
void cleanup(void) {
  SDL_DestroyWindow(window);
  SDL_Quit();
}

// Initialize player position based on map data
void init_player(map_data_t const *map, player_t *player) {
  // Default values
  player->x = 0;
  player->y = 0;
  player->angle = 0;
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
}

/**
 * Generate the view matrix incorporating player position, angle, and pitch
 * @param map Pointer to the map data
 * @param player Pointer to the player object
 * @param out Output matrix (view-projection combined)
 */
void get_view_matrix(map_data_t const *map, player_t const *player, mat4 out) {
  mapsector_t const *sector = find_player_sector(map, player->x, player->y);
  
  float z = 0;
  
  if (sector) {
    z = sector->floorheight + EYE_HEIGHT;
  }
  
  // Convert angle to radians for direction calculation
  float angle_rad = player->angle * M_PI / 180.0f + 0.001f;
  float pitch_rad = player->pitch * M_PI / 180.0f;
  
  // Calculate looking direction vector
  float look_dir_x = -cos(angle_rad);
  float look_dir_y = sin(angle_rad);
  float look_dir_z = sin(pitch_rad);
  
  // Scale the horizontal component of the look direction by the cosine of the pitch
  // This prevents the player from moving faster when looking up or down
  float cos_pitch = cos(pitch_rad);
  look_dir_x *= cos_pitch;
  look_dir_y *= cos_pitch;
  
  // Calculate look-at point
  float look_x = player->x + look_dir_x * 10.0f;
  float look_y = player->y + look_dir_y * 10.0f;
  float look_z = z + look_dir_z * 10.0f;
  
  // Create projection matrix
  mat4 proj;
  glm_perspective(glm_rad(90.0f), 4.0f / 3.0f, 1.0f, 2000.0f, proj);
  
  // Create view matrix
  mat4 view;
  vec3 eye = {player->x, player->y, z};
  vec3 center = {look_x, look_y, look_z}; // Look vector now includes pitch
  vec3 up = {0.0f, 0.0f, 1.0f};           // Z is up in Doom
  
  // Add a small offset to prevent precision issues with axis-aligned views
  if (fabs(player->pitch) > 89.5f) {
    // Adjust up vector when looking almost straight up or down
    up[0] = -sin(angle_rad);
    up[1] = -cos(angle_rad);
    up[2] = 0.0f;
  }
  
  glm_lookat(eye, center, up, view);
  
  // Combine view and projection into MVP
  glm_mat4_mul(proj, view, out);
}

/**
 * Process input and update player position with proper angle handling
 */
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

void draw_floors(map_data_t const *map, mat4 mvp);
void draw_walls(map_data_t const *map, mat4 mvp);

// Main function
int run(map_data_t const *map) {
  player_t player = {0};
  
  // Initialize player position based on map data
  init_player(map, &player);
  
  // Main game loop
  Uint32 last_time = SDL_GetTicks();
  
  while (running) {
    // Calculate delta time for smooth movement
    Uint32 current_time = SDL_GetTicks();
//    float delta_time = (current_time - last_time) / 1000.0f;
    last_time = current_time;
    
    // Handle input
    handle_input(&player);
    
    SDL_Event e;
    while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) return 0;
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, mode ? GL_LINE : GL_FILL);

    // Combine view and projection into MVP
    mat4 mvp;
    get_view_matrix(map, &player, mvp);

    draw_floors(map, mvp);
    
    draw_walls(map, mvp);
    
    draw_weapon();

    draw_crosshair();

    SDL_GL_SwapWindow(window);
    
    // Cap frame rate
    SDL_Delay(16);  // ~60 FPS
  }
  
  // Clean up
  cleanup();
  
  return 0;
}
