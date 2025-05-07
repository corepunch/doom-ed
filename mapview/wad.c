#include "map.h"

struct {
  filelump_t* directory;
  int num_lumps;
  void **cache;
  FILE *file;
} wad = {0};

bool init_wad(const char *filename) {
  wad.file = fopen(filename, "rb");
  if (!wad.file) {
    printf("Error: Could not open file %s\n", filename);
    return false;
  }
  
  // Read WAD header
  wadheader_t header;
  fread(&header, sizeof(wadheader_t), 1, wad.file);
  
  printf("WAD Type: %.4s\n", header.identification);
  printf("Lumps: %d\n", header.numlumps);
  
  // Read directory
  wad.directory = malloc(sizeof(filelump_t) * header.numlumps);
  wad.num_lumps = header.numlumps;
  wad.cache = malloc(sizeof(void*) * header.numlumps);
  fseek(wad.file, header.infotableofs, SEEK_SET);
  fread(wad.directory, sizeof(filelump_t), header.numlumps, wad.file);
  return true;
}

void shutdown_wad(void) {
  free(wad.directory);
  free(wad.cache);
  fclose(wad.file);
}

// Function to find a lump index by name
filelump_t *find_lump(const char* name) {
  for (int i = 0; i < wad.num_lumps; i++) {
    if (strncmp(wad.directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return &wad.directory[i];
    }
  }
  return NULL; // Not found
}

int find_lump_num(const char* name) {
  for (int i = 0; i < wad.num_lumps; i++) {
    if (strncmp(wad.directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return i;
    }
  }
  return -1; // Not found
}

char const *get_lump_name(int i) {
  return wad.directory[i].name;
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

// Function to read data from a file at a specific offset
void* read_lump_data(FILE* file, int offset, int size) {
  void* data = malloc(size);
  if (!data) return NULL;
  
  fseek(file, offset, SEEK_SET);
  fread(data, 1, size, file);
  return data;
}

bool is_map_block_valid(filelump_t* dir, int index, int total_lumps) {
  static const char* expected[] = {
    "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES",
    "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"
  };
  for (int i = 0; i < 10; i++) {
    if (index + 1 + i >= total_lumps) return false;
    if (strncmp(dir[index + 1 + i].name, expected[i], 8) != 0) return false;
  }
  return true;
}

void find_all_maps(void) {
  for (int i = 0; i < wad.num_lumps - 10; i++) {
    if (is_map_block_valid(wad.directory, i, wad.num_lumps)) {
      printf("Found map: %s\n", wad.directory[i].name);
    }
  }
}

// Function to load map data
map_data_t load_map(const char* map_name) {
  map_data_t map = {0};
  
  // Find the map marker lump
  int map_index = find_lump_num(map_name);
  if (map_index == -1) {
    printf("Map %s not found!\n", map_name);
    return map;
  }
  
  if (!cache_lump("PLAYPAL")) {
    printf("Error: Required lump not found (PLAYPAL)\n");
    return map;
  }
  
  map.palette = cache_lump("PLAYPAL");

  // Map lumps follow a specific order after the map marker
  if (map_index + 10 <= wad.num_lumps) {
    // THINGS
    int things_idx = map_index + 1;
    map.num_things = wad.directory[things_idx].size / sizeof(mapthing_t);
    map.things = (mapthing_t*)read_lump_data(wad.file, wad.directory[things_idx].filepos, wad.directory[things_idx].size);
    
    // LINEDEFS
    int linedefs_idx = map_index + 2;
    map.num_linedefs = wad.directory[linedefs_idx].size / sizeof(maplinedef_t);
    map.linedefs = (maplinedef_t*)read_lump_data(wad.file, wad.directory[linedefs_idx].filepos, wad.directory[linedefs_idx].size);
    
    // SIDEDEFS
    int sidedefs_idx = map_index + 3;
    map.num_sidedefs = wad.directory[sidedefs_idx].size / sizeof(mapsidedef_t);
    map.sidedefs = (mapsidedef_t*)read_lump_data(wad.file, wad.directory[sidedefs_idx].filepos, wad.directory[sidedefs_idx].size);
    
    // VERTEXES
    int vertices_idx = map_index + 4;
    map.num_vertices = wad.directory[vertices_idx].size / sizeof(mapvertex_t);
    map.vertices = (mapvertex_t*)read_lump_data(wad.file, wad.directory[vertices_idx].filepos, wad.directory[vertices_idx].size);
    
    // SECTORS
    int sectors_idx = map_index + 8;
    map.num_sectors = wad.directory[sectors_idx].size / sizeof(mapsector_t);
    map.sectors = (mapsector_t*)read_lump_data(wad.file, wad.directory[sectors_idx].filepos, wad.directory[sectors_idx].size);
  }
  
  for (int i = 0; i < map.num_things; i++) {
    mapsector_t const *sector = find_player_sector(&map, map.things[i].x, map.things[i].y);
    if(sector) {
      map.things[i].flags = sector - map.sectors;
    } else {
      map.things[i].flags = -1;
    }
  }
  
  return map;
}

// Function to free map data
void free_map_data(map_data_t* map) {
  CLEAR_COLLECTION(map, vertices);
  CLEAR_COLLECTION(map, linedefs);
  CLEAR_COLLECTION(map, sidedefs);
  CLEAR_COLLECTION(map, things);
  CLEAR_COLLECTION(map, sectors);
}

// Function to print basic map info
void print_map_info(map_data_t* map) {
  printf("Map info:\n");
  printf("  Vertices: %d\n", map->num_vertices);
  printf("  Linedefs: %d\n", map->num_linedefs);
  printf("  Sidedefs: %d\n", map->num_sidedefs);
  printf("  Things: %d\n", map->num_things);
  printf("  Sectors: %d\n", map->num_sectors);
  
  if (map->num_things > 0) {
    printf("\nPlayer start position:\n");
    // Find player start (thing type 1)
    for (int i = 0; i < map->num_things; i++) {
      if (map->things[i].type == 1) {
        printf("  Position: (%d, %d), Angle: %d\n",
               map->things[i].x, map->things[i].y, map->things[i].angle);
        break;
      }
    }
  }
}
