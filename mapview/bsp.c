#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stdbool.h>
#include <math.h>
#include "map.h"

// Forward declarations
void draw_walls(map_data_t const *map,
                mapsector_t const *sector,
                viewdef_t const *viewdef);

//
// R_PointOnSide
// Traverse a BSP node.
// Returns true if point is on back side.
//
static int R_PointOnSide(float x, float y, mapnode_t const *node) {
  float dx, dy;
  float left, right;
  
  if (!node->dx) {
    if (x <= node->x)
      return node->dy > 0;
    return node->dy < 0;
  }
  
  if (!node->dy) {
    if (y <= node->y)
      return node->dx < 0;
    return node->dx > 0;
  }
  
  dx = (x - node->x);
  dy = (y - node->y);
  
  // Cross product to determine which side
  left = node->dy * dx;
  right = dy * node->dx;
  
  if (right < left)
    return 0;  // front side
  return 1;    // back side
}

// Bounding box coordinates (Doom standard)
enum {
  BOXTOP,
  BOXBOTTOM,
  BOXLEFT,
  BOXRIGHT
};

//
// checkcoord lookup table
// Based on original Doom source code
// Maps box position to the corners that define visibility edges
//
static int checkcoord[12][4] = {
  {3,0,2,1},
  {3,0,2,0},
  {3,1,2,0},
  {0},
  {2,0,2,1},
  {0,0,0,0},
  {3,1,3,0},
  {0},
  {2,0,3,1},
  {2,1,3,1},
  {2,1,3,0}
};

