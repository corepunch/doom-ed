#include "map.h"
#include "sprites.h"
#include "console.h"
#include "editor.h"

result_t win_desktop(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_statbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_toolbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_stack(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_project(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

void init_windows(void) {
  //  create_window("FPS", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT(0, 0, 128, 64), NULL, win_perf, NULL);
  //  create_window("Statbar", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT((screen_width-VGA_WIDTH)/2, (screen_height-VGA_HEGHT), VGA_WIDTH, VGA_HEGHT), NULL, win_statbar, NULL);
  //  create_window("Console", 0, MAKERECT(32, 32, 512, 256), NULL, win_console, NULL);
  show_window(create_window("Desktop", WINDOW_NOTITLE|WINDOW_ALWAYSINBACK|WINDOW_NOTRAYBUTTON, MAKERECT(0, 0, screen_width, screen_height), NULL, win_desktop, NULL), true);
  show_window(create_window("Tray", WINDOW_NOTITLE|WINDOW_NOTRAYBUTTON, MAKERECT(0, 0, 0, 0), NULL, win_tray, NULL), true);
//  create_window("Game", WINDOW_NOFILL, MAKERECT(380, 128, 320, 320), NULL, win_game, NULL);
  create_window("Things", WINDOW_VSCROLL, MAKERECT(96, 96, 128, 256), NULL, win_things, NULL);
  create_window("Project", WINDOW_VSCROLL, MAKERECT(4, 20, 128, 256), NULL, win_project, NULL);
  //  create_window("Mode", 0, MAKERECT(200, 20, 320, 20), NULL, win_editmode, NULL);
//  create_window("Inspector", 0, MAKERECT(200, 20, 150, 300), NULL, win_sector, NULL);
  
  window_t *stackview = create_window("Sidebar", WINDOW_NOFILL, MAKERECT(screen_width-160, 16, 155, 320), NULL, win_stack, NULL);
  show_window(stackview, true);
  send_message(stackview, ST_ADDWINDOW, 0, create_window("Toolbar", WINDOW_NOTRAYBUTTON, MAKERECT(16, 16, 150, 16), NULL, win_toolbar, NULL));
  send_message(stackview, ST_ADDWINDOW, 0, create_window("Inspector", WINDOW_NOTRAYBUTTON, MAKERECT(200, 20, 150, 300), NULL, win_sector, NULL));
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
  
  allocate_mapside_textures();
  allocate_flat_textures();

  new_map();
//  open_map("MAP01");
  
  run();

  shutdown_wad();
  
  return 0;
}
