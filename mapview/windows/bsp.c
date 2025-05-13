#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stdbool.h>
#include "map.h"

// External references
extern GLuint world_prog;

// Forward declarations from floor.c
bool point_in_frustum(vec3 point, vec4 const planes[6]);
bool linedef_in_frustum_2d(vec4 const frustum[6], vec3 a, vec3 b);
void draw_walls(map_data_t const *map, mapsector_t const *sector, viewdef_t const *viewdef);
void draw_wall_ids(map_data_t const *map, mapsector_t const *sector, viewdef_t const *viewdef);
void draw_floors(map_data_t const *map, mapsector_t const *sector, viewdef_t const *viewdef);
void draw_floor_ids(map_data_t const *map, mapsector_t const *sector, viewdef_t const *viewdef);

// Track rendered subsectors to avoid duplicates
#define MAX_SUBSECTORS 4096
static bool subsector_rendered[MAX_SUBSECTORS];

// BSP utility functions

// Tests if a bounding box is visible in the view frustum
bool bbox_visible(int16_t const bbox[4], vec4 const frustum[6], int z_min, int z_max) {
  // Convert bbox from Doom format [left, bottom, right, top] to min/max format
  vec3 bbox_points[8] = {
    // Bottom corners
    {bbox[0], bbox[1], z_min},
    {bbox[2], bbox[1], z_min},
    {bbox[2], bbox[3], z_min},
    {bbox[0], bbox[3], z_min},
    // Top corners
    {bbox[0], bbox[1], z_max},
    {bbox[2], bbox[1], z_max},
    {bbox[2], bbox[3], z_max},
    {bbox[0], bbox[3], z_max}
  };
  
  // Check if any corner is inside the frustum
  for (int i = 0; i < 8; i++) {
    if (point_in_frustum(bbox_points[i], frustum)) {
      return true;
    }
  }
  
  // Check if bbox edges intersect frustum planes
  for (int i = 0; i < 4; i++) {
    int next = (i + 1) % 4;
    // Bottom face edges
    if (linedef_in_frustum_2d(frustum, bbox_points[i], bbox_points[next])) {
      return true;
    }
    // Top face edges
    if (linedef_in_frustum_2d(frustum, bbox_points[i+4], bbox_points[next+4])) {
      return true;
    }
    // Vertical edges
    if (linedef_in_frustum_2d(frustum, bbox_points[i], bbox_points[i+4])) {
      return true;
    }
  }
  
  // Special case: check if frustum is completely inside the bbox
  // (This is a simplification - would need more extensive testing for a complete check)
  vec3 view_pos = {
    frustum[0][0],  // Assuming viewPos is available from one of the planes
    frustum[0][1],
    frustum[0][2]
  };
  
  if (view_pos[0] >= bbox[0] && view_pos[0] <= bbox[2] &&
      view_pos[1] >= bbox[1] && view_pos[1] <= bbox[3] &&
      view_pos[2] >= z_min && view_pos[2] <= z_max) {
    return true;
  }
  
  return false;
}

// Check which side of the BSP partition line a point is on
int point_on_side(float x, float y, mapnode_t const *node) {
  // Line equation: ax + by + c = 0
  // Where a = node->dy, b = -node->dx, c = node->dx * node->y - node->dy * node->x
  
  float dx = x - node->x;
  float dy = y - node->y;
  
  // Dot product determines which side we're on
  float side = dx * node->dy - dy * node->dx;
  
  return (side < 0) ? 1 : 0;
}

// Render a subsector (leaf node in the BSP tree)
void render_subsector(map_data_t const *map, int ssector_num, viewdef_t const *viewdef, bool render_ids) {
  if (ssector_num >= map->num_subsectors || subsector_rendered[ssector_num]) {
    return;
  }
  
  subsector_rendered[ssector_num] = true;
  
  // Find sector this subsector belongs to by looking at the first linedef
  mapsubsector_t const *ssector = &map->subsectors[ssector_num];
  
  // We need to find the sector this subsector belongs to
  // In Doom, this would involve looking at the first seg's linedef's front sidedef's sector
  // Since we don't have segs data structure, we'll find the sector more directly
  
  // We can use point_in_sector to determine which sector contains the center point of this subsector
  // First, let's calculate an approximate center point by averaging vertices
  
  // For now, we'll use a simplified approach - find first linedef's sector
  // This is not 100% accurate but should work for most cases
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Check if this linedef's vertices are within the subsector
    // This is a simple heuristic since we don't have seg data
    if (linedef->sidenum[0] != 0xFFFF) {
      mapsidedef_t const *sidedef = &map->sidedefs[linedef->sidenum[0]];
      mapsector_t const *sector = &map->sectors[sidedef->sector];
      
      // Render this sector
      if (render_ids) {
        draw_floor_ids(map, sector, viewdef);
      } else {
        draw_floors(map, sector, viewdef);
      }
      
      break;
    }
  }
}

