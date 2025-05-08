#include "map.h"
#include "sprites.h"
#include "console.h"

game_t game;

bool init_sky(map_data_t const*);

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: %s <wad_file>\n", argv[0]);
    return 1;
  }
  
  const char* filename = argv[1];

  if (!init_wad(filename)) {
    printf("Error: Could not open file %s\n", filename);
    return 1;
  }

//  MAP29  The Living End  ~1300 linedefs, ~1900 sidedefs, ~1100 vertices
//  MAP15  Industrial Zone  Also very large, very open with many secrets
//  MAP14  The Inmost Dens  Highly detailed architecture
  
//  E2M5 – The Crater: vertical layers, lava pit puzzle, lots of atmosphere.
//  E3M1 – The Storehouse: crazy use of crushers, secrets, and teleport traps.
//  E4M4 – The Halls of Fear: shows how moody lighting and vertical loops can go a long way.
  
  // Load E1M1 map
  
  
  if (!cache_lump("PLAYPAL")) {
    printf("Error: Required lump not found (PLAYPAL)\n");
    return 0;
  }
  extern palette_entry_t *palette;
  palette = cache_lump("PLAYPAL");
  
  const char *mapname = "E1M1";
  
  map_data_t e1m1 = load_map(mapname);
  
  find_all_maps();
  
  const char *get_map_name(const char *name);
  void init_intermission(void);
  
  game.state = GS_WORLD;
  
  // Print map info
  if (e1m1.num_vertices > 0) {
    print_map_info(&e1m1);
    // Initialize SDL
    if (!init_sdl()) {
      return 1;
    }
    init_console();
    load_console_font();

    conprintf("Successfully loaded map %s", get_map_name(mapname));

    allocate_mapside_textures(&e1m1);
    allocate_flat_textures(&e1m1);
    init_sprites(&e1m1);
    init_sky(&e1m1);
    init_intermission();
    build_wall_vertex_buffer(&e1m1);
    build_floor_vertex_buffer(&e1m1);
    run(&e1m1);
  } else {
    printf("\nFailed to load map\n");
  }
  
  // Cleanup
  free_map_data(&e1m1);

  shutdown_wad();
  
  return 0;
}
