#include "editor.h"

// Check if a point exists at the given coordinates (within a small threshold)
bool point_exists(int x, int y, map_data_t *map) {
  const int threshold = 8; // Distance threshold
  
  for (int i = 0; i < map->num_vertices; i++) {
    int dx = map->vertices[i].x - x;
    int dy = map->vertices[i].y - y;
    if (dx*dx + dy*dy < threshold*threshold) {
      return true;
    }
  }
  return false;
}

// Add a new vertex to the map
static uint16_t add_vertex(map_data_t *map, int x, int y) {
  // Check if we're at capacity
  if (map->num_vertices >= 65535) {
    printf("Maximum number of vertices reached!\n");
    return 0;
  }
  
  // Resize vertices array if needed
  mapvertex_t *new_vertices = realloc(map->vertices, (map->num_vertices + 1) * sizeof(mapvertex_t));
  if (!new_vertices) {
    printf("Failed to allocate memory for new vertex!\n");
    return 0;
  }
  map->vertices = new_vertices;
  
  // Set vertex data
  uint16_t index = map->num_vertices;
  map->vertices[index].x = x;
  map->vertices[index].y = y;
  map->num_vertices++;
  
  return index;
}

// Add a new linedef to the map
static void add_linedef(map_data_t *map, uint16_t start, uint16_t end) {
  // Check if we're at capacity
  if (map->num_linedefs >= 65535) {
    printf("Maximum number of linedefs reached!\n");
    return;
  }
  
  // Resize linedefs array if needed
  maplinedef_t *new_linedefs = realloc(map->linedefs, (map->num_linedefs + 1) * sizeof(maplinedef_t));
  if (!new_linedefs) {
    printf("Failed to allocate memory for new linedef!\n");
    return;
  }
  map->linedefs = new_linedefs;
  
  // Set linedef data
  uint16_t index = map->num_linedefs;
  map->linedefs[index].start = start;
  map->linedefs[index].end = end;
  map->linedefs[index].flags = 1;  // Impassable by default
  map->linedefs[index].special = 0;
  map->linedefs[index].tag = 0;
  map->linedefs[index].sidenum[0] = map->num_sidedefs > 0 ? 0 : 0xFFFF;  // Front sidedef
  map->linedefs[index].sidenum[1] = 0xFFFF;  // No back sidedef
  map->num_linedefs++;
}

// Add these helper functions for sidedef and sector creation
static uint16_t add_sidedef(map_data_t *map, uint16_t sector_index) {
  // Check if we're at capacity
  if (map->num_sidedefs >= 65535) {
    printf("Maximum number of sidedefs reached!\n");
    return 0xFFFF;
  }
  
  // Resize sidedefs array if needed
  mapsidedef_t *new_sidedefs = realloc(map->sidedefs, (map->num_sidedefs + 1) * sizeof(mapsidedef_t));
  if (!new_sidedefs) {
    printf("Failed to allocate memory for new sidedef!\n");
    return 0xFFFF;
  }
  map->sidedefs = new_sidedefs;
  
  // Set sidedef data
  uint16_t index = map->num_sidedefs;
  map->sidedefs[index].textureoffset = 0;
  map->sidedefs[index].rowoffset = 0;
  map->sidedefs[index].toptexture[0] = '-'; // No upper texture by default
  map->sidedefs[index].toptexture[1] = '\0';
  map->sidedefs[index].bottomtexture[0] = '-'; // No lower texture by default
  map->sidedefs[index].bottomtexture[1] = '\0';
  
  strcpy(map->sidedefs[index].midtexture, "BRONZE1");
  //  map->sidedefs[index].midtexture[0] = '-'; // Default texture
  //  map->sidedefs[index].midtexture[1] = '\0';
  
  map->sidedefs[index].sector = sector_index;
  map->num_sidedefs++;
  
  return index;
}

static uint16_t add_sector(map_data_t *map) {
  // Check if we're at capacity
  if (map->num_sectors >= 65535) {
    printf("Maximum number of sectors reached!\n");
    return 0xFFFF;
  }
  
  // Resize sectors array if needed
  mapsector_t *new_sectors = realloc(map->sectors, (map->num_sectors + 1) * sizeof(mapsector_t));
  if (!new_sectors) {
    printf("Failed to allocate memory for new sector!\n");
    return 0xFFFF;
  }
  map->sectors = new_sectors;
  
  // Set sector data
  uint16_t index = map->num_sectors;
  map->sectors[index].floorheight = 0;
  map->sectors[index].ceilingheight = 128;
  
  // Set default floor and ceiling textures
  strncpy(map->sectors[index].floorpic, "FLOOR", 8);
  strncpy(map->sectors[index].ceilingpic, "CEIL", 8);
  
  map->sectors[index].lightlevel = 160;   // Default light level
  map->sectors[index].special = 0;        // No special effect
  map->sectors[index].tag = 0;            // No tag
  map->num_sectors++;
  
  return index;
}