//
// R_PointToAngle
// Calculate angle from point 1 to point 2
//
static float R_PointToAngle(float x1, float y1, float x2, float y2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  
  if (dx == 0 && dy == 0)
    return 0;
  
  return atan2f(dy, dx);
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box using Doom's algorithm.
// Returns true if some part of the bbox might be visible.
//
static bool R_CheckBBox(float viewx, float viewy, float viewangle, int16_t const *bbox) {
  int boxx, boxy;
  int boxpos;
  
  float x1, y1, x2, y2;
  float angle1, angle2;
  float span, angle;
  
  // Determine which box corners to check based on view position
  // This is the same logic as original Doom
  if (viewx <= bbox[BOXLEFT])
    boxx = 0;
  else if (viewx < bbox[BOXRIGHT])
    boxx = 1;
  else
    boxx = 2;
  
  if (viewy >= bbox[BOXTOP])
    boxy = 0;
  else if (viewy > bbox[BOXBOTTOM])
    boxy = 1;
  else
    boxy = 2;
  
  boxpos = (boxy << 2) + boxx;
  
  // If view point is inside the box, it's definitely visible
  if (boxpos == 5)
    return true;
  
  // Get the corners of the box that matter for this view position
  x1 = bbox[checkcoord[boxpos][0]];
  y1 = bbox[checkcoord[boxpos][1]];
  x2 = bbox[checkcoord[boxpos][2]];
  y2 = bbox[checkcoord[boxpos][3]];
  
  // Calculate angles to the corners relative to view direction
  angle1 = R_PointToAngle(viewx, viewy, x1, y1) - viewangle;
  angle2 = R_PointToAngle(viewx, viewy, x2, y2) - viewangle;
  
  // Normalize angles to -PI to PI range
  while (angle1 > M_PI) angle1 -= 2.0f * M_PI;
  while (angle1 < -M_PI) angle1 += 2.0f * M_PI;
  while (angle2 > M_PI) angle2 -= 2.0f * M_PI;
  while (angle2 < -M_PI) angle2 += 2.0f * M_PI;
  
  span = angle1 - angle2;
  
  // Box is completely behind view plane
  if (span >= M_PI)
    return true;
  
  // Use a 90-degree FOV for clipping (can be adjusted)
  float clipangle = M_PI / 2.0f;  // 90 degrees
  
  // Check if the box is completely outside the field of view
  // Both corners to the left
  if (angle1 > clipangle && angle2 > clipangle)
    return false;
  
  // Both corners to the right
  if (angle1 < -clipangle && angle2 < -clipangle)
    return false;
  
  // Box is at least partially visible
  return true;
}

//
// R_Subsector
// Draw all segs in a subsector.
//
static void R_Subsector(map_data_t const *map, int num, viewdef_t const *viewdef) {
  if (num >= map->num_subsectors)
    return;
  
  mapsubsector_t const *sub = &map->subsectors[num];
  
  // Get the first seg to determine which sector this subsector belongs to
  if (sub->numsegs == 0)
    return;
  
  mapseg_t const *seg = &map->segs[sub->firstseg];
  
  // Find the sector this subsector belongs to
  if (seg->linedef >= map->num_linedefs)
    return;
  
  maplinedef_t const *linedef = &map->linedefs[seg->linedef];
  if (linedef->sidenum[seg->side] >= map->num_sidedefs)
    return;
  
  mapsidedef_t const *sidedef = &map->sidedefs[linedef->sidenum[seg->side]];
  if (sidedef->sector >= map->num_sectors)
    return;
  
  mapsector_t const *sector = &map->sectors[sidedef->sector];
  
  // Check if we've already drawn this sector in this frame
  uint32_t sector_index = sector - map->sectors;
  if (map->floors.sectors[sector_index].frame == viewdef->frame)
    return;
  
  // Mark this sector as drawn in this frame
  map->floors.sectors[sector_index].frame = viewdef->frame;
  
  // Draw the sector's floor and ceiling
  extern GLuint world_prog;
  extern int world_prog_mvp;
  extern int world_prog_viewPos;
  extern int pixel;
  
  glDisable(GL_BLEND);
  glBindVertexArray(map->floors.vao);
  
  glUniformMatrix4fv(world_prog_mvp, 1, GL_FALSE, viewdef->mvp[0]);
  glUniform3fv(world_prog_viewPos, 1, viewdef->viewpos);
  
  mapsector2_t const *sec = &map->floors.sectors[sector_index];
  float light = sector->lightlevel / 255.0f;
  
  // Draw floor
  glCullFace(GL_BACK);
  if (CHECK_PIXEL(pixel, FLOOR, sector_index)) {
    draw_textured_surface(&sec->floor, HIGHLIGHT(light), GL_TRIANGLES);
  } else {
    draw_textured_surface(&sec->floor, light, GL_TRIANGLES);
  }
  
  // Draw ceiling
  glCullFace(GL_FRONT);
  if (CHECK_PIXEL(pixel, CEILING, sector_index)) {
    draw_textured_surface(&sec->ceiling, HIGHLIGHT(light), GL_TRIANGLES);
  } else {
    draw_textured_surface(&sec->ceiling, light, GL_TRIANGLES);
  }
  glCullFace(GL_BACK);
  
  // Draw walls for this sector
  draw_walls(map, sector, viewdef);
}

//
// R_RenderBSPNode
// Renders all subsectors below a given node,
// traversing subtree recursively.
//
static void R_RenderBSPNode(map_data_t const *map, int bspnum, viewdef_t const *viewdef) {
  // Check for leaf node (subsector)
  if (bspnum & NF_SUBSECTOR) {
    int subsector_num;
    if (bspnum == -1)
      subsector_num = 0;
    else
      subsector_num = bspnum & (~NF_SUBSECTOR);
    
    R_Subsector(map, subsector_num, viewdef);
    return;
  }
  
  // Not a leaf - it's a BSP node
  if (bspnum >= map->num_nodes)
    return;
  
  mapnode_t const *bsp = &map->nodes[bspnum];
  
  // Decide which side the view point is on
  int side = R_PointOnSide(viewdef->viewpos[0], viewdef->viewpos[1], bsp);
  
  // Recursively divide front space (side we're on)
  R_RenderBSPNode(map, bsp->children[side], viewdef);
  
  // Check if back space might be visible
  // Convert player angle from degrees to radians for the check
  float viewangle_rad = viewdef->player.angle * M_PI / 180.0f;
  if (R_CheckBBox(viewdef->viewpos[0], viewdef->viewpos[1], viewangle_rad, bsp->bbox[side^1])) {
    // Recursively divide back space
    R_RenderBSPNode(map, bsp->children[side^1], viewdef);
  }
}

//
// has_bsp_data
// Check if the map has valid BSP data for rendering.
//
static inline bool has_bsp_data(map_data_t const *map) {
  return map->num_nodes > 0 && map->num_subsectors > 0 && map->num_segs > 0;
}

//
// draw_bsp
// Main entry point for BSP-based rendering.
// Traverses the BSP tree and draws all visible sectors.
//
void draw_bsp(map_data_t const *map, viewdef_t const *viewdef) {
  if (!map || !viewdef)
    return;
  
  // Check if we have BSP data
  if (!has_bsp_data(map)) {
    // No BSP data - fall back to portal-based rendering
    // This happens with manually created maps
    extern void draw_floors(map_data_t const *, mapsector_t const *, viewdef_t const *);
    
    // Find the player's sector and start rendering from there
    mapsector_t const *player_sector = NULL;
    for (int i = 0; i < map->num_sectors; i++) {
      if (point_in_sector(map, viewdef->viewpos[0], viewdef->viewpos[1], i)) {
        player_sector = &map->sectors[i];
        break;
      }
    }
    
    // Fall back to rendering from first sector if player sector not found
    if (!player_sector && map->num_sectors > 0) {
      player_sector = &map->sectors[0];
    }
    
    if (player_sector) {
      draw_floors(map, player_sector, viewdef);
    }
    return;
  }
  
  // Start traversal from the root node (last node in the array)
  R_RenderBSPNode(map, map->num_nodes - 1, viewdef);
}
