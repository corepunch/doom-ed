#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <string.h>
#include <ctype.h>
#include "map.h"

#define MAX_TEXTURES 256
#define MAX_TEXDIR 8

// Palette structure
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} palette_entry_t;

// PNAMES lump structure
typedef struct {
  int32_t nummappatches;      // Number of patches
  lumpname_t name[1];         // Array of patch names (variable size)
} mappatchnames_t;

// Patch header structure
typedef struct {
  int16_t width;              // Width of the patch
  int16_t height;             // Height of the patch
  int16_t leftoffset;         // Left offset of the patch
  int16_t topoffset;          // Top offset of the patch
  int32_t columnofs[1];       // Column offsets (variable size)
} patch_t;

// Post (column segment) structure
typedef struct {
  uint8_t topdelta;           // Top delta
  uint8_t length;             // Length of the post
                              // followed by length bytes of data and a 0 byte
} post_t;

// Structure for a patch reference in a texture
typedef struct {
  int16_t originx;            // X offset
  int16_t originy;            // Y offset
  int16_t patch;              // Patch number (index into PNAMES)
  int16_t stepdir;            // Step direction (unused)
  int16_t colormap;           // Colormap (unused)
} mappatch_t;

// Structure for a texture definition in TEXTURE1/2
typedef struct {
  lumpname_t name;            // Texture name
  uint32_t masked;            // Masked flag (bit 3)
  uint16_t width;             // Width of the texture
  uint16_t height;            // Height of the texture
  uint32_t columndirectory;   // Column directory (unused, obsolete)
  uint16_t patchcount;        // Number of patches in this texture
  mappatch_t patches[1];      // Array of patches (variable size)
} maptexture_t;

// Texture directory structure for TEXTURE1/2
typedef struct {
  uint32_t numtextures;       // Number of textures
  uint32_t *offsets;          // Array of offsets to maptexture_t
  filelump_t *lump;           // Pointer to the lump containing this directory
} texture_directory_t;

// Collection to store loaded flat textures
typedef struct flat_cache_s {
  mapside_texture_t textures[MAX_TEXTURES];
  int num_textures;
} flat_cache_t;

// Collection to store loaded textures
typedef struct texture_cache_s {
  mapside_texture_t textures[MAX_TEXTURES];
  int num_textures;
} texture_cache_t;

flat_cache_t g_flat_cache = {0};
texture_cache_t g_cache = {0};

// Load a single patch and return its data
uint8_t* load_patch(FILE* wad_file, filelump_t* patch_lump, int* width, int* height) {
  if (!patch_lump || patch_lump->size <= 0) return NULL;
  
  // Seek to patch data
  fseek(wad_file, patch_lump->filepos, SEEK_SET);
  
  // Read patch header
  patch_t header;
  fread(&header, 8, 1, wad_file); // Read fixed part of header (width, height, offsets)
  
  *width = header.width;
  *height = header.height;
  
  // Allocate memory for column offsets
  int32_t* columnofs = malloc(header.width * sizeof(int32_t));
  if (!columnofs) return NULL;
  
  // Read column offsets
  fread(columnofs, sizeof(int32_t), header.width, wad_file);
  
  // Allocate memory for the patch data (RGBA)
  uint8_t* patch_data = calloc(header.width * header.height * 4, 1);
  if (!patch_data) {
    free(columnofs);
    return NULL;
  }
  
  // Process each column
  for (int x = 0; x < header.width; x++) {
    // Seek to the start of this column's data
    fseek(wad_file, patch_lump->filepos + columnofs[x], SEEK_SET);
    
    // Process posts in this column
    while (1) {
      uint8_t topdelta;
      fread(&topdelta, 1, 1, wad_file);
      
      // 0xFF indicates end of column
      if (topdelta == 0xFF) break;
      
      uint8_t length;
      fread(&length, 1, 1, wad_file);
      
      // Skip unused byte
      fseek(wad_file, 1, SEEK_CUR);
      
      // Read post data
      for (int y = 0; y < length; y++) {
        uint8_t color;
        fread(&color, 1, 1, wad_file);
        
        // Store in RGBA format (alpha set below)
        int pos = ((topdelta + y) * header.width + x) * 4;
        patch_data[pos] = color;  // Store color index in red channel temporarily
        patch_data[pos + 3] = 255; // Make pixel opaque
      }
      
      // Skip unused byte
      fseek(wad_file, 1, SEEK_CUR);
    }
  }
  
  free(columnofs);
  return patch_data;
}

