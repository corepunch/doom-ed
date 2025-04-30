#include "editor.h"
#include <math.h>

// Macros to reduce code duplication
#define RESIZE_ARRAY(TYPE, ARRAY, SIZE) \
TYPE *new_##ARRAY = realloc(map->ARRAY, (SIZE) * sizeof(TYPE)); \
if (!new_##ARRAY) { \
printf("Failed to allocate memory for new %s!\n", #ARRAY); \
return 0xFFFF; \
} \
map->ARRAY = new_##ARRAY

#define CHECK_CAPACITY(TYPE, ARRAY) \
if (map->num_##ARRAY >= 65535) { \
printf("Maximum number of %s reached!\n", #ARRAY); \
return 0xFFFF; \
}

// Utility functions for vertex manipulation
static float vertex_distance_sq(mapvertex_t v1, mapvertex_t v2) {
  float dx = v1.x - v2.x;
  float dy = v1.y - v2.y;
  return dx*dx + dy*dy;
}

static mapvertex_t vertex_midpoint(mapvertex_t v1, mapvertex_t v2) {
  mapvertex_t mid = {
    .x = (v1.x + v2.x) / 2,
    .y = (v1.y + v2.y) / 2
  };
  return mid;
}

static mapvertex_t vertex_normal(mapvertex_t v1, mapvertex_t v2, float scale) {
  float dx = v2.x - v1.x;
  float dy = v2.y - v1.y;
  float length = sqrtf(dx*dx + dy*dy);
  
  mapvertex_t normal = {
    .x = (int)(-dy / length * scale),
    .y = (int)(dx / length * scale)
  };
  return normal;
}

static bool vertex_is_near(mapvertex_t v1, mapvertex_t v2, int threshold) {
  return vertex_distance_sq(v1, v2) < threshold * threshold;
}

static mapvertex_t compute_centroid(mapvertex_t vertices[], int count) {
  int sum_x = 0, sum_y = 0;
  for (int i = 0; i < count; i++) {
    sum_x += vertices[i].x;
    sum_y += vertices[i].y;
  }
  
  mapvertex_t centroid = {
    .x = sum_x / count,
    .y = sum_y / count
  };
  return centroid;
}

// Check if a point exists at the given coordinates
bool point_exists(mapvertex_t point, map_data_t *map, int *index) {
  const int threshold = 8;
  for (int i = 0; i < map->num_vertices; i++) {
    if (vertex_is_near(map->vertices[i], point, threshold)) {
      if (index) *index = i;
      return true;
    }
  }
  return false;
}

// Add a new vertex to the map
static uint16_t add_vertex(map_data_t *map, mapvertex_t vertex) {
  CHECK_CAPACITY(mapvertex_t, vertices);
  RESIZE_ARRAY(mapvertex_t, vertices, map->num_vertices + 1);
  
  uint16_t index = map->num_vertices;
  map->vertices[index] = vertex;
  map->num_vertices++;
  
  return index;
}

// Add a new linedef to the map
static uint16_t add_linedef(map_data_t *map, uint16_t start, uint16_t end,
                            uint16_t front_side, uint16_t back_side) {
  CHECK_CAPACITY(maplinedef_t, linedefs);
  RESIZE_ARRAY(maplinedef_t, linedefs, map->num_linedefs + 1);
  
  uint16_t index = map->num_linedefs;
  map->linedefs[index] = (maplinedef_t){
    .start = end,
    .end = start,
    .flags = (back_side == 0xFFFF) ? 1 : 4, // 1=impassable, 4=two-sided
    .special = 0,
    .tag = 0,
    .sidenum = { front_side, back_side }
  };
  map->num_linedefs++;
  
  return index;
}

// Add a new sidedef to the map
static uint16_t add_sidedef(map_data_t *map, uint16_t sector_index) {
  CHECK_CAPACITY(mapsidedef_t, sidedefs);
  RESIZE_ARRAY(mapsidedef_t, sidedefs, map->num_sidedefs + 1);
  
  uint16_t index = map->num_sidedefs;
  map->sidedefs[index] = (mapsidedef_t){
    .textureoffset = 0,
    .rowoffset = 0,
    .sector = sector_index,
  };
  
  // Set default textures
  map->sidedefs[index].toptexture[0] = '-';
  map->sidedefs[index].toptexture[1] = '\0';
  map->sidedefs[index].bottomtexture[0] = '-';
  map->sidedefs[index].bottomtexture[1] = '\0';
  strcpy(map->sidedefs[index].midtexture, "BRONZE1");
  
  map->num_sidedefs++;
  return index;
}

// Add a new sector to the map
static uint16_t add_sector(map_data_t *map) {
  CHECK_CAPACITY(mapsector_t, sectors);
  RESIZE_ARRAY(mapsector_t, sectors, map->num_sectors + 1);
  
  uint16_t index = map->num_sectors;
  map->sectors[index] = (mapsector_t){
    .floorheight = 0,
    .ceilingheight = 128,
    .lightlevel = 160,
    .special = 0,
    .tag = 0
  };
  
  // Set default textures
  strncpy(map->sectors[index].floorpic, "FLOOR", 8);
  strncpy(map->sectors[index].ceilingpic, "CEIL", 8);
  
  map->num_sectors++;
  return index;
}

// Calculate if points form clockwise polygon
static bool is_clockwise(mapvertex_t points[], int num_points) {
  double sum = 0.0;
  for (int i = 0; i < num_points; i++) {
    int j = (i + 1) % num_points;
    sum += (points[j].x - points[i].x) * (points[j].y + points[i].y);
  }
  return sum > 0;
}

// Forward declaration
bool point_in_sector(map_data_t const* map, mapvertex_t point, int sector_index);

// Find sectors containing a given point
static void find_containing_sectors(map_data_t const *map, mapvertex_t point, int new_sector_index,
                                    uint16_t *sectors, int *num_sectors) {
  *num_sectors = 0;
  for (int i = 0; i < map->num_sectors; i++) {
    if (point_in_sector(map, point, i) && new_sector_index != i) {
      sectors[(*num_sectors)++] = i;
    }
  }
}

// Check if two line segments intersect
static bool segments_intersect(mapvertex_t v1, mapvertex_t v2, mapvertex_t v3, mapvertex_t v4) {
  int dx1 = v2.x - v1.x;
  int dy1 = v2.y - v1.y;
  int dx2 = v4.x - v3.x;
  int dy2 = v4.y - v3.y;
  
  int delta = dx1 * dy2 - dy1 * dx2;
  if (delta == 0) return false; // Parallel lines
  
  float t1 = ((v3.x - v1.x) * dy2 - (v3.y - v1.y) * dx2) / (float)delta;
  float t2 = ((v1.x - v3.x) * dy1 - (v1.y - v3.y) * dx1) / (float)(-delta);
  
  return (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1);
}

// Check if a linedef intersects any points in the draw path
static bool linedef_intersects_path(map_data_t *map, uint16_t linedef_index,
                                    mapvertex_t draw_points[], int num_draw_points) {
  maplinedef_t *linedef = &map->linedefs[linedef_index];
  mapvertex_t v1 = map->vertices[linedef->start];
  mapvertex_t v2 = map->vertices[linedef->end];
  
  for (int i = 0; i < num_draw_points; i++) {
    int next = (i + 1) % num_draw_points;
    mapvertex_t p1 = draw_points[i];
    mapvertex_t p2 = draw_points[next];
    
    if (segments_intersect(v1, v2, p1, p2)) {
      return true; // Intersection found
    }
  }
  return false;
}

// Find or create a vertex at the given position
static uint16_t get_or_add_vertex(map_data_t *map, mapvertex_t vertex) {
  const int threshold = 8;
  
  // Check if vertex already exists
  for (int j = 0; j < map->num_vertices; j++) {
    if (vertex_is_near(map->vertices[j], vertex, threshold)) {
      return j;
    }
  }
  
  // Create new vertex
  return add_vertex(map, vertex);
}

// Check if linedef already exists between vertices
static int find_existing_linedef(map_data_t const *map, uint16_t v1, uint16_t v2) {
  for (int j = 0; j < map->num_linedefs; j++) {
    if ((map->linedefs[j].start == v1 && map->linedefs[j].end == v2) ||
        (map->linedefs[j].start == v2 && map->linedefs[j].end == v1)) {
      return j;
    }
  }
  return -1;
}

// Determine which side of a line a point is on
static int point_side_of_line(mapvertex_t line_start, mapvertex_t line_end, mapvertex_t point) {
  return (line_end.x - line_start.x) * (point.y - line_start.y) -
  (line_end.y - line_start.y) * (point.x - line_start.x);
}

// Update linedef to be two-sided
static void make_linedef_two_sided(map_data_t *map, maplinedef_t *linedef, uint16_t sector_index) {
  if (linedef->sidenum[0] != 0xFFFF && linedef->sidenum[1] == 0xFFFF) {
    linedef->sidenum[1] = add_sidedef(map, sector_index);
    linedef->flags &= ~1; // Remove impassable flag
    linedef->flags |= 4;  // Set two-sided flag
    
    // Clear middle textures for two-sided linedefs
    memset(map->sidedefs[linedef->sidenum[0]].midtexture, 0, sizeof(texname_t));
    memset(map->sidedefs[linedef->sidenum[1]].midtexture, 0, sizeof(texname_t));
  }
}

// Process each edge of the new sector
static void process_sector_edges(map_data_t const *map, uint16_t new_sector_index,
                                 uint16_t vertex_indices[], int num_points, mapvertex_t center) {
  map_data_t tmp={0};
  memcpy(&tmp, map, sizeof(map_data_t));
  
  for (int i = 0; i < num_points; i++) {
    int next = (i + 1) % num_points;
    int linedef_idx = find_existing_linedef(map, vertex_indices[i], vertex_indices[next]);
    
    if (linedef_idx >= 0) {
      // Linedef exists
      maplinedef_t *linedef = &map->linedefs[linedef_idx];
      
      // Handle two-sided linedef
      if (linedef->sidenum[0] != 0xFFFF && linedef->sidenum[1] != 0xFFFF) {
        // Determine which side our sector is on
        mapvertex_t line_start = map->vertices[linedef->start];
        mapvertex_t line_end = map->vertices[linedef->end];
        
        int side = point_side_of_line(line_start, line_end, center);
        
        if (side < 0) { // Point is on left side
          if (linedef->sidenum[1] != 0xFFFF) {
            tmp.sidedefs[linedef->sidenum[1]].sector = new_sector_index;
          }
        } else { // Point is on right side
          if (linedef->sidenum[0] != 0xFFFF) {
            tmp.sidedefs[linedef->sidenum[0]].sector = new_sector_index;
          }
        }
        continue;
      }
      
      // Add sidedefs as needed
      if (linedef->start == vertex_indices[i] && linedef->end == vertex_indices[next]) {
        // Same direction
        if (linedef->sidenum[0] == 0xFFFF) {
          linedef->sidenum[0] = add_sidedef(&tmp, new_sector_index);
        } else {
          linedef->sidenum[1] = add_sidedef(&tmp, new_sector_index);
        }
      } else {
        // Opposite direction
        if (linedef->sidenum[1] == 0xFFFF) {
          linedef->sidenum[1] = add_sidedef(&tmp, new_sector_index);
        } else {
          linedef->sidenum[0] = add_sidedef(&tmp, new_sector_index);
        }
      }
      
      memset(tmp.sidedefs[linedef->sidenum[0]].midtexture, 0, sizeof(texname_t));
      if (linedef->sidenum[1] != 0xFFFF) {
        memset(tmp.sidedefs[linedef->sidenum[1]].midtexture, 0, sizeof(texname_t));
      }
      
      // Update linedef flags
      linedef->flags &= ~1;  // Remove impassable flag
      linedef->flags |= 4;   // Set two-sided flag
    } else {
      // Create new linedef
      uint16_t front_side = add_sidedef(&tmp, new_sector_index);
      uint16_t back_side = 0xFFFF;
      
      // Check if should be two-sided
      mapvertex_t v1 = map->vertices[vertex_indices[i]];
      mapvertex_t v2 = map->vertices[vertex_indices[next]];
      
      // Calculate normal and check point
      mapvertex_t mid = vertex_midpoint(v1, v2);
      mapvertex_t normal = vertex_normal(v1, v2, 5);
      mapvertex_t check_point = {
        .x = mid.x + normal.x,
        .y = mid.y + normal.y
      };
      
      // Find sectors at this point
      uint16_t right_sectors[1024];
      int num_right_sectors = 0;
      find_containing_sectors(map, check_point, new_sector_index, right_sectors, &num_right_sectors);
      
      // If sector found on right, make linedef two-sided
      if (num_right_sectors > 0) {
        back_side = add_sidedef(&tmp, right_sectors[0]);
        memset(tmp.sidedefs[back_side].midtexture, 0, sizeof(texname_t));
        memset(tmp.sidedefs[front_side].midtexture, 0, sizeof(texname_t));
      }
      
      add_linedef(&tmp, vertex_indices[i], vertex_indices[next], front_side, back_side);
    }
  }
  memcpy((map_data_t *)map, &tmp, sizeof(map_data_t));
}

// Handle sector splitting
static void handle_sector_splitting(map_data_t *map, uint16_t new_sector_index,
                                    mapvertex_t draw_points[], int num_points, uint16_t parent_sector) {
  // Find linedefs that intersect with new sector boundary
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t *linedef = &map->linedefs[i];
    
    // Check if from parent sector
    bool is_parent_sector_linedef = false;
    if ((linedef->sidenum[0] != 0xFFFF &&
         map->sidedefs[linedef->sidenum[0]].sector == parent_sector) ||
        (linedef->sidenum[1] != 0xFFFF &&
         map->sidedefs[linedef->sidenum[1]].sector == parent_sector)) {
      is_parent_sector_linedef = true;
    }
    
    if (!is_parent_sector_linedef) continue;
    
    // Check if linedef intersects new sector boundary
    if (linedef_intersects_path(map, i, draw_points, num_points)) {
      // Update intersected linedef to be two-sided
      make_linedef_two_sided(map, linedef, new_sector_index);
    }
  }
}

// Copy sector properties from parent to child
static void copy_sector_properties(map_data_t *map, uint16_t dest, uint16_t src) {
  mapsector_t *dest_sector = &map->sectors[dest];
  mapsector_t *src_sector = &map->sectors[src];
  
  dest_sector->floorheight = src_sector->floorheight;
  dest_sector->ceilingheight = src_sector->ceilingheight;
  strncpy(dest_sector->floorpic, src_sector->floorpic, 8);
  strncpy(dest_sector->ceilingpic, src_sector->ceilingpic, 8);
  dest_sector->lightlevel = src_sector->lightlevel;
  dest_sector->special = src_sector->special;
  dest_sector->tag = src_sector->tag;
}

// Find a sector to inherit properties from based on surrounding linedefs
static void find_and_copy_sector_properties(map_data_t *map, uint16_t new_sector_index) {
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t *linedef = &map->linedefs[i];
    
    if (linedef->sidenum[0] >= map->num_sidedefs || linedef->sidenum[1] >= map->num_sidedefs)
      continue;
    
    if (map->sidedefs[linedef->sidenum[0]].sector == new_sector_index) {
      copy_sector_properties(map, new_sector_index, map->sidedefs[linedef->sidenum[1]].sector);
      printf("New sector inherits properties from sector %d\n", map->sidedefs[linedef->sidenum[1]].sector);
      return;
    }
    
    if (map->sidedefs[linedef->sidenum[1]].sector == new_sector_index) {
      copy_sector_properties(map, new_sector_index, map->sidedefs[linedef->sidenum[0]].sector);
      printf("New sector inherits properties from sector %d\n", map->sidedefs[linedef->sidenum[0]].sector);
      return;
    }
  }
}

