#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"
#include "sprites.h"

const char* vs_src = "#version 150 core\n"
"in vec3 pos;\n"
"in vec2 uv;\n"
"out vec2 tex;\n"
"out vec4 vpos;\n"
"uniform vec2 tex0_size;\n"
"uniform mat4 mvp;\n"
"void main() {\n"
"  tex = uv / tex0_size;\n"
"  vpos = mvp * vec4(pos, 1.0);\n"
"  gl_Position = vpos;\n"
"}";

const char* fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec4 vpos;\n"
"out vec4 outColor;\n"
"uniform vec4 color;\n"
"uniform sampler2D tex0;\n"
"void main() {\n"
"  outColor = texture(tex0, tex) * color;\n"
"}";

GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  return s;
}

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
  unsigned char pix[] = { 255, 255, 255, 255 };
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

void draw_floors(map_data_t const *map, mat4 mvp);
void draw_walls(map_data_t const *map, mat4 mvp);

int pixel = 0;

// Main function
int run(map_data_t const *map) {
  player_t player = {0};
  
  // Initialize player position based on map data
  init_player(map, &player);
  
  // Main game loop
  Uint32 last_time = SDL_GetTicks();
  mat4 mvp;

  while (running) {
    // Calculate delta time for smooth movement
    Uint32 current_time = SDL_GetTicks();
//    float delta_time = (current_time - last_time) / 1000.0f;
    last_time = current_time;
    
    // Handle input
    handle_input((map_data_t *)map, &player);

    get_view_matrix(map, &player, mvp);

    SDL_Event e;
    while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) return 0;
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, mode ? GL_LINE : GL_FILL);
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, (const float*)mvp);

    void draw_wall_ids(map_data_t const *map, mat4 mvp);
    void draw_floor_ids(map_data_t const *map, mat4 mvp);

    draw_floor_ids(map, mvp);
    draw_wall_ids(map, mvp);

    int fb_width, fb_height;
    SDL_GL_GetDrawableSize(window, &fb_width, &fb_height);
    glReadPixels(fb_width / 2, fb_height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

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
