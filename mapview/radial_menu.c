#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <string.h>
#include <math.h>
#include "map.h"
#include "sprites.h"
#include "radial_menu.h"

// Radial menu shader sources
const char* radial_vs_src = "#version 150 core\n"
"in vec2 position;\n"
"in vec2 texcoord;\n"
"out vec2 tex;\n"
"out vec2 pos;\n"
"uniform mat4 projection;\n"
"uniform vec2 center;\n"
"uniform float radius_min;\n"
"uniform float radius_max;\n"
"uniform float angle_min;\n"
"uniform float angle_max;\n"
"void main() {\n"
"  tex = texcoord;\n"
"  // Transform position from [0,1] to polar coordinates\n"
"  float radius = mix(radius_min, radius_max, position.y);\n"
"  float angle = mix(angle_min, angle_max, position.x);\n"
"  // Convert to Cartesian coordinates\n"
"  vec2 offset = vec2(cos(angle), sin(angle)) * radius;\n"
"  pos = offset;\n"
"  gl_Position = projection * vec4(center + offset, 0.0, 1.0);\n"
"}";

const char* radial_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"in vec2 pos;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform float alpha;\n"
"uniform float highlight;\n"
"void main() {\n"
"  outColor = texture(tex0, tex);\n"
"  outColor.a *= alpha;\n"
"  if (highlight > 0.5) {\n"
"    outColor.rgb *= 1.5; // Brighten the texture if highlighted\n"
"  }\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

// Mesh vertices for radial segment (simple grid)
#define RADIAL_MESH_SIZE 16
#define VERTICES_PER_POINT 4 // pos.x, pos.y, tex.x, tex.y
#define TOTAL_VERTICES (RADIAL_MESH_SIZE * RADIAL_MESH_SIZE * VERTICES_PER_POINT)

// Radial menu system state
typedef struct {
  GLuint program;        // Shader program
  GLuint vao;            // Vertex array object
  GLuint vbo;            // Vertex buffer object
  float mesh_data[TOTAL_VERTICES];
  bool initialized;
} radial_menu_t;

static radial_menu_t g_radial_menu = {0};

// Forward declarations
static GLuint compile_radial_shader(GLenum type, const char* src);
static void generate_mesh();

// Initialize the radial menu system
bool init_radial_menu(void) {
  radial_menu_t* menu = &g_radial_menu;
  
  if (menu->initialized) {
    return true; // Already initialized
  }
  
  // Create shader program
  GLuint vertex_shader = compile_radial_shader(GL_VERTEX_SHADER, radial_vs_src);
  GLuint fragment_shader = compile_radial_shader(GL_FRAGMENT_SHADER, radial_fs_src);
  
  menu->program = glCreateProgram();
  glAttachShader(menu->program, vertex_shader);
  glAttachShader(menu->program, fragment_shader);
  glBindAttribLocation(menu->program, 0, "position");
  glBindAttribLocation(menu->program, 1, "texcoord");
  glLinkProgram(menu->program);
  
  // Check program linking status
  GLint status;
  glGetProgramiv(menu->program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    GLint log_length;
    glGetProgramiv(menu->program, GL_INFO_LOG_LENGTH, &log_length);
    char* log = malloc(log_length);
    glGetProgramInfoLog(menu->program, log_length, NULL, log);
    printf("Radial menu shader program linking error: %s\n", log);
    free(log);
    return false;
  }
  
  // Generate the grid mesh for the radial segment
  generate_mesh();
  
  // Create VAO, VBO
  glGenVertexArrays(1, &menu->vao);
  glBindVertexArray(menu->vao);
  
  glGenBuffers(1, &menu->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, menu->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(menu->mesh_data), menu->mesh_data, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  // Clean up shaders
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  menu->initialized = true;
  return true;
}

// Compile a shader
static GLuint compile_radial_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, 0);
  glCompileShader(shader);
  
  // Check for errors
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    char* log = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, log);
    printf("Radial shader compilation error: %s\n", log);
    free(log);
  }
  
  return shader;
}

