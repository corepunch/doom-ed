#include "map.h"
#include "sprites.h"

// Function to read data from a file at a specific offset
void* read_lump_data(FILE* file, int offset, int size) {
  void* data = malloc(size);
  if (!data) return NULL;
  
  fseek(file, offset, SEEK_SET);
  fread(data, 1, size, file);
  return data;
}

// Function to find a lump index by name
int find_lump(filelump_t* directory, int num_lumps, const char* name) {
  for (int i = 0; i < num_lumps; i++) {
    if (strncmp(directory[i].name, name, sizeof(lumpname_t)) == 0) {
      return i;
    }
  }
  return -1; // Not found
}

// Function to load map data
map_data_t load_map(FILE* file, filelump_t* directory, int num_lumps, const char* map_name) {
  map_data_t map = {0};
  
  // Find the map marker lump
  int map_index = find_lump(directory, num_lumps, map_name);
  if (map_index == -1) {
    printf("Map %s not found!\n", map_name);
    return map;
  }
  
  // Map lumps follow a specific order after the map marker
  if (map_index + 10 <= num_lumps) {
    // THINGS
    int things_idx = map_index + 1;
    map.num_things = directory[things_idx].size / sizeof(mapthing_t);
    map.things = (mapthing_t*)read_lump_data(file, directory[things_idx].filepos, directory[things_idx].size);
    
    // LINEDEFS
    int linedefs_idx = map_index + 2;
    map.num_linedefs = directory[linedefs_idx].size / sizeof(maplinedef_t);
    map.linedefs = (maplinedef_t*)read_lump_data(file, directory[linedefs_idx].filepos, directory[linedefs_idx].size);
    
    // SIDEDEFS
    int sidedefs_idx = map_index + 3;
    map.num_sidedefs = directory[sidedefs_idx].size / sizeof(mapsidedef_t);
    map.sidedefs = (mapsidedef_t*)read_lump_data(file, directory[sidedefs_idx].filepos, directory[sidedefs_idx].size);
    
    // VERTEXES
    int vertices_idx = map_index + 4;
    map.num_vertices = directory[vertices_idx].size / sizeof(mapvertex_t);
    map.vertices = (mapvertex_t*)read_lump_data(file, directory[vertices_idx].filepos, directory[vertices_idx].size);
    
    // SECTORS
    int sectors_idx = map_index + 8;
    map.num_sectors = directory[sectors_idx].size / sizeof(mapsector_t);
    map.sectors = (mapsector_t*)read_lump_data(file, directory[sectors_idx].filepos, directory[sectors_idx].size);
    
    map.walls.sections = malloc(sizeof(mapsidedef2_t) * map.num_sidedefs);
    memset(map.walls.sections, 0, sizeof(mapsidedef2_t) * map.num_sidedefs);
    for (uint32_t i = 0; i < map.num_sidedefs; i++) {
      map.walls.sections[i].def = &map.sidedefs[i];
      map.walls.sections[i].sector = &map.sectors[map.sidedefs[i].sector];
    }

    map.floors.sectors = malloc(sizeof(mapsector2_t) * map.num_sectors);
    memset(map.floors.sectors, 0, sizeof(mapsector2_t) * map.num_sectors);
    for (uint32_t i = 0; i < map.num_sectors; i++) {
      map.floors.sectors[i].sector = &map.sectors[i];
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: %s <wad_file>\n", argv[0]);
    return 1;
  }
  
  const char* filename = argv[1];
  FILE* file = fopen(filename, "rb");
  if (!file) {
    printf("Error: Could not open file %s\n", filename);
    return 1;
  }
  
  // Read WAD header
  wadheader_t header;
  fread(&header, sizeof(wadheader_t), 1, file);
  
  printf("WAD Type: %.4s\n", header.identification);
  printf("Lumps: %d\n", header.numlumps);
  
  // Read directory
  filelump_t* directory = malloc(sizeof(filelump_t) * header.numlumps);
  fseek(file, header.infotableofs, SEEK_SET);
  fread(directory, sizeof(filelump_t), header.numlumps, file);
  
//  MAP29  The Living End  ~1300 linedefs, ~1900 sidedefs, ~1100 vertices
//  MAP15  Industrial Zone  Also very large, very open with many secrets
//  MAP14  The Inmost Dens  Highly detailed architecture
  
//  E2M5 – The Crater: vertical layers, lava pit puzzle, lots of atmosphere.
//  E3M1 – The Storehouse: crazy use of crushers, secrets, and teleport traps.
//  E4M4 – The Halls of Fear: shows how moody lighting and vertical loops can go a long way.
  
  // Load E1M1 map
  map_data_t e1m1 = load_map(file, directory, header.numlumps, "MAP01");
  
  // Print map info
  if (e1m1.num_vertices > 0) {
    printf("\nSuccessfully loaded map\n");
    print_map_info(&e1m1);
    // Initialize SDL
    if (!init_sdl()) {
      return 1;
    }
    allocate_mapside_textures(&e1m1, file, directory, header.numlumps);
    allocate_flat_textures(&e1m1, file, directory, header.numlumps);
    init_sprites(file, directory, header.numlumps);
    build_wall_vertex_buffer(&e1m1);
    build_floor_vertex_buffer(&e1m1);
    run(&e1m1);
  } else {
    printf("\nFailed to load map\n");
  }
  
  // Cleanup
  free_map_data(&e1m1);
  free(directory);
  fclose(file);
  
  return 0;
}
