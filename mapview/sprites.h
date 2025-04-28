#ifndef __SPRITES__
#define __SPRITES__

#include <stdio.h>
#include <stdbool.h>
#include <OpenGL/gl3.h>

// Initialize sprite system
bool init_sprites(map_data_t *, FILE* wad_file, filelump_t* directory, int num_lumps);

// Draw a sprite at the specified screen position
void draw_sprite(const char* name, float x, float y, float scale, float alpha);

// Draw the weapon sprite at the bottom center of screen
void draw_weapon(void);

// Clean up sprite system resources
void cleanup_sprites(void);

// Draw a crosshair in the center of the screen
void draw_crosshair(void);

// Helper function to find a lump directly from file
int find_lump_from_file(FILE* wad_file, const char* name);

#endif /* __SPRITES__ */
