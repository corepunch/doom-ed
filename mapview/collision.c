#include "map.h"
#include <math.h>

// Constants for collision detection
#define PLAYER_RADIUS 16.0f       // Player collision radius (approx. from DOOM)
#define MAX_STEP_HEIGHT 24.0f     // Maximum height player can step up (from DOOM)
#define COLLISION_EPSILON 0.1f    // Small value to prevent floating point errors
#define MIN_WALL_DISTANCE 2.0f    // Minimum distance from walls

/**
 * Structure to track collision results
 */
typedef struct {
  bool collided;        // Whether a collision was detected
  float normal_x;       // Wall normal X component
  float normal_y;       // Wall normal Y component
  float penetration;    // Penetration depth
  int linedef_index;    // Index of the colliding linedef
} collision_result_t;

/**
 * Check if a point collides with walls in the map and get collision data
 *
 * @param map The map data
 * @param x X position to check
 * @param y Y position to check
 * @param result Pointer to store collision result
 */
void check_wall_collision(map_data_t const *map, float x, float y, collision_result_t *result) {
  result->collided = false;
  result->penetration = 0.0f;
  
  float shortest_dist_squared = PLAYER_RADIUS * PLAYER_RADIUS * 4.0f; // Initialize to something larger than radius^2
  
  // Check each linedef for collision
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *line = &map->linedefs[i];
    
    // Skip if this linedef doesn't have both sides (meaning it's not a solid wall)
    if (line->sidenum[1] != 0xFFFF) {
      // Check if both sides have same sector - if yes, can pass through
      uint16_t side1 = line->sidenum[0];
      uint16_t side2 = line->sidenum[1];
      if (side1 != 0xFFFF && side2 != 0xFFFF) {
        mapsector_t *sector1 = &map->sectors[map->sidedefs[side1].sector];
        mapsector_t *sector2 = &map->sectors[map->sidedefs[side2].sector];
        
        // If both sectors have same floor and ceiling height, we can pass
        if (sector1->floorheight == sector2->floorheight &&
            sector1->ceilingheight == sector2->ceilingheight) {
          continue;
        }
        
        // If the step is acceptable, we can pass
        if (abs(sector1->floorheight - sector2->floorheight) <= MAX_STEP_HEIGHT) {
          continue;
        }
      }
    }
    
    // Get wall start and end points
    mapvertex_t const *v1 = &map->vertices[line->start];
    mapvertex_t const *v2 = &map->vertices[line->end];
    
    // Calculate line segment properties
    float wx1 = v1->x;
    float wy1 = v1->y;
    float wx2 = v2->x;
    float wy2 = v2->y;
    
    // Line direction vector
    float dx = wx2 - wx1;
    float dy = wy2 - wy1;
    float line_length_squared = dx * dx + dy * dy;
    
    // Skip very short lines to prevent division by zero
    if (line_length_squared < COLLISION_EPSILON) {
      continue;
    }
    
    // Find shortest distance from point to line
    float t = ((x - wx1) * dx + (y - wy1) * dy) / line_length_squared;
    
    // If t is outside [0,1], closest point is one of the endpoints
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Calculate closest point on line segment
    float closest_x = wx1 + t * dx;
    float closest_y = wy1 + t * dy;
    
    // Calculate distance to closest point on line
    float distance_squared =
    (x - closest_x) * (x - closest_x) +
    (y - closest_y) * (y - closest_y);
    
    // If distance is less than player radius, we have a collision
    if (distance_squared < (PLAYER_RADIUS + MIN_WALL_DISTANCE) * (PLAYER_RADIUS + MIN_WALL_DISTANCE)) {
      // If this is the closest collision so far, record it
      if (distance_squared < shortest_dist_squared) {
        shortest_dist_squared = distance_squared;
        
        // Calculate the wall normal (perpendicular to the wall, pointing away from it)
        float length = sqrt(dx * dx + dy * dy);
        if (length > COLLISION_EPSILON) {
          // Wall normal (perpendicular to wall direction)
          result->normal_x = -dy / length;
          result->normal_y = dx / length;
          
          // Make sure normal points away from player
          float dot = (x - closest_x) * result->normal_x + (y - closest_y) * result->normal_y;
          if (dot < 0) {
            result->normal_x = -result->normal_x;
            result->normal_y = -result->normal_y;
          }
          
          // Calculate penetration depth
          float distance = sqrt(distance_squared);
          result->penetration = PLAYER_RADIUS + MIN_WALL_DISTANCE - distance;
          result->linedef_index = i;
          result->collided = true;
        }
      }
    }
  }
}

/**
 * Slides a movement vector along a wall based on wall normal
 *
 * @param move_x X component of movement vector
 * @param move_y Y component of movement vector
 * @param normal_x X component of wall normal
 * @param normal_y Y component of wall normal
 * @param slide_x Pointer to store resulting X slide component
 * @param slide_y Pointer to store resulting Y slide component
 */