// Create a GL texture from the given texture definition and patches
GLuint create_texture_from_definition(FILE* wad_file, filelump_t* directory, int num_lumps,
                                      maptexture_t* tex_def, mappatchnames_t* pnames,
                                      const palette_entry_t* palette) {
  int width = tex_def->width;
  int height = tex_def->height;
  
  // Allocate memory for the composed texture (RGBA)
  uint8_t* texture_data = calloc(width * height * 4, 1);
  if (!texture_data) return 0;
  
  // Process each patch in the texture
  for (int i = 0; i < tex_def->patchcount; i++) {
    mappatch_t* patch_ref = &tex_def->patches[i];
    
    // Get patch name from PNAMES
    if (patch_ref->patch >= pnames->nummappatches) {
      free(texture_data);
      return 0;
    }
    
    char* patch_name = pnames->name[patch_ref->patch];
    
    // Find patch lump
    int patch_index = find_lump(directory, num_lumps, patch_name);
    if (patch_index < 0) {
      printf("Warning: Could not find patch: %s\n", patch_name);
      continue;
    }
    
    filelump_t* patch_lump = &directory[patch_index];
    
    // Load patch data
    int patch_width, patch_height;
    uint8_t* patch_data = load_patch(wad_file, patch_lump, &patch_width, &patch_height);
    if (!patch_data) continue;
    
    // Composite patch into texture at specified position
    int originx = patch_ref->originx;
    int originy = patch_ref->originy;
    
    for (int y = 0; y < patch_height; y++) {
      int tex_y = originy + y;
      if (tex_y < 0 || tex_y >= height) continue;
      
      for (int x = 0; x < patch_width; x++) {
        int tex_x = originx + x;
        if (tex_x < 0 || tex_x >= width) continue;
        
        int patch_pos = (y * patch_width + x) * 4;
        int tex_pos = (tex_y * width + tex_x) * 4;
        
        // Only copy if pixel is opaque (has alpha = 255)
        if (patch_data[patch_pos + 3] == 255) {
          uint8_t color_index = patch_data[patch_pos];
          
          // Convert color index to RGB using palette
          texture_data[tex_pos] = palette[color_index].r;
          texture_data[tex_pos + 1] = palette[color_index].g;
          texture_data[tex_pos + 2] = palette[color_index].b;
          texture_data[tex_pos + 3] = 255; // Opaque
//          texture_data[tex_pos + 3] = color_index > 220 ? 0 : 255;
        }
      }
    }
    
    free(patch_data);
  }
  
  // Create OpenGL texture
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
  
  glGenerateMipmap(GL_TEXTURE_2D);
  
  free(texture_data);
  return tex;
}

// Read texture definitions from TEXTURE1/TEXTURE2 lumps
bool load_texture_directory(FILE* wad_file,
                            filelump_t* lump,
                            texture_directory_t *directory)
{
  if (!lump || lump->size <= 0) return NULL;
  
  fseek(wad_file, lump->filepos, SEEK_SET);
  
  // Read number of textures
  fread(&directory->numtextures, sizeof(uint32_t), 1, wad_file);
  
  // Allocate memory for offsets
  directory->offsets = malloc(directory->numtextures * sizeof(uint32_t));
  if (!directory->offsets) {
    free(directory);
    return false;
  }
  
  // Read offsets
  fread(directory->offsets, sizeof(uint32_t), directory->numtextures, wad_file);
  
  // Store the lump pointer
  directory->lump = lump;
  
  return true;
}

