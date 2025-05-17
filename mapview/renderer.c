#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"
#include "sprites.h"
#include "console.h"
#include "editor.h"

#define WALLPAPER_SIZE 64

const char* vs_src = "#version 150 core\n"
"in vec3 pos;\n"
"in vec2 uv;\n"
"in vec3 norm;\n"
"in vec4 color;\n"
"out vec2 tex;\n"
"out vec3 normal;\n"
"out vec3 fragPos;\n"
"out vec4 col;\n"
"uniform vec2 tex0_size;\n"
"uniform mat4 mvp;\n"
"void main() {\n"
"  tex = uv / tex0_size;\n"
"  normal = norm;\n"
"  col = vec4(1)-color;\n"
"  fragPos = pos;\n"
"  gl_Position = mvp * vec4(pos, 1.0);\n"
"}";

const char* fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec3 normal;\n"
"in vec3 fragPos;\n"
"out vec4 outColor;\n"
"uniform float light;\n"
"uniform vec3 viewPos;\n"
"uniform sampler2D tex0;\n"
"uniform mat4 mvp;\n"
"void main() {\n"
"  // Extract view position from inverse MVP matrix\n"
"  vec3 viewDir = normalize(viewPos - fragPos);\n"
"  float distance = smoothstep(500,0,distance(viewPos, fragPos));\n"
"  float facingFactor = abs(dot(normalize(normal), viewDir));\n"
"  float fading = mix(distance * light, 1.0, light * light);\n"
"  if (viewDir.z < -10000) { outColor = texture(tex0, tex) * light; return; }"
"  outColor = texture(tex0, tex) * mix(facingFactor, 1.0, 0.5) * fading * 1.5;\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

const char* fs_unlit_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec4 col;\n"
"out vec4 outColor;\n"
"uniform vec4 color;\n"
"uniform sampler2D tex0;\n"
"void main() {\n"
"  outColor = texture(tex0, tex) * color * col;\n"
"}";

GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  return s;
}

GLuint make_1bit_tex(void *data, int width, int height);

// Global variables
GLuint world_prog, ui_prog;
GLuint white_tex, black_tex, selection_tex, no_tex, wallpaper_tex;

int world_prog_mvp;
int world_prog_viewPos;
int world_prog_tex0_size;
int world_prog_tex0;
int world_prog_light;

int ui_prog_mvp;
int ui_prog_tex0_size;
int ui_prog_tex0;
int ui_prog_color;

editor_state_t editor = {0};
SDL_Window* window = NULL;
SDL_GLContext ctx;
SDL_Joystick* joystick = NULL;
bool running = true;
bool mode = false;
unsigned frame = 0;

palette_entry_t *palette;

void init_floor_shader(void);
void init_sky_geometry(void);
bool init_radial_menu(void);

unsigned char* generate_xor_pattern(void) {
  unsigned char* data = malloc(WALLPAPER_SIZE * WALLPAPER_SIZE);
  for (int y = 0; y < WALLPAPER_SIZE; ++y) {
    for (int x = 0; x < WALLPAPER_SIZE; ++x) {
      data[y * WALLPAPER_SIZE + x] = (x ^ y) & 0xFF;  // 0–255 pattern
    }
  }
  return data;
}

// Initialize SDL and create window/renderer
bool init_sdl(void) {
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  window = SDL_CreateWindow("DOOM Wireframe Renderer",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_OPENGL|SDL_WINDOW_INPUT_FOCUS);
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  // Open the first available controller
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    joystick = SDL_JoystickOpen(i);
    if (joystick) {
      SDL_JoystickEventState(SDL_ENABLE);
      printf("Opened joystick: %s\n", SDL_JoystickName(joystick));
    } else {
      printf("Could not open joystick: %s\n", SDL_GetError());
    }
  }
  
  if (!joystick) {
    printf("No game controller found\n");
  }
  
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  ctx = SDL_GL_CreateContext(window);
  
  GLuint vs, fs;

  vs = compile(GL_VERTEX_SHADER, vs_src);
  fs = compile(GL_FRAGMENT_SHADER, fs_src);
  world_prog = glCreateProgram();
  glAttachShader(world_prog, vs);
  glAttachShader(world_prog, fs);
  glBindAttribLocation(world_prog, 0, "pos");
  glBindAttribLocation(world_prog, 1, "uv");
  glBindAttribLocation(world_prog, 2, "norm");
  glBindAttribLocation(world_prog, 3, "color");
  glLinkProgram(world_prog);
  glUseProgram(world_prog);
  glUniform1i(world_prog_tex0, 0);
  glDeleteShader(vs);
  glDeleteShader(fs);
  
  world_prog_mvp = glGetUniformLocation(world_prog, "mvp");
  world_prog_viewPos = glGetUniformLocation(world_prog, "viewPos");
  world_prog_tex0_size = glGetUniformLocation(world_prog, "tex0_size");
  world_prog_tex0 = glGetUniformLocation(world_prog, "tex0");
  world_prog_light = glGetUniformLocation(world_prog, "light");

  vs = compile(GL_VERTEX_SHADER, vs_src);
  fs = compile(GL_FRAGMENT_SHADER, fs_unlit_src);
  ui_prog = glCreateProgram();
  glAttachShader(ui_prog, vs);
  glAttachShader(ui_prog, fs);
  glBindAttribLocation(ui_prog, 0, "pos");
  glBindAttribLocation(ui_prog, 1, "uv");
  glBindAttribLocation(ui_prog, 2, "norm");
  glBindAttribLocation(ui_prog, 3, "color");
  glLinkProgram(ui_prog);
  glUseProgram(ui_prog);
  glUniform1i(ui_prog_tex0, 0);
  glDeleteShader(vs);
  glDeleteShader(fs);
  
  ui_prog_mvp = glGetUniformLocation(ui_prog, "mvp");
  ui_prog_tex0_size = glGetUniformLocation(ui_prog, "tex0_size");
  ui_prog_tex0 = glGetUniformLocation(ui_prog, "tex0");
  ui_prog_color = glGetUniformLocation(ui_prog, "color");


  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  
  // 1x1 white texture
  glGenTextures(1, &white_tex);
  glBindTexture(GL_TEXTURE_2D, white_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(int){-1});

  glGenTextures(1, &black_tex);
  glBindTexture(GL_TEXTURE_2D, black_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(int){0xff000000});
  
  glGenTextures(1, &selection_tex);
  glBindTexture(GL_TEXTURE_2D, selection_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(int){0xff00ffff});
  
  glGenTextures(1, &no_tex);
  glBindTexture(GL_TEXTURE_2D, no_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &(int){0xffffff00});

  unsigned char* texdata = generate_xor_pattern();
  
  glGenTextures(1, &wallpaper_tex);
  glBindTexture(GL_TEXTURE_2D, wallpaper_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WALLPAPER_SIZE, WALLPAPER_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, texdata);
  
  // Swizzle red to all channels
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
  
  // Filtering + wrap
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  
  free(texdata);

  init_floor_shader();
    
  init_editor(&editor);
  
  init_things();
  
  init_sky_geometry();
  
  init_radial_menu();
  
  return true;
}

