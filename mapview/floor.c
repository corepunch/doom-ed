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

bool point_in_frustum(vec3 point, vec4 const planes[6]) {
  for (int i = 0; i < 6; i++) {
    float distance = glm_vec3_dot(point, (float*)planes[i]) + planes[i][3];
    if (distance < 0.0f) {
      return false; // Point is outside this plane
    }
  }
  return true; // Inside all planes
}

// Improved frustum culling for 2D linedefs
bool linedef_in_frustum_2d(vec4 const frustum[6], vec3 a, vec3 b) {
  // Quick reject: both endpoints are outside the same plane
  float a_left = glm_vec3_dot(a, (float*)frustum[0]) + frustum[0][3] < 0;
  float b_left = glm_vec3_dot(b, (float*)frustum[0]) + frustum[0][3] < 0;
  float a_right = glm_vec3_dot(a, (float*)frustum[1]) + frustum[1][3] < 0;
  float b_right = glm_vec3_dot(b, (float*)frustum[1]) + frustum[1][3] < 0;
  float a_near = glm_vec3_dot(a, (float*)frustum[4]) + frustum[4][3] < 0;
  float b_near = glm_vec3_dot(b, (float*)frustum[4]) + frustum[4][3] < 0;

  if ((a_left && b_left) || (a_right && b_right) || (a_near && b_near))
    return false;
  
  return true;
}

// Improved linedef-in-MVP check that handles edge cases better
bool linedef_in_mvp_2d(mat4 mvp, vec2 a, vec2 b) {
  vec4 pa = { a[0], a[1], 0.0f, 1.0f };
  vec4 pb = { b[0], b[1], 0.0f, 1.0f };
  
  // Transform points to clip space
  glm_mat4_mulv(mvp, pa, pa);
  glm_mat4_mulv(mvp, pb, pb);
  
  // Handle points behind the camera (negative w)
  if (pa[3] <= 0.0f && pb[3] <= 0.0f)
    return false;
  
  // Perspective divide for points with positive w
  if (pa[3] > 0.0f) {
    pa[0] /= pa[3]; pa[1] /= pa[3]; pa[2] /= pa[3];
  }
  
  if (pb[3] > 0.0f) {
    pb[0] /= pb[3]; pb[1] /= pb[3]; pb[2] /= pb[3];
  }
  
  // Special case: handle one point behind camera
  if (pa[3] <= 0.0f || pb[3] <= 0.0f) {
    // Line crosses near plane, so part is visible
    return true;
  }
  
  // Clipping check in normalized device coordinates
  const float EPSILON = 0.01f; // Small buffer to avoid edge cases
  bool completely_left = (pa[0] < -1.0f-EPSILON && pb[0] < -1.0f-EPSILON);
  bool completely_right = (pa[0] > 1.0f+EPSILON && pb[0] > 1.0f+EPSILON);
  bool completely_top = (pa[1] > 1.0f+EPSILON && pb[1] > 1.0f+EPSILON);
  bool completely_bottom = (pa[1] < -1.0f-EPSILON && pb[1] < -1.0f-EPSILON);
  
  // If completely outside any plane, reject
  if (completely_left || completely_right || completely_top || completely_bottom)
    return false;
  
  return true; // At least partially visible
}
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
  map->floors.sectors = realloc(map->floors.sectors, sizeof(mapsector2_t) * map->num_sectors);
  memset(map->floors.sectors, 0, sizeof(mapsector2_t) * map->num_sectors);
  for (uint32_t i = 0; i < map->num_sectors; i++) {
    map->floors.sectors[i].sector = &map->sectors[i];
  }
  
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

    if (!strncmp(map->sectors[i].ceilingpic, "F_SKY", 5)) {
      continue;
    }
    
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
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, x)); // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, u)); // UV
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, nx)); // Normal
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, color)); // Normal
  glEnableVertexAttribArray(3);
}

int sectors_drawn = 0;

void draw_walls(map_data_t const *map,
                mapsector_t const *sector,
                viewdef_t const *viewdef);

void draw_wall_ids(map_data_t const *map,
                   mapsector_t const *sector,
                   viewdef_t const *viewdef);

