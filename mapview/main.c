#include "map.h"
#include "sprites.h"
#include "console.h"

game_t game;

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
  
  game.state = GS_WORLD;
  
  // Print map info
  // Initialize SDL
  if (!init_sdl()) {
    return 1;
  }

  init_console();
  load_console_font();
  
  init_sprites();
  init_intermission();


  run();
  
  // Cleanup
  free_map_data(&game.map);

  shutdown_wad();
  
  return 0;
}
