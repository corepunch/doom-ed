#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include "map.h"

// Global variables to add
GLuint floor_prog;
GLuint floor_vao, floor_vbo;
GLuint floor_texture;

// Floor/ceiling vertex shader
const char* floor_vs_src = "#version 150 core\n"
"in vec3 position;\n"
"in vec2 texcoord;\n"
"out vec2 tex;\n"
"uniform mat4 mvp;\n"
"void main(void) {\n"
"  tex = texcoord;\n"
"  gl_Position = mvp * vec4(position, 1.0);\n"
"}";

// Floor/ceiling fragment shader
const char* floor_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform vec3 color;\n"
"uniform bool use_texture;\n"
"void main(void) {\n"
"  if (use_texture) {\n"
"    outColor = texture(tex0, tex);\n"
"  } else {\n"
"    outColor = vec4(color, 1.0);\n"
"  }\n"
"}";

#define MAX_VERTICES 1024
#define TEX_SIZE 64.0f

GLuint compile(GLenum type, const char* src);
GLuint get_texture(const char* name);

// Add this to your init_sdl function
void init_floor_shader(void) {
  // Create shader program for floors/ceilings
  GLuint floor_vs = compile(GL_VERTEX_SHADER, floor_vs_src);
  GLuint floor_fs = compile(GL_FRAGMENT_SHADER, floor_fs_src);
  floor_prog = glCreateProgram();
  glAttachShader(floor_prog, floor_vs);
  glAttachShader(floor_prog, floor_fs);
  glBindAttribLocation(floor_prog, 0, "position");
  glBindAttribLocation(floor_prog, 1, "texcoord");
  glLinkProgram(floor_prog);
  
  // Create VAO for floor geometry
  glGenVertexArrays(1, &floor_vao);
  glGenBuffers(1, &floor_vbo);
}

//// Helper function to check if a point is inside a polygon
bool point_in_polygon(float x, float y, vec2s *vertices, int num_vertices) {
  bool inside = false;
  for (int i = 0, j = num_vertices - 1; i < num_vertices; j = i++) {
    if ((vertices[i].y > y) != (vertices[j].y > y) &&
        (x < (vertices[j].x - vertices[i].x) * (y - vertices[i].y) /
         (vertices[j].y - vertices[i].y) + vertices[i].x)) {
      inside = !inside;
    }
  }
  return inside;
}

// Improved triangulation function for DOOM sectors
int triangulate_sector(mapvertex_t *vertices, int num_vertices, float *out_vertices);

// Function to calculate texture coordinates based on sector dimensions
void calculate_texture_coords(float *vertices, int vertex_count, float sector_width, float sector_height) {
  // Find bounds of the sector
  float min_x = FLT_MAX, min_y = FLT_MAX;
  float max_x = -FLT_MAX, max_y = -FLT_MAX;
  
  for (int i = 0; i < vertex_count; i += 5) {
    float x = vertices[i];
    float y = vertices[i + 1];
    
    min_x = MIN(min_x, x);
    min_y = MIN(min_y, y);
    max_x = MAX(max_x, x);
    max_y = MAX(max_y, y);
  }
  
  // Calculate scaling factors for texture coordinates
  float width = max_x - min_x;
  float height = max_y - min_y;
  
  float scale_x = width / sector_width;
  float scale_y = height / sector_height;
  
  // Update texture coordinates
  for (int i = 0; i < vertex_count; i += 5) {
    // Position coordinates
    float x = vertices[i];
    float y = vertices[i + 1];
    
    // Texture coordinates (normalized 0-1)
    vertices[i + 3] = (x - min_x) / width * scale_x;
    vertices[i + 4] = (y - min_y) / height * scale_y;
  }
}

static bool
belongs_to_sector(uint32_t i,
                  const maplinedef_t *linedef,
                  const map_data_t *map)
{
  if (linedef->sidenum[0] != 0xFFFF &&
      map->sidedefs[linedef->sidenum[0]].sector == i)
  {
    return true;
  }
  
  if (linedef->sidenum[1] != 0xFFFF &&
      map->sidedefs[linedef->sidenum[1]].sector == i)
  {
    return true;
  }
  return false;
}

uint32_t find_first_linedef(map_data_t const *map, uint32_t i) {
  for (int j = 0; j < map->num_linedefs; j++) {
    if (belongs_to_sector(i, &map->linedefs[j], map)) {
      return j;
    }
  }
  return -1;
}