// Clean up SDL resources
void cleanup(void) {
  if (joystick) {
    SDL_JoystickClose(joystick);
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void draw_wallpaper(void) {
  void set_projection(int x, int y, int w, int h);
  
  glDepthMask(GL_FALSE);

  mat4 projection;
  wall_vertex_t verts[4] = {
    { screen_width, 0, 0, screen_width, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, 0, 0, -1 },
    { 0, screen_height, 0, 0, screen_height, 0, 0, 0, -1 },
    { screen_width, screen_height, 0, screen_width, screen_height, 0, 0, 0, -1 },
  };
  glm_ortho(0, screen_width, screen_height, 0, -1, 1, projection);

  glUseProgram(ui_prog);
  glBindTexture(GL_TEXTURE_2D, wallpaper_tex);
  glUniform1i(ui_prog_tex0, 0);
  glUniform2f(ui_prog_tex0_size, WALLPAPER_SIZE, WALLPAPER_SIZE);
  glUniform4f(ui_prog_color, 1, 1, 1, 1);
  glUniformMatrix4fv(ui_prog_mvp, 1, GL_FALSE, projection[0]);
  glBindVertexArray(editor.vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor.vbo);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glDisableVertexAttribArray(3);
  glVertexAttrib4f(3, 0, 0, 0, 0);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Main function
int run(void) {
  // Main game loop
  Uint32 last_time = SDL_GetTicks();

  draw_wallpaper();
  
  while (running) {
    mode = false;
    // Calculate delta time for smooth movement
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - last_time) / 1000.0f;
    last_time = current_time;

//    glClearColor(0.825f, 0.590f, 0.425f, 1.0f);
//    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    void game_tick(float);
    void draw_intermission(void);
    void handle_intermission_input(float delta_time);

    switch (game.state) {
      case GS_DUNGEON:
        game_tick(delta_time);
        handle_windows();
//        draw_windows(!SDL_GetRelativeMouseMode());
        break;
      case GS_EDITOR:
        handle_editor_input(&game.map, &editor, &game.player, delta_time);
        draw_editor(&game.map, &editor, &game.player);
        break;
      case GS_WORLD:
        handle_intermission_input(delta_time);
        draw_intermission();
        break;
      default:
        break;
    }

//    SDL_GL_SwapWindow(window);
    
    glFlush();
    
    // Cap frame rate
//    SDL_Delay(16);  // ~60 FPS
  }
  
  // Clean up
  cleanup();
  
  return 0;
}

void GetMouseInVirtualCoords(int* vx, int* vy) {
  extern SDL_Window* window;
  
//  const int target_width = DOOM_WIDTH;
//  const int target_height = DOOM_HEIGHT;
  
//  int win_width, win_height;
  int mouse_x, mouse_y;
  
//  SDL_GetWindowSize(window, &win_width, &win_height);
  SDL_GetMouseState(&mouse_x, &mouse_y);
  
  // Calculate uniform scale (based on height)
//  float scale = (float)win_height / target_height;
//  float render_width = target_width * scale;
//  float offset_x = (win_width - render_width) / 2.0f;
  
  // Convert real mouse coordinates to virtual
  float virtual_x = mouse_x/2;//(mouse_x - offset_x) / scale;
  float virtual_y = mouse_y/2;// / scale;
  
  // Return as integers
  *vx = (int)virtual_x;
  *vy = (int)virtual_y;
}