void calculate_slide_vector(float move_x, float move_y, float normal_x, float normal_y,
                            float *slide_x, float *slide_y) {
  // Calculate dot product of movement and normal
  float dot = move_x * normal_x + move_y * normal_y;
  
  // Slide vector = movement - (normal * dot_product)
  *slide_x = move_x - (normal_x * dot);
  *slide_y = move_y - (normal_y * dot);
  
  // Normalize the slide vector to maintain speed
  float slide_length = sqrt((*slide_x) * (*slide_x) + (*slide_y) * (*slide_y));
  if (slide_length > COLLISION_EPSILON) {
    float move_length = sqrt(move_x * move_x + move_y * move_y);
    *slide_x = (*slide_x) * move_length / slide_length;
    *slide_y = (*slide_y) * move_length / slide_length;
  }
}

/**
 * Updates player position with collision detection and wall sliding
 *
 * @param map The map data
 * @param player Player object to update
 * @param move_x X component of desired movement
 * @param move_y Y component of desired movement
 */
void update_player_position_with_sliding(map_data_t const *map, player_t *player,
                                         float move_x, float move_y) {
  // Calculate new position
  float new_x = player->x + move_x;
  float new_y = player->y + move_y;
  
  // Check for collisions at the target position
  collision_result_t result;
  check_wall_collision(map, new_x, new_y, &result);
  
  // If no collision, move directly
  if (!result.collided) {
    // Verify that it's a valid sector
    mapsector_t const *new_sector = find_player_sector(map, (int)new_x, (int)new_y);
    mapsector_t const *current_sector = find_player_sector(map, (int)player->x, (int)player->y);
    
    // Check sector transitions for step height
    if (new_sector && current_sector) {
      float floor_diff = new_sector->floorheight - current_sector->floorheight;
      
      // If step is too high, do not move
      if (floor_diff > MAX_STEP_HEIGHT) {
        return;
      }
      
      // Check ceiling clearance
      if ((new_sector->ceilingheight - new_sector->floorheight) < EYE_HEIGHT) {
        return;
      }
      
      // Safe to move
      player->x = new_x;
      player->y = new_y;
      player->z = new_sector->floorheight + EYE_HEIGHT; // Update height based on floor
    }
    return;
  }
  
  // We need to handle collision with sliding
  
  // First, resolve penetration by moving back along normal
  float resolve_x = player->x + result.normal_x * result.penetration;
  float resolve_y = player->y + result.normal_y * result.penetration;
  
  // Calculate slide vector
  float slide_x, slide_y;
  calculate_slide_vector(move_x, move_y, result.normal_x, result.normal_y, &slide_x, &slide_y);
  
  // If the slide vector is very small, don't bother
  if ((slide_x * slide_x + slide_y * slide_y) < COLLISION_EPSILON) {
    return;
  }
  
  // Try the slide movement
  new_x = player->x + slide_x;
  new_y = player->y + slide_y;
  
  // Check for collision with slide vector
  collision_result_t slide_result;
  check_wall_collision(map, new_x, new_y, &slide_result);
  
  if (!slide_result.collided) {
    // Verify that it's a valid sector
    mapsector_t const *new_sector = find_player_sector(map, (int)new_x, (int)new_y);
    mapsector_t const *current_sector = find_player_sector(map, (int)player->x, (int)player->y);
    
    // Check sector transitions for step height
    if (new_sector && current_sector) {
      float floor_diff = new_sector->floorheight - current_sector->floorheight;
      
      // If step is too high, do not move
      if (floor_diff > MAX_STEP_HEIGHT) {
        return;
      }
      
      // Check ceiling clearance
      if ((new_sector->ceilingheight - new_sector->floorheight) < EYE_HEIGHT) {
        return;
      }
      
      // Safe to slide
      player->x = new_x;
      player->y = new_y;
      player->z = new_sector->floorheight + EYE_HEIGHT; // Update height based on floor
    }
  } else {
    // If we still collide after sliding, try a smaller slide (50%)
    new_x = player->x + slide_x * 0.5f;
    new_y = player->y + slide_y * 0.5f;
    
    collision_result_t small_slide_result;
    check_wall_collision(map, new_x, new_y, &small_slide_result);
    
    if (!small_slide_result.collided) {
      // Verify that it's a valid sector
      mapsector_t const *new_sector = find_player_sector(map, (int)new_x, (int)new_y);
      mapsector_t const *current_sector = find_player_sector(map, (int)player->x, (int)player->y);
      
      // Check sector transitions
      if (new_sector && current_sector) {
        float floor_diff = new_sector->floorheight - current_sector->floorheight;
        
        // If step is too high, do not move
        if (floor_diff > MAX_STEP_HEIGHT) {
          return;
        }
        
        // Check ceiling clearance
        if ((new_sector->ceilingheight - new_sector->floorheight) < EYE_HEIGHT) {
          return;
        }
        
        // Safe to slide at 50%
        player->x = new_x;
        player->y = new_y;
        player->z = new_sector->floorheight + EYE_HEIGHT; // Update height based on floor
      }
    }
  }
}
