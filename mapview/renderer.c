#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"

// Constants for rendering
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FOV 90.0
#define NEAR_Z 0.1
#define FAR_Z 1000.0
#define MOVEMENT_SPEED 10.0
#define ROTATION_SPEED 3.0

//const char* vs_src = "#version 150 core\n"
//"in vec3 pos; in vec2 uv;\nout vec2 tex;\n"
//"uniform mat4 mvp;\n"
//"void main() { tex = uv; gl_Position = mvp * vec4(pos, 1.0); }";

const char* vs_src = "#version 150 core\n"
"in vec2 bary; in vec2 uv;\nout vec2 tex;\n"
"uniform mat4 mvp;\n"
"uniform mat4 rect;\n"
"uniform vec2 texScale;\n"
"uniform vec2 texOffset;\n"
"void main() {\n"
"  tex = uv*texScale+texOffset;\n"
"  vec4 a = rect[0];\n"
"  vec4 b = rect[1];\n"
"  vec4 c = rect[2];\n"
"  vec4 d = rect[3];\n"
"  vec4 pos = mix(mix(a, b, bary.x), mix(d, c, bary.x), bary.y);\n"
"  gl_Position = mvp * pos;\n"
"}";

const char* fs_src = "#version 150 core\n"
"in vec2 tex;\nout vec4 outColor;\n"
"uniform vec3 color;\n"
"uniform sampler2D tex0;\n"
"void main() { outColor = texture(tex0, tex) * vec4(color + smoothstep(1.0,0.5,gl_FragCoord.z), 1.0); }";

float verts[] = {
  // bary.x bary.y    uv.x uv.y
  0.0f, 0.0f,        0.0f, 1.0f, // bottom left
  1.0f, 0.0f,        1.0f, 1.0f, // bottom right
  1.0f, 1.0f,        1.0f, 0.0f, // top right
  0.0f, 1.0f,        0.0f, 0.0f, // top left
};

unsigned int idx[] = { 0, 1, 2, 2, 3, 0 };

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
  float height;
} player_t;

// Global variables
GLuint prog, vao;
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
  
  GLuint vbo, ebo, tex;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
  
  GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
  prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glBindAttribLocation(prog, 0, "bary");
  glBindAttribLocation(prog, 1, "uv");
  glLinkProgram(prog);
  glUseProgram(prog);

//  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  // 1x1 white texture
  unsigned char pix[] = { 255, 255, 255, 255 };
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
    
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

mapside_texture_t *get_texture(const char* name);

float get_wall_direction_shading(vec2s v1, vec2s v2) {
  vec2s dir = glms_vec2_normalize(glms_vec2_sub(v2, v1));
  float shading = fabsf(dir.x); // Y contributes more as it becomes vertical
  return shading; // 0 = horizontal, 1 = vertical
}