// Calculate if points form clockwise or counter-clockwise polygon
static bool is_clockwise(int points[][2], int num_points) {
  double sum = 0.0;
  for (int i = 0; i < num_points; i++) {
    int j = (i + 1) % num_points;
    sum += (points[j][0] - points[i][0]) * (points[j][1] + points[i][1]);
  }
  return sum > 0;
}

// Update the finish_sector function to create sidedefs and a sector
void finish_sector(map_data_t *map, editor_state_t *editor) {
  if (editor->num_draw_points < 3) {
    printf("Need at least 3 points to create a sector!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // First, check if we need to reverse the points to ensure counter-clockwise winding
  // (counter-clockwise is standard for Doom engine and determines the front side)
  bool clockwise = is_clockwise(editor->draw_points, editor->num_draw_points);
  if (clockwise) {
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
  uint16_t sector_index = add_sector(map);
  if (sector_index == 0xFFFF) {
    printf("Failed to create sector, aborting sector creation!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // Add all vertices
  uint16_t vertex_indices[MAX_DRAW_POINTS];
  for (int i = 0; i < editor->num_draw_points; i++) {
    vertex_indices[i] = add_vertex(map, editor->draw_points[i][0], editor->draw_points[i][1]);
  }
  
  // Add linedefs and sidedefs
  for (int i = 0; i < editor->num_draw_points; i++) {
    int next = (i + 1) % editor->num_draw_points;
    
    // Check if a linedef already exists between these two vertices
    bool linedef_exists = false;
    uint16_t existing_linedef_index = 0;
    
    for (int j = 0; j < map->num_linedefs; j++) {
      if ((map->linedefs[j].start == vertex_indices[i] && map->linedefs[j].end == vertex_indices[next]) ||
          (map->linedefs[j].start == vertex_indices[next] && map->linedefs[j].end == vertex_indices[i])) {
        linedef_exists = true;
        existing_linedef_index = j;
        break;
      }
    }
    
    if (linedef_exists) {
      // If linedef exists, check if it's already two-sided
      maplinedef_t *linedef = &map->linedefs[existing_linedef_index];
      
      // If it's already two-sided, we have an error (can't have more than 2 sides)
      if (linedef->sidenum[0] != 0xFFFF && linedef->sidenum[1] != 0xFFFF) {
        printf("WARNING: Linedef %d already has two sides!\n", existing_linedef_index);
        continue;
      }
      
      // Check if we need to add a sidedef to front or back
      if (linedef->start == vertex_indices[i] && linedef->end == vertex_indices[next]) {
        // This is the front side (same direction)
        if (linedef->sidenum[0] == 0xFFFF) {
          linedef->sidenum[0] = add_sidedef(map, sector_index);
        }
      } else {
        // This is the back side (opposite direction)
        if (linedef->sidenum[1] == 0xFFFF) {
          linedef->sidenum[1] = add_sidedef(map, sector_index);
        }
      }
      
      // Make sure the linedef is no longer marked as impassable (it's now two-sided)
      linedef->flags &= ~1;  // Remove impassable flag
      linedef->flags |= 4;   // Set two-sided flag
    } else {
      // Create a new linedef
      uint16_t linedef_index = map->num_linedefs;
      
      // Resize linedefs array if needed
      maplinedef_t *new_linedefs = realloc(map->linedefs, (map->num_linedefs + 1) * sizeof(maplinedef_t));
      if (!new_linedefs) {
        printf("Failed to allocate memory for new linedef!\n");
        continue;
      }
      map->linedefs = new_linedefs;
      
      // Set linedef data
      map->linedefs[linedef_index].start = vertex_indices[next];
      map->linedefs[linedef_index].end = vertex_indices[i];
      map->linedefs[linedef_index].flags = 1;  // Impassable by default
      map->linedefs[linedef_index].special = 0;
      map->linedefs[linedef_index].tag = 0;
      
      // Create a front sidedef for this linedef
      map->linedefs[linedef_index].sidenum[0] = add_sidedef(map, sector_index);
      map->linedefs[linedef_index].sidenum[1] = 0xFFFF;  // No back sidedef
      
      map->num_linedefs++;
    }
  }
  
  printf("Created sector %d with %d vertices and %d walls\n",
         sector_index, editor->num_draw_points, editor->num_draw_points);
  
  // Reset drawing state
  editor->drawing = false;
  editor->num_draw_points = 0;
  
  // Rebuild vertex buffers
  build_wall_vertex_buffer(map);
  build_floor_vertex_buffer(map);
}
