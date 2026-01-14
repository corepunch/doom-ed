//
//  thing_info.c
//  mapview
//
//  DOOM/Hexen-specific thing information implementation
//  This file isolates all game-specific data dependencies
//

#include "thing_info.h"
#include "../sprites.h"
#include <string.h>
#include <stdio.h>

// Include game-specific headers
#ifdef HEXEN
#include "../../hexen/info.h"
#else
#include "../../doom/info.h"
#endif

// Import external game data from info.c
extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
extern state_t states[NUMSTATES];
extern char *sprnames[NUMSPRITES];

// Sprite frame data structures
typedef struct {
  bool rotate;
  sprite_t *angle[8];
  bool flip[8];
} spriteframe_t;

typedef struct  {
  int num_frames;
  spriteframe_t spriteframes[24];
} spritedef_t;

// Static sprite database
static spritedef_t sprites[NUMSPRITES];

// Get the sprite for a given thing type and angle
sprite_t *game_get_thing_sprite(uint16_t thing_type, uint16_t angle) {
  static sprite_t empty = {
    .width = 8,
    .height = 8,
    .texture = 1,
  };
  
  for (int i = 0; i < NUMMOBJTYPES; i++) {
    if (mobjinfo[i].doomednum == thing_type) {
      state_t const *s = &states[mobjinfo[i].spawnstate];
      spritedef_t const *sdef = &sprites[s->sprite];
      uint16_t frame = s->frame & 0x7fff;
      
      if (sdef->num_frames == 0) {
        return &empty;
      }
      
      if (sdef->num_frames <= frame) {
        frame = 0;
      }
      
      spriteframe_t const *sf = &sdef->spriteframes[frame];
      sprite_t *sprite = sf->rotate ? sf->angle[angle % 8] : sf->angle[0];
      return sprite ? sprite : &empty;
    }
  }
  
  return &empty;
}

// Get the number of sprite types in the current game
int game_get_num_sprites(void) {
  return NUMSPRITES;
}

// Get the sprite name prefix for a given sprite index
const char *game_get_sprite_name(int sprite_index) {
  if (sprite_index < 0 || sprite_index >= NUMSPRITES) {
    return NULL;
  }
  return sprnames[sprite_index];
}

// Check if a thing type is a player start position
bool game_is_player_start(uint16_t thing_type) {
#ifdef HEXEN
  return thing_type == MT_MAPSPOT;
#else
  return thing_type == MT_PLAYER;
#endif
}

// Initialize the game-specific thing information system
bool game_init_thing_info(void) {
  extern sprite_t* find_sprite6(const char* name);
  
  memset(sprites, 0, sizeof(sprites));
  
  for (int i = 0; i < NUMSPRITES; i++) {
    // Determine angle frame (A-H based on 45-degree increments)
    // Doom uses 8 angle sprites, with 0 being east, 45 northeast, etc.
    for (int j = 0; j < 16; j++) {
      char frame_char = 'A' + j;
      char sprite_name[8] = {0};
      const char *prefix = sprnames[i];
      
      // Format sprite name (typically PREFIX + FRAME + ANGLE, e.g., "TROOA1")
      snprintf(sprite_name, sizeof(sprite_name), "%s%c1", prefix, frame_char);
      
      if (find_sprite6(sprite_name)) {
        for (int k = 0; k < 8; k++) {
          snprintf(sprite_name, sizeof(sprite_name), "%s%c%d", prefix, frame_char, k+1);
          sprites[i].spriteframes[j].angle[k] = find_sprite6(sprite_name);
        }
        sprites[i].spriteframes[j].rotate = true;
      } else {
        snprintf(sprite_name, sizeof(sprite_name), "%s%c0", prefix, frame_char);
        sprites[i].spriteframes[j].angle[0] = find_sprite6(sprite_name);
      }
      
      if (!sprites[i].spriteframes[j].angle[0]) {
        break;
      }
      
      sprites[i].num_frames++;
    }
  }
  
  return true;
}
