#ifndef __LIBGAME_WAD_H__
#define __LIBGAME_WAD_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Type definitions
typedef char wadid_t[4];     // "IWAD" or "PWAD"
typedef char lumpname_t[8];  // Lump name, null-terminated
typedef char texname_t[8];   // Texture name, null-terminated

// WAD header structure
typedef struct {
  wadid_t identification;     // Should be "IWAD" or "PWAD"
  uint32_t numlumps;          // Number of lumps in the WAD
  uint32_t infotableofs;      // Pointer to the location of the directory
} wadheader_t;

// WAD directory entry structure
typedef struct {
  uint32_t filepos;           // File offset of the lump
  uint32_t size;              // Size of the lump in bytes
  lumpname_t name;            // Name of the lump, null-terminated
} filelump_t;

// Patch header structure
typedef struct {
  int16_t width;              // Width of the patch
  int16_t height;             // Height of the patch
  int16_t leftoffset;         // Left offset of the patch
  int16_t topoffset;          // Top offset of the patch
  int32_t columnofs[1];       // Column offsets (variable size)
} patch_t;

// Palette structure
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} palette_entry_t;

// Texture definition
typedef struct {
  char const *name;
  uint16_t width;
  uint16_t height;
} texdef_t;

extern palette_entry_t *palette;

// WAD file functions
bool init_wad(const char *filename);
void shutdown_wad(void);
filelump_t *find_lump(const char* name);
int find_lump_num(const char* name);
char const *get_lump_name(int i);
void *cache_lump_num(uint16_t i);
void *cache_lump(const char* name);
void* read_lump_data(FILE* file, filelump_t const *lump);
bool is_map_block_valid(filelump_t* dir, int index, int total_lumps);
void find_all_maps(void (*proc)(const char *, void *), void *parm);

#endif // __LIBGAME_WAD_H__
