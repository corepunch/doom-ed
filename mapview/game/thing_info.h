//
//  thing_info.h
//  mapview
//
//  Game-specific thing information interface
//  This provides an abstraction layer between the editor and DOOM/Hexen game data
//

#ifndef __THING_INFO_H__
#define __THING_INFO_H__

#include <stdint.h>
#include <stdbool.h>

// Forward declarations to avoid including sprites.h
typedef struct sprite_s sprite_t;

/**
 * Get the sprite for a given thing type and angle
 * 
 * @param thing_type The DOOM/Hexen thing type (doomednum)
 * @param angle The viewing angle (0-7 for 8 directions)
 * @return Pointer to sprite data, or NULL if not found
 */
sprite_t *game_get_thing_sprite(uint16_t thing_type, uint16_t angle);

/**
 * Get the number of sprite types in the current game
 * 
 * @return Number of sprites defined in the game
 */
int game_get_num_sprites(void);

/**
 * Get the sprite name prefix for a given sprite index
 * 
 * @param sprite_index Index into the sprite table
 * @return Pointer to 4-character sprite name (e.g., "TROO"), or NULL if invalid
 */
const char *game_get_sprite_name(int sprite_index);

/**
 * Check if a thing type is a player start position
 * 
 * @param thing_type The thing type to check
 * @return true if this is a player start, false otherwise
 */
bool game_is_player_start(uint16_t thing_type);

/**
 * Initialize the game-specific thing information system
 * This loads sprite frames and sets up lookup tables
 * 
 * @return true on success, false on failure
 */
bool game_init_thing_info(void);

#endif /* __THING_INFO_H__ */
