#include <mapview/map.h>
#include <mapview/sprites.h>
#include <mapview/console.h>
#include <editor/editor.h>
#include <mapview/gamefont.h>
#include <ui/kernel/kernel.h>

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 480

void init_floor_shader(void);
void init_sky_geometry(void);
bool init_radial_menu(void);

//result_t win_desktop(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
//result_t win_tray(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_statbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_project(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_dummy(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

window_t *g_inspector = NULL;

void init_windows(void) {
  //  create_window("FPS", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT(0, 0, 128, 64), NULL, win_perf, NULL);
  //  create_window("Statbar", WINDOW_NOTITLE|WINDOW_TRANSPARENT, MAKERECT((ui_get_system_metrics(SM_CXSCREEN)-VGA_WIDTH)/2, (ui_get_system_metrics(SM_CYSCREEN)-VGA_HEGHT), VGA_WIDTH, VGA_HEGHT), NULL, win_statbar, NULL);
  //  create_window("Console", 0, MAKERECT(32, 32, 512, 256), NULL, win_console, NULL);
//  create_window("Game", WINDOW_NOFILL, MAKERECT(380, 128, 320, 320), NULL, win_game, NULL);
  show_window(create_window("Things", WINDOW_VSCROLL, MAKERECT(8, 96, THING_SIZE*3, 256), NULL, win_things, NULL), true);
  show_window(create_window("Project", WINDOW_VSCROLL, MAKERECT(4, 20, 128, 256), NULL, win_project, NULL), true);
  //  create_window("Mode", 0, MAKERECT(200, 20, 320, 20), NULL, win_editmode, NULL);
//  create_window("Inspector", 0, MAKERECT(200, 20, 150, 300), NULL, win_sector, NULL);
  
  g_inspector = create_window("Inspector", WINDOW_TOOLBAR, MAKERECT(ui_get_system_metrics(SM_CXSCREEN)-200, 40, 150, 300), NULL, win_dummy, NULL);
  show_window(g_inspector, true);
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
  
  // Print map info
  // Initialize window and OpenGL context
  if (!ui_init_graphics(SDL_INIT_JOYSTICK|UI_INIT_DESKTOP|UI_INIT_TRAY,
                        "DOOM Wireframe Renderer",
                        SCREEN_WIDTH, SCREEN_HEIGHT))
  {
    return 1;
  }
  
  // Initialize joystick support through UI layer
  // Note: Joystick initialization is now handled by the UI layer
  // to decouple SDL-specific code and prepare for potential GLFW migration
  ui_joystick_init();
  
  // Initialize SDL
  init_resources();
  init_floor_shader();
  init_sky_geometry();
  init_radial_menu();
  init_gamefont();
  load_console_font();
  init_sprites();
  init_things();
  init_intermission();
  init_windows();
  
  allocate_mapside_textures();
  allocate_flat_textures();

//  new_map();
  open_map("MAP01");
  
//  bool running = true;
  // Main game loop
  while (running) {
    SDL_Event event;
    while (get_message(&event)) {
//      if (event.type == SDL_QUIT) {
//        running = false;
//      }
      dispatch_message(&event);
    }
    repost_messages();
  }

  shutdown_wad();
  
  ui_joystick_shutdown();
  
  ui_shutdown_graphics();
  
  return 0;
}