uint32_t get_sector_vertices(map_data_t const *map,
                             uint32_t i,
                             mapvertex_t *sector_vertices)
{
  bool used[1024]={0};
  
  uint32_t num_vertices = 0;
  uint32_t first = find_first_linedef(map, i);
  
  if (first > map->num_linedefs)
    return 0;
  
  used[first]=true;

  sector_vertices[num_vertices++] = map->vertices[map->linedefs[first].start];
  sector_vertices[num_vertices++] = map->vertices[map->linedefs[first].end];
  
  int current = map->linedefs[first].end;
  
  while (current != map->linedefs[first].start) {
    bool found = false;
    for (int j = first + 1; j < map->num_linedefs; j++) {
      maplinedef_t const *linedef = &map->linedefs[j];
      if (!belongs_to_sector(i, linedef, map) || used[j])
        continue;
      if (linedef->start == current) {
        sector_vertices[num_vertices++] = map->vertices[linedef->end];
        current = linedef->end;
        used[j] = true;
        found = true;
        break;
      }
      if (linedef->end == current) {
        sector_vertices[num_vertices++] = map->vertices[linedef->start];
        current = linedef->start;
        used[j] = true;
        found = true;
        break;
      }
    }
    if (!found) {
      assert(false);
      return 0;
    }
  }
  return num_vertices;
}

// Main function to draw floors and ceilings
void draw_floors(map_data_t const *map, mat4 mvp) {
  glUseProgram(floor_prog);
  glBindVertexArray(floor_vao);
  
  // Set MVP matrix uniform
  glUniformMatrix4fv(glGetUniformLocation(floor_prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  
  // Process each sector
  for (int i = 0; i < map->num_sectors; i++) {
    // Collect all vertices for this sector
    mapvertex_t sector_vertices[MAX_VERTICES]; // Assuming max MAX_VERTICES vertices per sector
    int num_vertices = get_sector_vertices(map, i, sector_vertices);
    
    // Skip sectors with too few vertices
    if (num_vertices < 3) continue;
    
    // Triangulate the sector
    // Allocate memory for vertices (position + texcoord) * 3 vertices per triangle * num_triangles
    float vertices[5 * (MAX_VERTICES+1)];
    int vertex_count = triangulate_sector(sector_vertices, num_vertices, vertices);
    
    // Set appropriate texture coordinates
    float sector_width = TEX_SIZE; // Default texture size
    float sector_height = TEX_SIZE;
    calculate_texture_coords(vertices, vertex_count, sector_width, sector_height);
    
    // Draw floor
    // Set z height to floor height
    for (int j = 0; j < vertex_count; j += 5) {
      vertices[j + 2] = map->sectors[i].floorheight;
    }
    
    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, floor_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(float), vertices, GL_DYNAMIC_DRAW);
    
    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Bind floor texture
    GLuint floor_tex = get_texture(map->sectors[i].floorpic);
    if (floor_tex != -1) {
      glBindTexture(GL_TEXTURE_2D, floor_tex);
      glUniform1i(glGetUniformLocation(floor_prog, "use_texture"), 1);
    } else {
      // Use flat color based on sector light level
      float light = map->sectors[i].lightlevel / 255.0f;
      glUniform3f(glGetUniformLocation(floor_prog, "color"), light * 0.7f, light * 0.7f, light * 0.7f);
      glUniform1i(glGetUniformLocation(floor_prog, "use_texture"), 0);
    }
    
    // Draw triangles
    glDrawArrays(GL_TRIANGLES, 0, vertex_count/5);
//    glDrawArrays(GL_LINE_STRIP, 0, vertex_count/5);
    
//
//    // Draw ceiling (use same triangulation but different height)
//    // Set z height to ceiling height
//    for (int j = 0; j < vertex_count; j += 5) {
//      vertices[j + 2] = sector->ceilingheight;
//    }
//    
//    // Update vertex buffer
//    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(float), vertices, GL_DYNAMIC_DRAW);
//    
//    // Bind ceiling texture
//    GLuint ceiling_tex = get_texture(sector->ceilingpic);
//    if (ceiling_tex != -1) {
//      glBindTexture(GL_TEXTURE_2D, ceiling_tex);
//      glUniform1i(glGetUniformLocation(floor_prog, "use_texture"), 1);
//    } else {
//      // Use flat color based on sector light level
//      float light = sector->lightlevel / 255.0f;
//      glUniform3f(glGetUniformLocation(floor_prog, "color"), light * 0.5f, light * 0.5f, light * 0.6f);
//      glUniform1i(glGetUniformLocation(floor_prog, "use_texture"), 0);
//    }
//    
//    // Draw triangles
//    glDrawArrays(GL_TRIANGLES, 0, vertex_count / 5);
  }
}
