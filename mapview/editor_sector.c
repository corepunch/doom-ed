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

#define DIST_SQ(X1, Y1, X2, Y2) ((X1-X2)*(X1-X2) + (Y1-Y2)*(Y1-Y2))

// Check if a point exists at the given coordinates
bool point_exists(int x, int y, map_data_t *map) {
  const int threshold = 8;
  for (int i = 0; i < map->num_vertices; i++) {
    if (DIST_SQ(map->vertices[i].x, map->vertices[i].y, x, y) < threshold*threshold) {
      return true;
    }
  }
  return false;
}

// Add a new vertex to the map
static uint16_t add_vertex(map_data_t *map, int x, int y) {
  CHECK_CAPACITY(mapvertex_t, vertices);
  RESIZE_ARRAY(mapvertex_t, vertices, map->num_vertices + 1);
  
  uint16_t index = map->num_vertices;
  map->vertices[index] = (mapvertex_t){ .x = x, .y = y };
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
static bool is_clockwise(int points[][2], int num_points) {
  double sum = 0.0;
  for (int i = 0; i < num_points; i++) {
    int j = (i + 1) % num_points;
    sum += (points[j][0] - points[i][0]) * (points[j][1] + points[i][1]);
  }
  return sum > 0;
}

// Forward declaration
bool point_in_sector(map_data_t const* map, int x, int y, int sector_index);

// Find sectors containing a given point
static void find_containing_sectors(map_data_t const *map, int x, int y, int new_sector_index,
                                    uint16_t *sectors, int *num_sectors) {
  *num_sectors = 0;
  for (int i = 0; i < map->num_sectors; i++) {
    if (point_in_sector(map, x, y, i) && new_sector_index != i) {
      sectors[(*num_sectors)++] = i;
    }
  }
}

// Check if a linedef intersects any points in the draw path
static bool linedef_intersects_path(map_data_t *map, uint16_t linedef_index,
                                    int draw_points[][2], int num_draw_points) {
  maplinedef_t *linedef = &map->linedefs[linedef_index];
  mapvertex_t *v1 = &map->vertices[linedef->start];
  mapvertex_t *v2 = &map->vertices[linedef->end];
  
  for (int i = 0; i < num_draw_points; i++) {
    int next = (i + 1) % num_draw_points;
    int x1 = draw_points[i][0];
    int y1 = draw_points[i][1];
    int x2 = draw_points[next][0];
    int y2 = draw_points[next][1];
    
    // Line segment intersection test
    int dx1 = v2->x - v1->x;
    int dy1 = v2->y - v1->y;
    int dx2 = x2 - x1;
    int dy2 = y2 - y1;
    
    int delta = dx1 * dy2 - dy1 * dx2;
    if (delta == 0) continue; // Parallel lines
    
    float t1 = ((x1 - v1->x) * dy2 - (y1 - v1->y) * dx2) / (float)delta;
    float t2 = ((v1->x - x1) * dy1 - (v1->y - y1) * dx1) / (float)(-delta);
    
    if (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1) {
      return true; // Intersection found
    }
  }
  return false;
}

// Find or create a vertex at the given position
static uint16_t get_or_add_vertex(map_data_t *map, int x, int y) {
  const int threshold = 8;
  
  // Check if vertex already exists
  for (int j = 0; j < map->num_vertices; j++) {
    if (DIST_SQ(map->vertices[j].x, map->vertices[j].y, x, y) < threshold*threshold) {
      return j;
    }
  }
  
  // Create new vertex
  return add_vertex(map, x, y);
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

// Process each edge of the new sector
static void process_sector_edges(map_data_t const *map, uint16_t new_sector_index,
                                 uint16_t vertex_indices[], int num_points, int center_x, int center_y) {
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
        int x1 = map->vertices[linedef->start].x;
        int y1 = map->vertices[linedef->start].y;
        int x2 = map->vertices[linedef->end].x;
        int y2 = map->vertices[linedef->end].y;
        int side = (x2 - x1) * (center_y - y1) - (y2 - y1) * (center_x - x1);
        
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
      memset(tmp.sidedefs[linedef->sidenum[1]].midtexture, 0, sizeof(texname_t));

      // Update linedef flags
      linedef->flags &= ~1;  // Remove impassable flag
      linedef->flags |= 4;   // Set two-sided flag
    } else {
      // Create new linedef
      uint16_t front_side = add_sidedef(&tmp, new_sector_index);
      uint16_t back_side = 0xFFFF;
      
      // Check if should be two-sided
      float dx = map->vertices[vertex_indices[next]].x - map->vertices[vertex_indices[i]].x;
      float dy = map->vertices[vertex_indices[next]].y - map->vertices[vertex_indices[i]].y;
      float length = sqrtf(dx*dx + dy*dy);
      float nx = -dy / length; // Normal points right
      float ny = dx / length;
      
      // Check point to right of linedef
      int mid_x = (map->vertices[vertex_indices[i]].x + map->vertices[vertex_indices[next]].x) / 2;
      int mid_y = (map->vertices[vertex_indices[i]].y + map->vertices[vertex_indices[next]].y) / 2;
      int check_x = mid_x + (int)(nx * 5);
      int check_y = mid_y + (int)(ny * 5);
      
      // Find sectors at this point
      uint16_t right_sectors[1024];
      int num_right_sectors = 0;
      find_containing_sectors(map, check_x, check_y, new_sector_index, right_sectors, &num_right_sectors);
      
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
                                    int draw_points[][2], int num_points, uint16_t parent_sector) {
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
      if (linedef->sidenum[0] != 0xFFFF && linedef->sidenum[1] == 0xFFFF) {
        linedef->sidenum[1] = add_sidedef(map, new_sector_index);
        linedef->flags &= ~1; // Remove impassable flag
        linedef->flags |= 4;  // Set two-sided flag
      }
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

void finish_sector(map_data_t *map, editor_state_t *editor) {
  if (editor->num_draw_points < 3) {
    printf("Need at least 3 points to create a sector!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // Ensure counter-clockwise winding (standard for Doom)
  if (is_clockwise(editor->draw_points, editor->num_draw_points)) {
    // Reverse the points
    for (int i = 0; i < editor->num_draw_points / 2; i++) {
      int j = editor->num_draw_points - 1 - i;
      // Swap points
      int temp_x = editor->draw_points[i][0];
      int temp_y = editor->draw_points[i][1];
      editor->draw_points[i][0] = editor->draw_points[j][0];
      editor->draw_points[i][1] = editor->draw_points[j][1];
      editor->draw_points[j][0] = temp_x;
      editor->draw_points[j][1] = temp_y;
    }
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
  int center_x = 0, center_y = 0;
  for (int i = 0; i < editor->num_draw_points; i++) {
    center_x += editor->draw_points[i][0];
    center_y += editor->draw_points[i][1];
  }
  center_x /= editor->num_draw_points;
  center_y /= editor->num_draw_points;
  
  // Find containing sectors
  uint16_t containing_sectors[1024];
  int num_containing_sectors = 0;
  find_containing_sectors(map, center_x, center_y, -1, containing_sectors, &num_containing_sectors);
  
  // Copy properties from parent sector if inside one
  if (num_containing_sectors > 0) {
    uint16_t parent_sector = containing_sectors[0];
    copy_sector_properties(map, new_sector_index, parent_sector);
    printf("New sector inherits properties from sector %d\n", parent_sector);
  }
  
  // Get or create vertices
  uint16_t vertex_indices[MAX_DRAW_POINTS];
  for (int i = 0; i < editor->num_draw_points; i++) {
    vertex_indices[i] = get_or_add_vertex(map, editor->draw_points[i][0], editor->draw_points[i][1]);
  }
  
  // Process sector edges
  process_sector_edges(map, new_sector_index, vertex_indices, editor->num_draw_points, center_x, center_y);
  
  // Handle sector splitting if inside another sector
  if (num_containing_sectors > 0) {
    handle_sector_splitting(map, new_sector_index, editor->draw_points,
                            editor->num_draw_points, containing_sectors[0]);
  } else for (int i = 0; i < map->num_linedefs; i++) {
    if (map->linedefs[i].sidenum[0] >= map->num_sidedefs ||
        map->linedefs[i].sidenum[1] >= map->num_sidedefs)
      continue;
    if (map->sidedefs[map->linedefs[i].sidenum[0]].sector == new_sector_index) {
      copy_sector_properties(map, new_sector_index, map->sidedefs[map->linedefs[i].sidenum[1]].sector);
      printf("New sector inherits properties from sector %d\n", map->sidedefs[map->linedefs[i].sidenum[1]].sector);
      break;
    }
    if (map->sidedefs[map->linedefs[i].sidenum[1]].sector == new_sector_index) {
      copy_sector_properties(map, new_sector_index, map->sidedefs[map->linedefs[i].sidenum[0]].sector);
      printf("New sector inherits properties from sector %d\n", map->sidedefs[map->linedefs[i].sidenum[0]].sector);
      break;
    }
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
  int vertex = add_vertex(map, x, y);
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