// Find a texture in the directory by name
maptexture_t* find_texture(FILE* wad_file, texture_directory_t* directory, texname_t name) {
  if (!directory || !directory->lump) return NULL;
  
  for (uint32_t i = 0; i < directory->numtextures; i++) {
    fseek(wad_file, directory->lump->filepos + directory->offsets[i], SEEK_SET);
    
    maptexture_t tex_header;
    fread(&tex_header, sizeof(maptexture_t) - sizeof(mappatch_t), 1, wad_file);

    if (strncmp(tex_header.name, name, sizeof(texname_t)) == 0) {
      // Found texture, allocate memory and read full texture definition
      int size = sizeof(maptexture_t) - sizeof(mappatch_t) + tex_header.patchcount * sizeof(mappatch_t);
      maptexture_t* tex = malloc(size);
      if (!tex) return NULL;
      
      fseek(wad_file, directory->lump->filepos + directory->offsets[i], SEEK_SET);
      fread(tex, size, 1, wad_file);
      
      return tex;
    }
  }
  
  return NULL;
}

// Load PNAMES lump
mappatchnames_t* load_pnames(FILE* wad_file, filelump_t* lump) {
  if (!lump || lump->size <= 0) return NULL;
  
  fseek(wad_file, lump->filepos, SEEK_SET);
  
  int32_t num_patches;
  fread(&num_patches, sizeof(int32_t), 1, wad_file);
  
  // Allocate memory for patch names
  int size = sizeof(mappatchnames_t) + (num_patches - 1) * sizeof(lumpname_t);
  mappatchnames_t* pnames = malloc(size);
  if (!pnames) return NULL;
  
  pnames->nummappatches = num_patches;
  
  // Read patch names
  fread(pnames->name, sizeof(lumpname_t), num_patches, wad_file);
  
  return pnames;
}

// Try to load texture from texture directories
static mapside_texture_t*
load_texture_from_directories(FILE* wad_file, filelump_t* directory, int num_lumps,
                              texname_t tex_name, texture_directory_t* tex_dirs,
                              mappatchnames_t* pnames, const palette_entry_t* palette)
{
  static mapside_texture_t tmp;
  maptexture_t* tex_def = NULL;
  texname_t uppercase = {0};

  for (int i = 0; i < sizeof(texname_t); i++) {
    uppercase[i] = toupper(tex_name[i]);
  }
  
  // Try each texture directory until we find the texture
  for (int i = 0; i < MAX_TEXDIR && tex_dirs[i].offsets; i++) {
    tex_def = find_texture(wad_file, &tex_dirs[i], uppercase);
    if (tex_def) break;
  }
  
  // If texture definition found, create OpenGL texture
  if (tex_def) {
    tmp.texture = create_texture_from_definition(wad_file, directory, num_lumps, tex_def, pnames, palette);
    tmp.width = tex_def->width;
    tmp.height = tex_def->height;
    strncpy(tmp.name, tex_name, sizeof(texname_t));
    free(tex_def);
    return &tmp;
  }
  
  return NULL;
}

// Try to load texture
static void maybe_load_texture(FILE* wad_file, filelump_t* directory, int num_lumps,
                               texture_cache_t* cache, texname_t tex_name,
                               texture_directory_t* tex_dirs, mappatchnames_t* pnames,
                               palette_entry_t* palette) {
  if (tex_name[0] == '-' || tex_name[0] == '\0') return;
  if (cache->num_textures == MAX_TEXTURES) return;
  
  // Check if texture is already in cache
  for (int j = 0; j < cache->num_textures; j++) {
    if (strncmp(cache->textures[j].name, tex_name, 8) == 0) {
      return;  // Already cached
    }
  }
  
  // Try to load from texture directories
  mapside_texture_t *tex =
  load_texture_from_directories(wad_file, directory, num_lumps, tex_name,
                                tex_dirs, pnames, palette);
  
  if (tex) {
    cache->textures[cache->num_textures++] = *tex;
  }
}

