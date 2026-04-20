#include <mapview/map.h>
#include <mapview/sprites.h>
#include <mapview/console.h>
#include <editor/editor.h>
#include <mapview/gamefont.h>
#include <ui/kernel/kernel.h>
#include <ui/gem_magic.h>

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 480

void init_floor_shader(void);
void init_sky_geometry(void);
bool init_radial_menu(void);

result_t win_statbar(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_game(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_project(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);
result_t win_dummy(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

window_t *g_inspector = NULL;

static void init_windows(hinstance_t hinstance) {
  show_window(create_window("Things", WINDOW_VSCROLL, MAKERECT(8, 96, THING_SIZE*3, 256), NULL, win_things, hinstance, NULL), true);
  show_window(create_window("Project", WINDOW_VSCROLL, MAKERECT(4, 20, 128, 256), NULL, win_project, hinstance, NULL), true);
  g_inspector = create_window("Inspector", WINDOW_TOOLBAR, MAKERECT(ui_get_system_metrics(kSystemMetricScreenWidth)-200, 40, 150, 300), NULL, win_dummy, hinstance, NULL);
  show_window(g_inspector, true);
}

bool gem_init(int argc, char *argv[], hinstance_t hinstance) {
  if (argc < 2) {
    printf("Usage: doom-ed <wad_file>\n");
    return false;
  }

  const char *filename = argv[1];

  if (!init_wad(filename)) {
    printf("Error: Could not open file %s\n", filename);
    return false;
  }

  if (!cache_lump("PLAYPAL")) {
    printf("Error: Required lump not found (PLAYPAL)\n");
    return false;
  }
  extern palette_entry_t *palette;
  palette = cache_lump("PLAYPAL");

  ui_joystick_init();
  init_resources();
  init_floor_shader();
  init_sky_geometry();
  init_radial_menu();
  init_gamefont();
  load_console_font();
  init_sprites();
  init_things();
  init_intermission();
  init_windows(hinstance);

  allocate_mapside_textures();
  allocate_flat_textures();

  open_map("MAP01");
  return true;
}

void gem_shutdown(void) {
  shutdown_wad();
  ui_joystick_shutdown();
}

GEM_STANDALONE_MAIN("DOOM Wireframe Renderer", UI_INIT_DESKTOP|UI_INIT_TRAY,
                    SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL)
