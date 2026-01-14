// Drawing primitives implementation
// Extracted from mapview/window.c

#include <SDL2/SDL.h>
#include "gl_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "user.h"
#include "messages.h"
#include "draw.h"

// External references
extern window_t *windows;
extern window_t *_focused;
extern int screen_width, screen_height;
extern SDL_Window *window;

// Forward declarations
extern void draw_text_small(const char* text, int x, int y, uint32_t col);
extern int send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// UI drawing system state
typedef struct {
  GLuint program;        // Shader program for drawing rectangles
  GLuint vao;            // Vertex array object
  GLuint vbo;            // Vertex buffer object
  float projection[16];  // Orthographic projection matrix
} ui_draw_system_t;

static ui_draw_system_t ui_draw_system = {0};

// Internal white texture for drawing solid colors
static GLuint ui_white_texture = 0;

// UI shader sources (same as sprite shader but for UI framework)
static const char* ui_vs_src = "#version 150 core\n"
"in vec2 position;\n"
"in vec2 texcoord;\n"
"in vec4 color;\n"
"out vec2 tex;\n"
"out vec4 col;\n"
"uniform mat4 projection;\n"
"uniform vec2 offset;\n"
"uniform vec2 scale;\n"
"void main() {\n"
"  col = color;\n"
"  tex = texcoord;\n"
"  gl_Position = projection * vec4(position * scale + offset, 0.0, 1.0);\n"
"}";

static const char* ui_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec4 col;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform float alpha;\n"
"void main() {\n"
"  outColor = texture(tex0, tex) * col;\n"
"  outColor.a *= alpha;\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

// Simple vertex structure for UI rectangles
typedef struct {
  int16_t x, y, z;    // Position
  int16_t u, v;       // Texture coordinates
  int8_t nx, ny, nz;  // Normal (unused for 2D)
  int32_t color;      // Color
} ui_vertex_t;

// Rectangle vertices (quad)
static ui_vertex_t rect_verts[] = {
  {0, 0, 0, 0, 0, 0, 0, 0, -1}, // bottom left
  {0, 1, 0, 0, 1, 0, 0, 0, -1}, // top left
  {1, 1, 0, 1, 1, 0, 0, 0, -1}, // top right
  {1, 0, 0, 1, 0, 0, 0, 0, -1}, // bottom right
};

// Compile a shader
static GLuint compile_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  
  // Check compilation status
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compilation failed: %s\n", log);
    return 0;
  }
  
  return shader;
}

// Initialize UI drawing system
static void init_ui_draw_system(void) {
  if (ui_draw_system.program != 0) {
    return; // Already initialized
  }
  
  ui_draw_system_t* sys = &ui_draw_system;
  
  // Create shader program
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, ui_vs_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, ui_fs_src);
  
  sys->program = glCreateProgram();
  glAttachShader(sys->program, vertex_shader);
  glAttachShader(sys->program, fragment_shader);
  glBindAttribLocation(sys->program, 0, "position");
  glBindAttribLocation(sys->program, 1, "texcoord");
  glBindAttribLocation(sys->program, 2, "color");
  glLinkProgram(sys->program);
  
  // Check link status
  GLint success;
  glGetProgramiv(sys->program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(sys->program, sizeof(log), NULL, log);
    fprintf(stderr, "Shader program linking failed: %s\n", log);
  }
  
  // Clean up shaders (no longer needed after linking)
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  // Create VAO and VBO
  glGenVertexArrays(1, &sys->vao);
  glBindVertexArray(sys->vao);
  
  glGenBuffers(1, &sys->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, sys->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(rect_verts), rect_verts, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(ui_vertex_t), (void*)0); // Position
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(ui_vertex_t), (void*)(3 * sizeof(int16_t))); // UV
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ui_vertex_t), (void*)(5 * sizeof(int16_t) + 3 * sizeof(int8_t))); // Color
  
  printf("UI drawing system initialized successfully\n");
}

// Set orthographic projection for UI drawing
void set_projection(int x, int y, int w, int h) {
  // Ensure the UI drawing system is initialized
  if (ui_draw_system.program == 0) {
    init_ui_draw_system();
  }
  
  // Create orthographic projection matrix
  // glm_ortho equivalent: orthographic projection from (x, y) to (w, h)
  float left = x, right = w, bottom = h, top = y;
  float near = -1, far = 1;
  
  float* m = ui_draw_system.projection;
  
  // Column-major order for OpenGL
  m[0] = 2.0f / (right - left);
  m[1] = 0.0f;
  m[2] = 0.0f;
  m[3] = 0.0f;
  
  m[4] = 0.0f;
  m[5] = 2.0f / (top - bottom);
  m[6] = 0.0f;
  m[7] = 0.0f;
  
  m[8] = 0.0f;
  m[9] = 0.0f;
  m[10] = -2.0f / (far - near);
  m[11] = 0.0f;
  
  m[12] = -(right + left) / (right - left);
  m[13] = -(top + bottom) / (top - bottom);
  m[14] = -(far + near) / (far - near);
  m[15] = 1.0f;
  
  // Set the projection matrix in the shader
  glUseProgram(ui_draw_system.program);
  glUniformMatrix4fv(glGetUniformLocation(ui_draw_system.program, "projection"), 1, GL_FALSE, ui_draw_system.projection);
}