// Main function to allocate textures for map sides
int allocate_mapside_textures(map_data_t* map, FILE* wad_file, filelump_t* directory, int num_lumps) {
  texture_cache_t* cache = &g_cache;
  
  // Find lumps
  int palette_index = find_lump(directory, num_lumps, "PLAYPAL");
  int pnames_index = find_lump(directory, num_lumps, "PNAMES");
  
  if (palette_index < 0 || pnames_index < 0) {
    printf("Error: Required lumps not found (PLAYPAL, PNAMES)\n");
    return 0;
  }
  
  filelump_t* palette_lump = &directory[palette_index];
  filelump_t* pnames_lump  = &directory[pnames_index];
  
  // Array of texture directories
  texture_directory_t tex_dirs[MAX_TEXDIR]={0};
  int dir_count = 0;
  
  // Find and load all texture directories (TEXTURE1, TEXTURE2, etc.)
  for (int i = 0; dir_count < MAX_TEXDIR; i++) {
    char tex_lump_name[sizeof(lumpname_t)+1]={0};
    snprintf(tex_lump_name, sizeof(tex_lump_name), "TEXTURE%d", i+1);
    int index = find_lump(directory, num_lumps, tex_lump_name);
    if (index >= 0) {
      filelump_t* lump = &directory[index];
      if (load_texture_directory(wad_file, lump, &tex_dirs[dir_count])) {
        dir_count++;
      }
    }
  }
  
  if (dir_count == 0) {
    printf("Error: No texture directories found (TEXTURE1, TEXTURE2)\n");
    return 0;
  }
  
  // Load palette
  fseek(wad_file, palette_lump->filepos, SEEK_SET);
  palette_entry_t* palette = malloc(256 * sizeof(palette_entry_t));
  if (!palette) {
    printf("Error: Failed to allocate memory for palette\n");
    for (int i = 0; i < dir_count; i++) {
      free(tex_dirs[i].offsets);
    }
    return 0;
  }
  fread(palette, sizeof(palette_entry_t), 256, wad_file);
  
  // Load PNAMES
  mappatchnames_t* pnames = load_pnames(wad_file, pnames_lump);
  if (!pnames) {
    printf("Error: Failed to load PNAMES lump\n");
    free(palette);
    for (int i = 0; i < dir_count; i++) {
      free(tex_dirs[i].offsets);
    }
    return 0;
  }
  
  // Process each sidedef
  for (int i = 0; i < map->num_sidedefs; i++) {
    mapsidedef_t* side = &map->sidedefs[i];
    
    maybe_load_texture(wad_file, directory, num_lumps, cache, side->toptexture, tex_dirs, pnames, palette);
    maybe_load_texture(wad_file, directory, num_lumps, cache, side->midtexture, tex_dirs, pnames, palette);
    maybe_load_texture(wad_file, directory, num_lumps, cache, side->bottomtexture, tex_dirs, pnames, palette);
  }
  
  // Clean up
  free(palette);
  free(pnames);
  for (int i = 0; i < dir_count; i++) {
    free(tex_dirs[i].offsets);
  }
  
  return cache->num_textures;
}

// Free texture cache
void free_texture_cache(texture_cache_t* cache) {
  if (!cache) return;
  
  for (int i = 0; i < cache->num_textures; i++) {
    if (cache->textures[i].texture) {
      glDeleteTextures(1, &cache->textures[i].texture);
    }
  }
  
  cache->num_textures = 0;
}

// Get texture from cache by name
mapside_texture_t *get_texture_from_cache(texture_cache_t* cache, const char* name) {
  for (int i = 0; i < cache->num_textures; i++) {
    if (strncmp(cache->textures[i].name, name, 8) == 0) {
      return &cache->textures[i];
    }
  }
  
  return NULL;
}

// Convenience function to get texture
mapside_texture_t *get_texture(const char* name) {
  return get_texture_from_cache(&g_cache, name);
}

/*
 Flat textures
 */