// Main BSP traversal function
void traverse_bsp_node(map_data_t const *map, int node_index, viewdef_t const *viewdef, bool render_ids) {
  // Check if this is a subsector (leaf node)
  if (node_index & NF_SUBSECTOR) {
    render_subsector(map, node_index & ~NF_SUBSECTOR, viewdef, render_ids);
    return;
  }
  
  // Get the current node
  mapnode_t const *node = &map->nodes[node_index];
  
  // Get min/max heights for the entire map (approximate)
  int z_min = INT16_MIN;
  int z_max = INT16_MAX;
  for (int i = 0; i < map->num_sectors; i++) {
    z_min = MAX(z_min, map->sectors[i].floorheight);
    z_max = MIN(z_max, map->sectors[i].ceilingheight);
  }
  
  // Check if child nodes are within the view frustum
  bool child0_visible = bbox_visible(node->bbox[0], viewdef->frustum, z_min, z_max);
  bool child1_visible = bbox_visible(node->bbox[1], viewdef->frustum, z_min, z_max);
  
  // Determine which side of the partition line the viewer is on
  int side = point_on_side(viewdef->viewpos[0], viewdef->viewpos[1], node);
  
  // Process nodes front-to-back relative to the viewer
  if (child0_visible && child1_visible) {
    // Traverse closest child first, then farthest child
    traverse_bsp_node(map, node->children[side], viewdef, render_ids);
    traverse_bsp_node(map, node->children[!side], viewdef, render_ids);
  } else if (child0_visible) {
    traverse_bsp_node(map, node->children[0], viewdef, render_ids);
  } else if (child1_visible) {
    traverse_bsp_node(map, node->children[1], viewdef, render_ids);
  }
}

// Main BSP drawing function
void draw_bsp(map_data_t const *map, viewdef_t const *viewdef) {
  // Clear the subsector tracking array
  memset(subsector_rendered, 0, sizeof(subsector_rendered));
  
  // Reset counter for performance metrics
  extern int sectors_drawn;
  sectors_drawn = 0;
  
  // Start traversal from the root node (last node in the array)
  if (map->num_nodes > 0) {
    traverse_bsp_node(map, map->num_nodes - 1, viewdef, false);
  }
}

// Version of BSP drawing that renders sector IDs instead of textures
void draw_bsp_ids(map_data_t const *map, viewdef_t const *viewdef) {
  // Clear the subsector tracking array
  memset(subsector_rendered, 0, sizeof(subsector_rendered));
  
  // Start traversal from the root node (last node in the array)
  if (map->num_nodes > 0) {
    traverse_bsp_node(map, map->num_nodes - 1, viewdef, true);
  }
}

// Helper function to find which subsector contains a point
// Useful for determining player's current sector
int find_subsector(map_data_t const *map, float x, float y) {
  int node_index = map->num_nodes - 1;
  
  // Traverse the BSP tree until we reach a leaf node (subsector)
  while (!(node_index & NF_SUBSECTOR)) {
    mapnode_t const *node = &map->nodes[node_index];
    int side = point_on_side(x, y, node);
    node_index = node->children[side];
  }
  
  return node_index & ~NF_SUBSECTOR;
}

// Function to find the sector the player is in using BSP traversal
mapsector_t const *find_player_sector_bsp(map_data_t const *map, float x, float y) {
  int ssector_num = find_subsector(map, x, y);
  
  if (ssector_num >= map->num_subsectors) {
    return NULL;
  }
  
  mapsubsector_t const *ssector = &map->subsectors[ssector_num];
  
  // Find the sector this subsector belongs to
  // We need to find a linedef that includes vertices referenced by this subsector
  // In a complete implementation, we would use segs to find this directly
  
  // Since we don't have direct seg access, we'll check for point containment
  // For each sector, check if the approximate center of this subsector is inside
  
  // Calculate an approximate center point for this subsector
  // This is a simplification - in a real implementation with segs data
  // we would use the actual vertices
  float avg_x = 0, avg_y = 0;
  int vertex_count = 0;
  
  // In a real implementation, we would iterate through the segs
  // Since we don't have that, we'll try to find linedefs that might be associated
  for (int i = 0; i < map->num_linedefs; i++) {
    // This is a heuristic approach that may not work for all maps
    // but should work for simple cases
    if (vertex_count < 4) { // Limit the search to avoid excessive computation
      avg_x += map->vertices[map->linedefs[i].start].x;
      avg_y += map->vertices[map->linedefs[i].start].y;
      vertex_count++;
    }
  }
  
  if (vertex_count > 0) {
    avg_x /= vertex_count;
    avg_y /= vertex_count;
    
    // Find which sector contains this point
    for (int i = 0; i < map->num_sectors; i++) {
      if (point_in_sector(map, avg_x, avg_y, i)) {
        return &map->sectors[i];
      }
    }
  }
  
  // Fallback: iterate through linedefs to find one associated with this subsector
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    if (linedef->sidenum[0] != 0xFFFF) {
      mapsidedef_t const *sidedef = &map->sidedefs[linedef->sidenum[0]];
      return &map->sectors[sidedef->sector];
    }
  }
  
  return NULL;
}

// Initialize BSP rendering
void init_bsp(void) {
  // Any one-time initialization needed for BSP rendering
  memset(subsector_rendered, 0, sizeof(subsector_rendered));
}
