#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "editor.h"
#include "sprites.h"

#define WALLPAPER_SIZE 64

extern GLuint ui_prog;
extern editor_state_t editor;
GLuint wallpaper_tex;

unsigned char* generate_xor_pattern(void) {
  unsigned char* data = malloc(WALLPAPER_SIZE * WALLPAPER_SIZE);
  for (int y = 0; y < WALLPAPER_SIZE; ++y) {
    for (int x = 0; x < WALLPAPER_SIZE; ++x) {
      data[y * WALLPAPER_SIZE + x] = (x ^ y) & 0xFF;  // 0–255 pattern
    }
  }
  return data;
}

void create_desktop_tex(void) {
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
  
//  draw_rect(17, 0, 0, 256, 256);
}

result_t win_desktop(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      create_desktop_tex();
      return true;
    case WM_PAINT:
      draw_wallpaper();
      return true;
  }
  return false;
}