// Load a single flat texture and create an OpenGL texture
mapside_texture_t *
load_flat_texture(FILE* wad_file,
                  filelump_t* flat_lump,
                  const palette_entry_t* palette,
                  texname_t floorpic)
{
  static mapside_texture_t tmp = {0};
  if (!flat_lump || flat_lump->size <= 0) return 0;
  
  // Check size - flats should be 64x64 (4096 bytes)
  if (flat_lump->size != 4096) {
    printf("Warning: Flat %s has unexpected size: %d bytes\n", flat_lump->name, flat_lump->size);
    if (flat_lump->size < 4096) return 0;
  }
  
  // Seek to flat data
  fseek(wad_file, flat_lump->filepos, SEEK_SET);
  
  // Flats are 64x64 pixels
  const int width = 64;
  const int height = 64;
  
  // Allocate memory for the flat data (RGBA)
  uint8_t* flat_data = malloc(width * height * 4);
  if (!flat_data) return 0;
  
  // Read raw flat data (just color indices)
  uint8_t raw_flat[4096];
  fread(raw_flat, 1, 4096, wad_file);
  
  // Convert color indices to RGBA using palette
  for (int i = 0; i < width * height; i++) {
    uint8_t index = raw_flat[i];
    flat_data[i * 4] = palette[index].r;     // R
    flat_data[i * 4 + 1] = palette[index].g; // G
    flat_data[i * 4 + 2] = palette[index].b; // B
    flat_data[i * 4 + 3] = 255;              // A (always opaque)
  }
  
  // Create OpenGL texture
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flat_data);
  
  glGenerateMipmap(GL_TEXTURE_2D);
  
  free(flat_data);
  
  tmp.texture = tex;
  tmp.width = width;
  tmp.height = height;
  strncpy(tmp.name, floorpic, sizeof(texname_t));

  return &tmp;
}

// Check if a lump is a marker (like F_START, F_END)
bool is_marker(const char* name) {
  if (name[0] == 'F' && name[1] == '_' && strcmp(name + 2, "START") == 0) return true;
  if (name[0] == 'F' && name[1] == '_' && strcmp(name + 2, "END") == 0) return true;
  if (strcmp(name, "FF_START") == 0) return true;
  if (strcmp(name, "FF_END") == 0) return true;
  return false;
}

