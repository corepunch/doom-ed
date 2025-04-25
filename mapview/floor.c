#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include "map.h"

// Global variables to add
extern GLuint world_prog;

#define MAX_EDGES 0x10000
#define MAX_VERTICES 1024
#define TEX_SIZE 64.0f

GLuint compile(GLenum type, const char* src);
mapside_texture_t const *get_flat_texture(const char* name);

// Add this to your init_sdl function
void init_floor_shader(void) {
}

// Improved triangulation function for DOOM sectors
int triangulate_sector(mapvertex_t *vertices, int num_vertices, wall_vertex_t *out_vertices);

// Function to calculate texture coordinates based on sector dimensions
void calculate_texture_coords(wall_vertex_t *vertices, int vertex_count) {
  // Find bounds of the sector
  float min_x = FLT_MAX, min_y = FLT_MAX;
  float max_x = -FLT_MAX, max_y = -FLT_MAX;
  
  for (int i = 0; i < vertex_count; i++) {
    float x = vertices[i].x;
    float y = vertices[i].y;
    
    min_x = MIN(min_x, x);
    min_y = MIN(min_y, y);
    max_x = MAX(max_x, x);
    max_y = MAX(max_y, y);
  }
  
  // Update texture coordinates
  for (int i = 0; i < vertex_count; i++) {
    // Texture coordinates (normalized 0-1)
    vertices[i].u = (vertices[i].x - min_x);
    vertices[i].v = (vertices[i].y - min_y);
    vertices[i].nz = -127;
  }
}

static bool
belongs_to_sector(uint32_t i,
                  const maplinedef_t *linedef,
                  const map_data_t *map)
{
  bool value = false;
  if (linedef->sidenum[0] != 0xFFFF && map->sidedefs[linedef->sidenum[0]].sector == i) {
    value = !value;
  }
  if (linedef->sidenum[1] != 0xFFFF && map->sidedefs[linedef->sidenum[1]].sector == i) {
    value = !value;
  }
  return value;
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
  static bool used[MAX_EDGES];
  
  memset(used, 0, sizeof(used));
  
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
      return 0;
    }
  }
  return num_vertices;
}

void build_floor_vertex_buffer(map_data_t *map) {
  map->floors.num_vertices = 0;
  // Create VAO for walls if not already created
  if (!map->floors.vao) {
    glGenVertexArrays(1, &map->floors.vao);
    glGenBuffers(1, &map->floors.vbo);
  }
  
  for (int i = 0; i < map->num_sectors; i++) {
    // Collect all vertices for this sector
    mapvertex_t sector_vertices[MAX_VERTICES]; // Assuming max MAX_VERTICES vertices per sector
    int num_vertices = get_sector_vertices(map, i, sector_vertices);
    
    // Skip sectors with too few vertices
    if (num_vertices < 3) continue;
    
    // Triangulate the sector
    wall_vertex_t vertices[MAX_VERTICES]={0};
    int vertex_count = triangulate_sector(sector_vertices, num_vertices, vertices);
    
    // Set appropriate texture coordinates
    calculate_texture_coords(vertices, vertex_count);
    
    // Draw floor
    // Set z height to floor height
    for (int j = 0; j < vertex_count; j++) {
      vertices[j].z = map->sectors[i].floorheight;
    }
    
    map->floors.sectors[i].floor.vertex_start = map->floors.num_vertices;
    map->floors.sectors[i].floor.vertex_count = vertex_count;
    map->floors.sectors[i].floor.texture = get_flat_texture(map->sectors[i].floorpic);

    memcpy(&map->floors.vertices[map->floors.num_vertices], vertices, vertex_count * sizeof(wall_vertex_t));
    map->floors.num_vertices += vertex_count;

    for (int j = 0; j < vertex_count; j++) {
      vertices[j].z = map->sectors[i].ceilingheight;
    }

    map->floors.sectors[i].ceiling.vertex_start = map->floors.num_vertices;
    map->floors.sectors[i].ceiling.vertex_count = vertex_count;
    map->floors.sectors[i].ceiling.texture = get_flat_texture(map->sectors[i].ceilingpic);
    
    memcpy(&map->floors.vertices[map->floors.num_vertices], vertices, vertex_count * sizeof(wall_vertex_t));
    map->floors.num_vertices += vertex_count;
  }
  
  // Upload vertex data to GPU
  glBindVertexArray(map->floors.vao);
  glBindBuffer(GL_ARRAY_BUFFER, map->floors.vbo);
  glBufferData(GL_ARRAY_BUFFER, map->floors.num_vertices * sizeof(wall_vertex_t), map->floors.vertices, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), (void*)0); // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), (void*)(3 * sizeof(int16_t))); // UV
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(wall_vertex_t), (void*)(5 * sizeof(int16_t))); // Normal
  glEnableVertexAttribArray(2);
}

// Main function to draw floors and ceilings
void draw_floors(map_data_t const *map, mat4 mvp) {
  glBindVertexArray(map->floors.vao);
  
  // Set MVP matrix uniform
  glUniformMatrix4fv(glGetUniformLocation(world_prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  
  // Process each sector
  for (int i = 0; i < map->num_sectors; i++) {
    // Use flat color based on sector light level
    float light = map->sectors[i].lightlevel / 255.0f;
    
    extern int pixel;
    draw_textured_surface(&map->floors.sectors[i].floor, pixel == (i+1) * 0x10000 ? light + 0.25 : light, GL_TRIANGLES);
    draw_textured_surface(&map->floors.sectors[i].ceiling, pixel == (i+10000) * 0x10000 ? light + 0.25 : light, GL_TRIANGLES);
  }
}

void draw_floor_ids(map_data_t const *map, mat4 mvp) {
  glBindVertexArray(map->floors.vao);

  for (int i = 0; i < map->num_sectors; i++) {
    draw_textured_surface_id(&map->floors.sectors[i].floor, (i+1) * 0x10000, GL_TRIANGLES);
    draw_textured_surface_id(&map->floors.sectors[i].ceiling, ((i+10000) * 0x10000), GL_TRIANGLES);
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}
