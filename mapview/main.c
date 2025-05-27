#include "map.h"
#include "sprites.h"
#include "console.h"
#include "editor.h"

game_t game;

result_t win_desktop(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_statbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_editor(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_toolbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void init_windows(void) {
  //  create_window("FPS", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT(0, 0, 128, 64), NULL, win_perf, NULL);
  //  create_window("Statbar", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT((screen_width-VGA_WIDTH)/2, (screen_height-VGA_HEGHT), VGA_WIDTH, VGA_HEGHT), NULL, win_statbar, NULL);
  //  create_window("Console", 0, MAKERECT(32, 32, 512, 256), NULL, win_console, NULL);
  extern editor_state_t editor;
  create_window("Desktop", WINDOW_NOTITLE|WINDOW_ALWAYSINBACK, MAKERECT(0, 0, screen_width, screen_height), NULL, win_desktop, &editor);
  create_window("Tray", WINDOW_NOTITLE, MAKERECT(0, 0, 0, 0), NULL, win_tray, &editor);
  create_window("Game", WINDOW_NOFILL, MAKERECT(380, 128, 320, 320), NULL, win_game, &editor);
  create_window("Editor", 0, MAKERECT(32, 128, 320, 320), NULL, win_editor, &editor);
  create_window("Things", WINDOW_VSCROLL, MAKERECT(96, 96, 128, 256), NULL, win_things, &editor);
  //  create_window("Mode", 0, MAKERECT(200, 20, 320, 20), NULL, win_editmode, &editor);
  create_window("Inspector", 0, MAKERECT(200, 20, 150, 300), NULL, win_sector, &editor);
  create_window("Toolbar", WINDOW_ALWAYSONTOP, MAKERECT(16, 16, 128, 16), NULL, win_toolbar, &editor);
}

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
  
//  printf("%s\n", cache_lump("MAPINFO"));
  
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
  init_windows();

  goto_map("MAP01");
  
  run();
  
  // Cleanup
  free_map_data(&game.map);

  shutdown_wad();
  
  return 0;
}