// Main function to allocate flat textures for map
int allocate_flat_textures(map_data_t* map, FILE* wad_file, filelump_t* directory, int num_lumps) {
  flat_cache_t* cache = &g_flat_cache;
  
  // Find palette lump
  int palette_index = find_lump(directory, num_lumps, "PLAYPAL");
  if (palette_index < 0) {
    printf("Error: Required lump not found (PLAYPAL)\n");
    return 0;
  }
  
  // Load palette
  filelump_t* palette_lump = &directory[palette_index];
  fseek(wad_file, palette_lump->filepos, SEEK_SET);
  palette_entry_t* palette = malloc(256 * sizeof(palette_entry_t));
  if (!palette) {
    printf("Error: Failed to allocate memory for palette\n");
    return 0;
  }
  fread(palette, sizeof(palette_entry_t), 256, wad_file);
  
  // Find F_START and F_END markers
  int f_start = -1, f_end = -1;
  int ff_start = -1, ff_end = -1;
  
  for (int i = 0; i < num_lumps; i++) {
    if (strncmp(directory[i].name, "F_START", 8) == 0) f_start = i;
    else if (strncmp(directory[i].name, "F_END", 8) == 0) f_end = i;
    else if (strncmp(directory[i].name, "FF_START", 8) == 0) ff_start = i;
    else if (strncmp(directory[i].name, "FF_END", 8) == 0) ff_end = i;
  }
  
  if (f_start < 0 || f_end < 0 || f_start >= f_end) {
    printf("Error: Could not find flat markers (F_START/F_END)\n");
    free(palette);
    return 0;
  }
  
  // Load flats between F_START and F_END
  for (int i = f_start + 1; i < f_end; i++) {
    // Skip markers
    if (is_marker(directory[i].name)) continue;
    
    // Check if we're at capacity
    if (cache->num_textures >= MAX_TEXTURES) break;
    
    // Check if flat is already in cache
    bool already_cached = false;
    for (int j = 0; j < cache->num_textures; j++) {
      if (strncmp(cache->textures[j].name, directory[i].name, 8) == 0) {
        already_cached = true;
        break;
      }
    }
    if (already_cached) continue;
    
    // Load the flat texture
    mapside_texture_t *tex = load_flat_texture(wad_file, &directory[i], palette, directory[i].name);
    if (tex) {
      cache->textures[cache->num_textures++] = *tex;
    }
  }
  
  // If FF_START/FF_END exist, load those flats too (DOOM II)
  if (ff_start >= 0 && ff_end >= 0 && ff_start < ff_end) {
    for (int i = ff_start + 1; i < ff_end; i++) {
      // Skip markers
      if (is_marker(directory[i].name)) continue;
      
      // Check if we're at capacity
      if (cache->num_textures >= MAX_TEXTURES) break;
      
      // Check if flat is already in cache
      bool already_cached = false;
      for (int j = 0; j < cache->num_textures; j++) {
        if (strncmp(cache->textures[j].name, directory[i].name, 8) == 0) {
          already_cached = true;
          break;
        }
      }
      if (already_cached) continue;
      
      // Load the flat texture
      mapside_texture_t *tex = load_flat_texture(wad_file, &directory[i], palette, directory[i].name);
      if (tex) {
        cache->textures[cache->num_textures++] = *tex;
      }
    }
  }
  
  // Process each sector to preload required flat textures
  for (int i = 0; i < map->num_sectors; i++) {
    mapsector_t* sector = &map->sectors[i];
    
    // Preload floor texture if not already loaded
    bool floor_loaded = false;
    for (int j = 0; j < cache->num_textures; j++) {
      if (strncmp(cache->textures[j].name, sector->floorpic, 8) == 0) {
        floor_loaded = true;
        break;
      }
    }
    
    // If not loaded yet, try to find and load it
    if (!floor_loaded) {
      int flat_index = find_lump(directory, num_lumps, sector->floorpic);
      if (flat_index >= 0) {
        mapside_texture_t *tex = load_flat_texture(wad_file, &directory[flat_index], palette, sector->floorpic);
        if (tex && cache->num_textures < MAX_TEXTURES) {
          cache->textures[cache->num_textures++] = *tex;
        }
      }
    }
    
    // Preload ceiling texture if not already loaded
    bool ceiling_loaded = false;
    for (int j = 0; j < cache->num_textures; j++) {
      if (strncmp(cache->textures[j].name, sector->ceilingpic, 8) == 0) {
        ceiling_loaded = true;
        break;
      }
    }
    
    // If not loaded yet, try to find and load it
    if (!ceiling_loaded) {
      int flat_index = find_lump(directory, num_lumps, sector->ceilingpic);
      if (flat_index >= 0) {
        mapside_texture_t *tex = load_flat_texture(wad_file, &directory[flat_index], palette, sector->ceilingpic);
        if (tex && cache->num_textures < MAX_TEXTURES) {
          cache->textures[cache->num_textures++] = *tex;
        }
      }
    }
  }
  
  free(palette);
  return cache->num_textures;
}

// Free flat texture cache
void free_flat_texture_cache(flat_cache_t* cache) {
  if (!cache) return;
  
  for (int i = 0; i < cache->num_textures; i++) {
    if (cache->textures[i].texture) {
      glDeleteTextures(1, &cache->textures[i].texture);
    }
  }
  
  cache->num_textures = 0;
}

// Get flat texture from cache by name
mapside_texture_t const *
get_flat_texture_from_cache(flat_cache_t* cache, const char* name) {
  for (int i = 0; i < cache->num_textures; i++) {
    if (strncmp(cache->textures[i].name, name, 8) == 0) {
      return &cache->textures[i];
    }
  }
  
  return NULL;
}

// Convenience function to get flat texture
mapside_texture_t const *
get_flat_texture(const char* name) {
  return get_flat_texture_from_cache(&g_flat_cache, name);
}
