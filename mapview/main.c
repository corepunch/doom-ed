#include "map.h"
#include "sprites.h"
#include "console.h"


bool init_sky(map_data_t const*);

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
  
  const char *mapname = "MAP01";
  
  map_data_t e1m1 = load_map(mapname);
  
  find_all_maps();
  
  const char *get_map_name(const char *name);
  
  // Print map info
  if (e1m1.num_vertices > 0) {
    print_map_info(&e1m1);
    // Initialize SDL
    if (!init_sdl()) {
      return 1;
    }
    init_console();
    load_console_font(e1m1.palette);

    conprintf("Successfully loaded map %s", get_map_name(mapname));

    allocate_mapside_textures(&e1m1);
    allocate_flat_textures(&e1m1);
    init_sprites(&e1m1);
    init_sky(&e1m1);
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