// Generate a 16x16 grid mesh for the radial segment using triangle strips
static void generate_mesh(void) {
  radial_menu_t* menu = &g_radial_menu;
  float* ptr = menu->mesh_data;
  
  for (int y = 0; y < RADIAL_MESH_SIZE - 1; y++) {
    for (int x = 0; x < RADIAL_MESH_SIZE; x++) {
      // Top vertex
      *ptr++ = (float)x / (RADIAL_MESH_SIZE - 1);       // x
      *ptr++ = (float)y / (RADIAL_MESH_SIZE - 1);       // y
      *ptr++ = (float)x / (RADIAL_MESH_SIZE - 1);       // u
      *ptr++ = (float)y / (RADIAL_MESH_SIZE - 1)*2.0;       // v
      
      // Bottom vertex
      *ptr++ = (float)x / (RADIAL_MESH_SIZE - 1);       // x
      *ptr++ = (float)(y + 1) / (RADIAL_MESH_SIZE - 1); // y
      *ptr++ = (float)x / (RADIAL_MESH_SIZE - 1);       // u
      *ptr++ = (float)(y + 1) / (RADIAL_MESH_SIZE - 1)*2.0; // v
    }
  }
}

// Draw a single radial segment with the specified texture
void draw_radial(GLuint texture, float center_x, float center_y,
                 float radius_min, float radius_max,
                 float angle_min, float angle_max,
                 float alpha, bool highlight) {
  
  radial_menu_t* menu = &g_radial_menu;
  
  if (!menu->initialized) {
    if (!init_radial_menu()) {
      return;
    }
  }
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Disable depth testing for UI elements
  glDisable(GL_DEPTH_TEST);
  
  // Use radial menu shader program
  glUseProgram(menu->program);
  
  float *get_sprite_matrix(void);
  // Set uniforms
  glUniformMatrix4fv(glGetUniformLocation(menu->program, "projection"), 1, GL_FALSE, get_sprite_matrix());
  glUniform2f(glGetUniformLocation(menu->program, "center"), center_x, center_y);
  glUniform1f(glGetUniformLocation(menu->program, "radius_min"), radius_min);
  glUniform1f(glGetUniformLocation(menu->program, "radius_max"), radius_max);
  glUniform1f(glGetUniformLocation(menu->program, "angle_min"), angle_min);
  glUniform1f(glGetUniformLocation(menu->program, "angle_max"), angle_max);
  glUniform1f(glGetUniformLocation(menu->program, "alpha"), alpha);
  glUniform1f(glGetUniformLocation(menu->program, "highlight"), highlight ? 1.0f : 0.0f);
  
  // Bind texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(glGetUniformLocation(menu->program, "tex0"), 0);
  
  // Bind VAO and draw
  glBindVertexArray(menu->vao);
  
  // Draw the mesh as triangle strips
//  for (int y = 0; y < RADIAL_MESH_SIZE - 1; y++) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * RADIAL_MESH_SIZE * RADIAL_MESH_SIZE);
//  }
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

// Draw a complete radial menu with multiple segments
void draw_radial_menu(GLuint* textures, int texture_count, float center_x, float center_y,
                      float inner_radius, float outer_radius, int selected_index) {
  if (texture_count <= 0) return;
  
  const float TWO_PI = 2.0f * M_PI;
  float segment_angle = TWO_PI / texture_count;
  
  // Draw each segment
  for (int i = 0; i < texture_count; i++) {
    float angle_min = i * segment_angle;
    float angle_max = (i + 1) * segment_angle;
    
    // Add a small gap between segments
    angle_min += 0.02f;
    angle_max -= 0.02f;
    
    bool is_highlighted = (i == selected_index);
    
    draw_radial(textures[i], center_x, center_y,
                inner_radius, outer_radius,
                angle_min, angle_max,
                1.0f, is_highlighted);
  }
}

// Calculate which segment is selected based on cursor position
int get_selected_segment(float cursor_x, float cursor_y, float center_x, float center_y, int segment_count) {
  // Calculate vector from center to cursor
  float dx = cursor_x - center_x;
  float dy = cursor_y - center_y;
  
  // Calculate angle (in radians)
  float angle = atan2f(dy, dx);
  
  // Convert to positive angle (0 to 2π)
  if (angle < 0) {
    angle += 2.0f * M_PI;
  }
  
  // Calculate segment index
  float segment_angle = 2.0f * M_PI / segment_count;
  int segment = (int)(angle / segment_angle);
  
  return segment;
}

// Clean up radial menu resources
void cleanup_radial_menu(void) {
  radial_menu_t* menu = &g_radial_menu;
  
  if (menu->initialized) {
    glDeleteProgram(menu->program);
    glDeleteVertexArrays(1, &menu->vao);
    glDeleteBuffers(1, &menu->vbo);
    menu->initialized = false;
  }
}