void draw_portals(map_data_t const *map,
                  mapsector_t const *sector,
                  viewdef_t const *viewdef,
                  void(*func)(map_data_t const *,
                              mapsector_t const *,
                              viewdef_t const *))
{
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    mapvertex_t const *a = &map->vertices[linedef->start];
    mapvertex_t const *b = &map->vertices[linedef->end];
    if (linedef->sidenum[0] == 0xFFFF || linedef->sidenum[1] == 0xFFFF)
      continue;
    for (int j = 0; j < 2; j++) {
      if (map->sidedefs[linedef->sidenum[j]].sector == sector - map->sectors &&
          (viewdef->nowalls ||
           linedef_in_frustum_2d(viewdef->frustum,
                                (vec3){a->x,a->y,sector->floorheight},
                                (vec3){b->x,b->y,sector->floorheight}) ||
           linedef_in_frustum_2d(viewdef->frustum,
                                 (vec3){a->x,a->y,sector->ceilingheight},
                                 (vec3){b->x,b->y,sector->ceilingheight})))
      {
        func(map, &map->sectors[map->sidedefs[linedef->sidenum[!j]].sector], viewdef);
      }
    }
  }
}

// Main function to draw floors and ceilings
void draw_floors(map_data_t const *map,
                 mapsector_t const *sector,
                 viewdef_t const *viewdef)
{
  if (!sector) {
    if (map->sectors) {
      sector = map->sectors;
    } else {
      return;
    }
  }
  if (map->floors.sectors[sector-map->sectors].frame == viewdef->frame)
    return;
  
  if (!viewdef->nowalls) {
    sectors_drawn++;
  }
  
  glBindVertexArray(map->floors.vao);
  
  // Set MVP matrix uniform
  glUniformMatrix4fv(glGetUniformLocation(world_prog, "mvp"), 1, GL_FALSE, viewdef->mvp[0]);
  glUniform3fv(glGetUniformLocation(world_prog, "viewPos"), 1, viewdef->viewpos);

  mapsector2_t *sec = &map->floors.sectors[sector-map->sectors];
  sec->frame = viewdef->frame;
  
  // Use flat color based on sector light level
  float light = sector->lightlevel / 255.0f;
  
  extern int pixel;
  glCullFace(GL_BACK);
//  if (CHECK_PIXEL(pixel, FLOOR, sector - map->sectors)) {
//    draw_textured_surface(&sec->floor, HIGHLIGHT(light), GL_TRIANGLES);
//  } else {
    draw_textured_surface(&sec->floor, light, GL_TRIANGLES);
//  }
  glCullFace(GL_FRONT);
//  if (CHECK_PIXEL(pixel, CEILING, sector - map->sectors)) {
//    draw_textured_surface(&sec->ceiling, HIGHLIGHT(light), GL_TRIANGLES);
//  } else {
    draw_textured_surface(&sec->ceiling, light, GL_TRIANGLES);
//  }
  glCullFace(GL_BACK);

  if (!viewdef->nowalls) {
    draw_walls(map, sector, viewdef);
  }
  
  draw_portals(map, sector, viewdef, draw_floors);
}

void
draw_floor_ids(map_data_t const *map,
               mapsector_t const *sector,
               viewdef_t const *viewdef)
{
  if (!sector) {
    if (map->sectors) {
      sector = map->sectors;
    } else {
      return;
    }
  }
  if (map->floors.sectors[sector-map->sectors].frame == viewdef->frame)
    return;

  uint32_t i = (uint32_t)(sector - map->sectors);
  mapsector2_t *sec = &map->floors.sectors[sector-map->sectors];
  sec->frame = viewdef->frame;

  glBindVertexArray(map->floors.vao);

  glCullFace(GL_BACK);
  draw_textured_surface_id(&sec->floor, i | PIXEL_FLOOR, GL_TRIANGLES);

  glCullFace(GL_FRONT);
  draw_textured_surface_id(&sec->ceiling, i | PIXEL_CEILING, GL_TRIANGLES);
  
  // Reset texture binding
  glCullFace(GL_BACK);
  glBindTexture(GL_TEXTURE_2D, 0);

  draw_wall_ids(map, sector, viewdef);

  draw_portals(map, sector, viewdef, draw_floor_ids);
}

