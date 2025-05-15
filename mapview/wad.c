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

void *cache_lump_num(uint16_t i) {
  wad.cache[i] = malloc(wad.directory[i].size);
  fseek(wad.file, wad.directory[i].filepos, SEEK_SET);
  fread(wad.cache[i], wad.directory[i].size, 1, wad.file);
  return wad.cache[i];
}

void *cache_lump(const char* name) {
  for (int i = 0; i < wad.num_lumps; i++) {
    if (strncmp(wad.directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return cache_lump_num(i);
    }
  }
  return NULL; // Not found
}

// Function to read data from a file at a specific offset
void* read_lump_data(FILE* file, filelump_t const *lump) {
  void* data = malloc(lump->size);
  if (!data) return NULL;
  
  fseek(file, lump->filepos, SEEK_SET);
  fread(data, 1, lump->size, file);
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

  // Map lumps follow a specific order after the map marker
  if (map_index + 10 <= wad.num_lumps) {
    // THINGS
    map.num_things = wad.directory[map_index + ML_THINGS].size / sizeof(mapthing_t);
    map.things = read_lump_data(wad.file, &wad.directory[map_index + ML_THINGS]);
    
    // LINEDEFS
    map.num_linedefs = wad.directory[map_index + ML_LINEDEFS].size / sizeof(maplinedef_t);
    map.linedefs = read_lump_data(wad.file, &wad.directory[map_index + ML_LINEDEFS]);
    
    // SIDEDEFS
    map.num_sidedefs = wad.directory[map_index + ML_SIDEDEFS].size / sizeof(mapsidedef_t);
    map.sidedefs = read_lump_data(wad.file, &wad.directory[map_index + ML_SIDEDEFS]);
    
    // VERTEXES
    map.num_vertices = wad.directory[map_index + ML_VERTEXES].size / sizeof(mapvertex_t);
    map.vertices = read_lump_data(wad.file, &wad.directory[map_index + ML_VERTEXES]);

    // NODES
    map.num_nodes = wad.directory[map_index + ML_NODES].size / sizeof(mapnode_t);
    map.nodes = read_lump_data(wad.file, &wad.directory[map_index + ML_NODES]);

    // SSECTORS
    map.num_subsectors = wad.directory[map_index + ML_SSECTORS].size / sizeof(mapsubsector_t);
    map.subsectors = read_lump_data(wad.file, &wad.directory[map_index + ML_SSECTORS]);

    // SECTORS
    map.num_sectors = wad.directory[map_index + ML_SECTORS].size / sizeof(mapsector_t);
    map.sectors = read_lump_data(wad.file, &wad.directory[map_index + ML_SECTORS]);
    
    // SEGS
    map.num_segs = wad.directory[map_index + ML_SEGS].size / sizeof(mapseg_t);
    map.segs = read_lump_data(wad.file, &wad.directory[map_index + ML_SEGS]);
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
  memset(map, 0, sizeof(map_data_t));
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