// Draw a rectangle with extended parameters
void draw_rect_ex(int tex, int x, int y, int w, int h, int type, float alpha) {
  // Ensure the UI drawing system is initialized
  if (ui_draw_system.program == 0) {
    init_ui_draw_system();
  }
  
  ui_draw_system_t* sys = &ui_draw_system;
  
  // Bind shader program and set uniforms
  glUseProgram(sys->program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(sys->program, "tex0"), 0);
  glUniform2f(glGetUniformLocation(sys->program, "offset"), x, y);
  glUniform2f(glGetUniformLocation(sys->program, "scale"), w, h);
  glUniform1f(glGetUniformLocation(sys->program, "alpha"), alpha);
  
  // Bind VAO and draw
  glBindVertexArray(sys->vao);
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Disable depth testing for UI elements
  glDisable(GL_DEPTH_TEST);
  
  glDrawArrays(type ? GL_LINE_LOOP : GL_TRIANGLE_FAN, 0, 4);
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

// Draw a rectangle at the specified screen position
void draw_rect(int tex, int x, int y, int w, int h) {
  draw_rect_ex(tex, x, y, w, h, false, 1);
}

// Get the UI drawing shader program (for text rendering)
GLuint ui_get_draw_program(void) {
  // Ensure the UI drawing system is initialized
  if (ui_draw_system.program == 0) {
    init_ui_draw_system();
  }
  return ui_draw_system.program;
}

// Set shader uniforms for drawing (used by text rendering)
void ui_set_draw_uniforms(int tex, int x, int y, int w, int h, float alpha) {
  // Ensure the UI drawing system is initialized
  if (ui_draw_system.program == 0) {
    init_ui_draw_system();
  }
  
  ui_draw_system_t* sys = &ui_draw_system;
  
  // Bind shader program and set uniforms
  glUseProgram(sys->program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(sys->program, "tex0"), 0);
  glUniform2f(glGetUniformLocation(sys->program, "offset"), x, y);
  glUniform2f(glGetUniformLocation(sys->program, "scale"), w, h);
  glUniform1f(glGetUniformLocation(sys->program, "alpha"), alpha);
}

// Initialize the internal white texture
static void init_ui_white_texture(void) {
  if (ui_white_texture == 0) {
    glGenTextures(1, &ui_white_texture);
    glBindTexture(GL_TEXTURE_2D, ui_white_texture);
    uint32_t white_pixel = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
}

// Get titlebar height
int titlebar_height(window_t const *win) {
  int t = 0;
  if (!(win->flags&WINDOW_NOTITLE)) {
    t += TITLEBAR_HEIGHT;
  }
  if (win->flags&WINDOW_TOOLBAR) {
    t += TOOLBAR_HEIGHT;
  }
  return t;
}

// Draw focused border
void draw_focused(rect_t const *r) {
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_FOCUSED, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_FOCUSED, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_FOCUSED, r->x, r->y+r->h, r->w+1, 1);
}

// Draw bevel border
void draw_bevel(rect_t const *r) {
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, r->w+2, 1);
  fill_rect(COLOR_LIGHT_EDGE, r->x-1, r->y-1, 1, r->h+2);
  fill_rect(COLOR_DARK_EDGE, r->x+r->w, r->y, 1, r->h+1);
  fill_rect(COLOR_DARK_EDGE, r->x, r->y+r->h, r->w+1, 1);
  fill_rect(COLOR_FLARE, r->x-1, r->y-1, 1, 1);
}

// Draw button
void draw_button(int x, int y, int w, int h, bool pressed) {
  fill_rect(pressed?COLOR_DARK_EDGE:COLOR_LIGHT_EDGE, x-1, y-1, w+2, h+2);
  fill_rect(pressed?COLOR_LIGHT_EDGE:COLOR_DARK_EDGE, x, y, w+1, h+1);
  fill_rect(pressed?COLOR_PANEL_DARK_BG:COLOR_PANEL_BG, x, y, w, h);
  if (pressed) {
    fill_rect(COLOR_FLARE, x+w, y+h, 1, 1);
  } else {
    fill_rect(COLOR_FLARE, x-1, y-1, 1, 1);
  }
}

