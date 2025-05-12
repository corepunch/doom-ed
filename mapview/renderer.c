#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"
#include "sprites.h"
#include "console.h"
#include "editor.h"

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
"  outColor = texture(tex0, tex) * color/* * col*/;\n"
"}";

GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  return s;
}

// Global variables
GLuint world_prog, ui_prog;
GLuint white_tex, black_tex, selection_tex, no_tex;

editor_state_t editor = {0};
SDL_Window* window = NULL;
SDL_GLContext ctx;
SDL_Joystick* joystick = NULL;
bool running = true;
bool mode = false;

palette_entry_t *palette;

void init_floor_shader(void);
void init_sky_geometry(void);
bool init_radial_menu(void);

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
  glUniform1i(glGetUniformLocation(world_prog, "tex0"), 0);
  glDeleteShader(vs);
  glDeleteShader(fs);

  
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
  glUniform1i(glGetUniformLocation(ui_prog, "tex0"), 0);
  glDeleteShader(vs);
  glDeleteShader(fs);

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

// Main function
int run(void) {
  
  // Main game loop
  Uint32 last_time = SDL_GetTicks();
  
  while (running) {
    mode = false;
    // Calculate delta time for smooth movement
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - last_time) / 1000.0f;
    last_time = current_time;

    glClearColor(0.825f, 0.590f, 0.425f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
//    glPolygonMode(GL_FRONT_AND_BACK, mode ? GL_LINE : GL_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    void game_tick(float);
    void draw_intermission(void);
    void handle_intermission_input(float delta_time);

    switch (game.state) {
      case GS_DUNGEON:
        game_tick(delta_time);
        handle_windows();
        draw_windows(!SDL_GetRelativeMouseMode());
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

    SDL_GL_SwapWindow(window);
    
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