// Helper function to draw a textured quad
void draw_textured_quad(const mapvertex_t *v1, const mapvertex_t *v2,
                        float bottom, float top, mapside_texture_t *texture,
                        float u_offset, float v_offset, float light, bool flip) {
  if (!texture)
    return;
  
  mat4 rect;
  
  if (!flip) {
    // Normal orientation (front side)
    rect[0][0] = v1->x; rect[0][1] = v1->y; rect[0][2] = bottom; rect[0][3] = 1.0f; // a (bottom left)
    rect[1][0] = v2->x; rect[1][1] = v2->y; rect[1][2] = bottom; rect[1][3] = 1.0f; // b (bottom right)
    rect[2][0] = v2->x; rect[2][1] = v2->y; rect[2][2] = top;    rect[2][3] = 1.0f; // c (top right)
    rect[3][0] = v1->x; rect[3][1] = v1->y; rect[3][2] = top;    rect[3][3] = 1.0f; // d (top left)
  } else {
    // Flipped orientation (back side)
    rect[0][0] = v2->x; rect[0][1] = v2->y; rect[0][2] = bottom; rect[0][3] = 1.0f; // a (bottom left)
    rect[1][0] = v1->x; rect[1][1] = v1->y; rect[1][2] = bottom; rect[1][3] = 1.0f; // b (bottom right)
    rect[2][0] = v1->x; rect[2][1] = v1->y; rect[2][2] = top;    rect[2][3] = 1.0f; // c (top right)
    rect[3][0] = v2->x; rect[3][1] = v2->y; rect[3][2] = top;    rect[3][3] = 1.0f; // d (top left)
  }
  
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
  
  vec2s a= {v1->x,v1->y}, b = {v2->x,v2->y};
  
  float shade = get_wall_direction_shading(a, b) * 0.4 + 0.6;
  float dist = glms_vec2_distance(a, b);
  
  light *= shade;
  
  // Apply texture offsets
  glUniform2f(glGetUniformLocation(prog, "texScale"), dist/texture->width, fabs(top-bottom)/texture->height);
  glUniform2f(glGetUniformLocation(prog, "texOffset"), u_offset, v_offset);
  
  glUniform3f(glGetUniformLocation(prog, "color"), light * BRIGHTNESS, light * BRIGHTNESS, light * BRIGHTNESS);
  
  glUniformMatrix4fv(glGetUniformLocation(prog, "rect"), 1, GL_FALSE, (const float*)rect);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Helper function to draw a sidedef
void draw_sidedef(const mapvertex_t *v1, const mapvertex_t *v2,
                  const mapsidedef_t *sidedef, const mapsector_t *sector,
                  const mapsector_t *other_sector, bool is_back_side) {
  float tex_scale = 64.0f; // Convert to normalized coordinates
  float u_offset = sidedef->textureoffset / tex_scale;
  float v_offset = sidedef->rowoffset / tex_scale;
  
  if (other_sector) {
    // This is a two-sided wall
    
    // Draw upper texture (if ceiling heights differ)
    if (sector->ceilingheight > other_sector->ceilingheight) {
      draw_textured_quad(v1, v2,
                         other_sector->ceilingheight, sector->ceilingheight,
                         get_texture(sidedef->toptexture), u_offset, v_offset, sector->lightlevel/255.0f, is_back_side);
    }
    
    // Draw lower texture (if floor heights differ)
    if (sector->floorheight < other_sector->floorheight) {
      draw_textured_quad(v1, v2,
                         sector->floorheight, other_sector->floorheight,
                         get_texture(sidedef->bottomtexture), u_offset, v_offset, sector->lightlevel/255.0f, is_back_side);
    }
    
    // Draw middle texture (if specified) - important for door frames, windows, etc.
    mapside_texture_t *mid_tex = get_texture(sidedef->midtexture);
    if (mid_tex) {
      float bottom = MAX(sector->floorheight, other_sector->floorheight);
      float top = MIN(sector->ceilingheight, other_sector->ceilingheight);
      draw_textured_quad(v1, v2, bottom, top, mid_tex, u_offset, v_offset,
                         sector->lightlevel/255.0f, is_back_side);
    }
  } else {
    // This is a one-sided wall, just draw the middle texture
    draw_textured_quad(v1, v2,
                       sector->floorheight, sector->ceilingheight,
                       get_texture(sidedef->midtexture), u_offset, v_offset, sector->lightlevel/255.0f, is_back_side);
  }
}

// Draw the map from player perspective
void draw_map(map_data_t const *map, mat4 mvp) {
  glUseProgram(prog);
  glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  glBindVertexArray(vao);
  
  // Draw linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    
    // Check if front sidedef exists
    if (linedef->sidenum[0] >= map->num_sidedefs) {
      continue; // Skip if invalid front sidedef
    }
    
    mapsidedef_t const *front_sidedef = &map->sidedefs[linedef->sidenum[0]];
    mapsector_t const *front_sector = &map->sectors[front_sidedef->sector];
    
    // Check if there's a back side to this linedef
    mapsidedef_t const *back_sidedef = NULL;
    mapsector_t const *back_sector = NULL;
    
    if (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs) {
      back_sidedef = &map->sidedefs[linedef->sidenum[1]];
      back_sector = &map->sectors[back_sidedef->sector];
    }
    
    // Draw front side
    draw_sidedef(v1, v2, front_sidedef, front_sector, back_sector, false);
    
    // Draw back side if it exists
    if (back_sidedef) {
      draw_sidedef(v2, v1, back_sidedef, back_sector, front_sector, true);
    }
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}

void get_view_matrix(map_data_t const *map, player_t const *player, mat4 out) {
  mapsector_t const *sector = find_player_sector(map, player->x, player->y);
  
  float z = 0;
  
  if (sector) {
    z = sector->floorheight + EYE_HEIGHT;
  }
  
  // Convert angle to radians for direction calculation
  float angle_rad = player->angle * M_PI / 180.0f + 0.001f;
  
  // Calculate where the player is looking
  float look_x = player->x + cos(angle_rad) * 10.0f;
  float look_y = player->y - sin(angle_rad) * 10.0f; // Y is flipped in DOOM
  
  mat4 proj;
  glm_perspective(glm_rad(90.0f), 4.0f / 3.0f, 10, 2000.0f, proj);
  
  mat4 view;
  vec3 eye = {player->x, player->y, z};
  vec3 center = {look_x, look_y, z}; // Look straight ahead, not at origin
  vec3 up = {0.0f, 0.0f, 1.0f};      // Z is up in Doom
  glm_lookat(eye, center, up, view);
  
  // Combine view and projection into MVP
  glm_mat4_mul(proj, view, out);
}

/**
 * Process input and update player position with proper angle handling
 */
void handle_input(player_t *player) {
  SDL_Event event;
  const Uint8* keystates = SDL_GetKeyboardState(NULL);
  
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      running = false;
    }
  }
  
  // Convert player angle to radians for movement calculations
  float angle_rad = player->angle * M_PI / 180.0;
  
  // Calculate forward/backward direction vector
  float forward_x = cos(angle_rad) * MOVEMENT_SPEED;
  float forward_y = -sin(angle_rad) * MOVEMENT_SPEED; // Y is flipped in DOOM
  
  // Calculate strafe direction vector (perpendicular to forward)
  float strafe_x = -sin(angle_rad) * MOVEMENT_SPEED;
  float strafe_y = -cos(angle_rad) * MOVEMENT_SPEED;
  
  // Handle movement keys
  if (keystates[SDL_SCANCODE_UP]) {
    // Move forward
    player->x += forward_x;
    player->y += forward_y;
  }
  if (keystates[SDL_SCANCODE_DOWN]) {
    // Move backward
    player->x -= forward_x;
    player->y -= forward_y;
  }
  if (keystates[SDL_SCANCODE_D]) {
    // Strafe left
    player->x += strafe_x;
    player->y += strafe_y;
  }
  if (keystates[SDL_SCANCODE_A]) {
    // Strafe right
    player->x -= strafe_x;
    player->y -= strafe_y;
  }
  
  // Handle rotation keys
  if (keystates[SDL_SCANCODE_LEFT]) {
    player->angle -= ROTATION_SPEED;
    if (player->angle < 0) player->angle += 360;
  }
  if (keystates[SDL_SCANCODE_RIGHT]) {
    player->angle += ROTATION_SPEED;
    if (player->angle >= 360) player->angle -= 360;
  }
  if (keystates[SDL_SCANCODE_LSHIFT]) {
    mode = !mode;
  }
}

void draw_floors(map_data_t const *map, mat4 mvp);

// Main function
int run(map_data_t const *map) {
  player_t player;
  
  // Initialize player position based on map data
  init_player(map, &player);
  
  // Main game loop
  Uint32 last_time = SDL_GetTicks();
  
  while (running) {
    // Calculate delta time for smooth movement
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - last_time) / 1000.0f;
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
    
    // Draw the map
    draw_map(map, mvp);

    SDL_GL_SwapWindow(window);
    
    
    // Cap frame rate
    SDL_Delay(16);  // ~60 FPS
  }
  
  // Clean up
  cleanup();
  
  return 0;
}