// Draw window panel
void draw_panel(window_t const *win) {
  int t = titlebar_height(win);
  int x = win->frame.x, y = win->frame.y-t;
  int w = win->frame.w, h = win->frame.h+t;
  bool active = _focused == win;
  if (active) {
    draw_focused(MAKERECT(x, y, w, h));
  } else {
    draw_bevel(MAKERECT(x, y, w, h));
  }
  if (!(win->flags & WINDOW_NORESIZE)) {
    int r = RESIZE_HANDLE;
    fill_rect(COLOR_LIGHT_EDGE, x+w, y+h-r+1, 1, r);
    fill_rect(COLOR_LIGHT_EDGE, x+w-r+1, y+h, r, 1);
  }
  if (!(win->flags&WINDOW_NOFILL)) {
    fill_rect(COLOR_PANEL_BG, x, y, w, h);
  }
}

// Draw window controls (close, minimize, etc.)
void draw_window_controls(window_t *win) {
  int t = titlebar_height(win);
  int x = win->frame.x, y = win->frame.y-t;
  int w = win->frame.w;
  
  // Draw close button
  draw_icon8(icon8_minus, x+w-10, y+3, COLOR_DARK_EDGE);
  draw_icon8(icon8_minus, x+w-11, y+2, COLOR_TEXT_NORMAL);
}

// Set OpenGL viewport for window
void set_viewport(window_t const *win) {
  int w, h;
  SDL_GL_GetDrawableSize(window, &w, &h);
  
  float scale_x = (float)w / screen_width;
  float scale_y = (float)h / screen_height;
  
  int vp_x = (int)(win->frame.x * scale_x);
  int vp_y = (int)((screen_height - win->frame.y - win->frame.h) * scale_y); // flip Y
  int vp_w = (int)(win->frame.w * scale_x);
  int vp_h = (int)(win->frame.h * scale_y);
  
  glEnable(GL_SCISSOR_TEST);
  glViewport(vp_x, vp_y, vp_w, vp_h);
  glScissor(vp_x, vp_y, vp_w, vp_h);
}

// Paint window to stencil buffer
void paint_window_stencil(window_t const *w) {
  int p = 1;
  int t = titlebar_height(w);
  glStencilFunc(GL_ALWAYS, w->id, 0xFF);            // Always pass
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); // Replace stencil with window ID
  draw_rect(1, w->frame.x-p, w->frame.y-t-p, w->frame.w+p*2, w->frame.h+t+p*2);
}

// Repaint window stencil buffer
static void repaint_stencil(void) {
  set_viewport(&(window_t){0, 0, screen_width, screen_height});
  set_projection(0, 0, screen_width, screen_height);
  
  glEnable(GL_STENCIL_TEST);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  for (window_t *w = windows; w; w = w->next) {
    if (!w->visible)
      continue;
    send_message(w, WM_PAINTSTENCIL, 0, NULL);
  }
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

// Draw all windows
void draw_windows(bool rich) {
  repaint_stencil();
  for (window_t *win = windows; win; win = win->next) {
    if (!win->visible)
      continue;
    send_message(win, WM_NCPAINT, 0, NULL);
    send_message(win, WM_PAINT, 0, NULL);
  }
}

// Set stencil test to render for specific window
void ui_set_stencil_for_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Set stencil test to render for root window
void ui_set_stencil_for_root_window(uint32_t window_id) {
  glStencilFunc(GL_EQUAL, window_id, 0xFF);
}

// Begin frame rendering
void ui_begin_frame(void) {
  // Nothing to do here for now - left for future expansion
}

// End frame rendering  
void ui_end_frame(void) {
  // Nothing to do here for now - left for future expansion
}

// Clear screen with color
void ui_clear_screen(float r, float g, float b) {
  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// Swap display buffers
void ui_swap_buffers(void) {
  extern SDL_Window *window;
  SDL_GL_SwapWindow(window);
}

// Fill a rectangle with a solid color
void fill_rect(int color, int x, int y, int w, int h) {
  // Ensure the white texture is initialized
  if (ui_white_texture == 0) {
    init_ui_white_texture();
  }
  
  // Update the white texture with the desired color
  glBindTexture(GL_TEXTURE_2D, ui_white_texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
  
  // Draw a rectangle using the texture
  draw_rect_ex(ui_white_texture, x, y, w, h, false, 1);
}

void draw_icon8(int icon, int x, int y, uint32_t col) {
  char str[2] = { icon+128+6*16, 0 };
  draw_text_small(str, x, y, col);
}

void draw_icon16(int icon, int x, int y, uint32_t col) {
  icon*=2;
  char str[6] = { icon+128, icon+129, '\n', icon+144, icon+145, 0 };
  draw_text_small(str, x, y, col);
}
