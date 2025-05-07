#include "map.h"

struct {
  filelump_t* directory;
  int num_lumps;
  void **cache;
  FILE *file;
} wad = {0};

// Function to find a lump index by name
filelump_t *find_lump(const char* name) {
  for (int i = 0; i < wad.num_lumps; i++) {
    if (strncmp(wad.directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return &wad.directory[i];
    }
  }
  return NULL; // Not found
}

void *cache_lump(const char* name) {
  for (int i = 0; i < wad.num_lumps; i++) {
    if (strncmp(wad.directory[i].name, name, sizeof(lumpname_t)) == 0) {
      wad.cache[i] = malloc(wad.directory[i].size);
      fseek(wad.file, wad.directory[i].filepos, SEEK_SET);
      fread(wad.cache[i], wad.directory[i].size, 1, wad.file);
      return wad.cache[i];
    }
  }
  return NULL; // Not found
}
