#include "map.h"

struct {
  filelump_t* directory;
  int num_lumps;
} wad = {0};

// Function to find a lump index by name
int find_lump(filelump_t* directory, int num_lumps, const char* name) {
  for (int i = 0; i < num_lumps; i++) {
    if (strncmp(directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return i;
    }
  }
  return -1; // Not found
}