// Reverse the order of points in an array
static void reverse_vertex_array(mapvertex_t points[], int count) {
  for (int i = 0; i < count / 2; i++) {
    int j = count - 1 - i;
    // Swap points
    mapvertex_t temp = points[i];
    points[i] = points[j];
    points[j] = temp;
  }
}

// Validate and prepare draw points for sector creation
static bool prepare_draw_points(editor_state_t *editor) {
  if (editor->num_draw_points < 3) {
    printf("Need at least 3 points to create a sector!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return false;
  }
  
  // Ensure counter-clockwise winding (standard for Doom)
  if (is_clockwise(editor->draw_points, editor->num_draw_points)) {
    reverse_vertex_array(editor->draw_points, editor->num_draw_points);
  }
  
  return true;
}

void finish_sector(map_data_t *map, editor_state_t *editor) {
  if (!prepare_draw_points(editor)) {
    return;
  }
  
  // Create a new sector
  uint16_t new_sector_index = add_sector(map);
  if (new_sector_index == 0xFFFF) {
    printf("Failed to create sector, aborting!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // Find center point of new sector
  mapvertex_t center = compute_centroid(editor->draw_points, editor->num_draw_points);
  
  // Find containing sectors
  uint16_t containing_sectors[1024];
  int num_containing_sectors = 0;
  find_containing_sectors(map, center, -1, containing_sectors, &num_containing_sectors);
  
  // Copy properties from parent sector if inside one
  if (num_containing_sectors > 0) {
    uint16_t parent_sector = containing_sectors[0];
    copy_sector_properties(map, new_sector_index, parent_sector);
    printf("New sector inherits properties from sector %d\n", parent_sector);
  }
  
  // Get or create vertices
  uint16_t vertex_indices[MAX_DRAW_POINTS];
  for (int i = 0; i < editor->num_draw_points; i++) {
    vertex_indices[i] = get_or_add_vertex(map, editor->draw_points[i]);
  }
  
  // Process sector edges
  process_sector_edges(map, new_sector_index, vertex_indices, editor->num_draw_points, center);
  
  // Handle sector splitting if inside another sector
  if (num_containing_sectors > 0) {
    handle_sector_splitting(map, new_sector_index, editor->draw_points,
                            editor->num_draw_points, containing_sectors[0]);
  } else {
    find_and_copy_sector_properties(map, new_sector_index);
  }
  
  printf("Created sector %d with %d vertices and %d walls\n",
         new_sector_index, editor->num_draw_points, editor->num_draw_points);
  
  // Reset drawing state
  editor->drawing = false;
  editor->num_draw_points = 0;
  
  // Rebuild vertex buffers
  build_wall_vertex_buffer(map);
  build_floor_vertex_buffer(map);
}

void split_linedef(map_data_t *map, int linedef_id, float x, float y) {
  maplinedef_t *linedef = &map->linedefs[linedef_id];
  mapvertex_t split_point = {.x = (int16_t)x, .y = (int16_t)y};
  
  int vertex = add_vertex(map, split_point);
  uint16_t front = 0xffff, back = 0xffff;
  
  if (linedef->sidenum[0] < map->num_sidedefs) {
    front = add_sidedef(map, 0);
    memcpy(&map->sidedefs[front], &map->sidedefs[linedef->sidenum[0]], sizeof(mapsidedef_t));
  }
  
  if (linedef->sidenum[1] < map->num_sidedefs) {
    back = add_sidedef(map, 0);
    memcpy(&map->sidedefs[back], &map->sidedefs[linedef->sidenum[1]], sizeof(mapsidedef_t));
  }
  
  add_linedef(map, linedef->end, vertex, front, back);
  linedef->end = vertex;
  
  // Rebuild vertex buffers
  build_wall_vertex_buffer(map);
  build_floor_vertex_buffer(map);
}
