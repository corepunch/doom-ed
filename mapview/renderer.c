#include <SDL2/SDL.h>
#include <mapview/gl_compat.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <math.h>

#include <mapview/map.h>
#include <mapview/sprites.h>
#include <mapview/console.h>
#include <editor/editor.h>
#include <ui/kernel/joystick.h>

// External references to window and context (defined in ui/kernel)
extern SDL_Window* window;
extern SDL_GLContext ctx;

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
GLuint white_tex, black_tex, selection_tex, no_tex;

int world_prog_mvp;
int world_prog_viewPos;
int world_prog_tex0_size;
int world_prog_tex0;
int world_prog_light;

int ui_prog_mvp;
int ui_prog_tex0_size;
int ui_prog_tex0;
int ui_prog_color;

bool mode = false;
unsigned frame = 0;

palette_entry_t *palette;

editor_state_t *get_editor(void) {
  return g_game ? &g_game->state : NULL;
}

// Initialize SDL and create window/renderer
bool init_resources(void) {  
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

  return true;
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
