#include <editor/editor.h>

enum {
  ID_SECTOR_FLOOR_HEIGHT = 1000,
  ID_SECTOR_FLOOR_IMAGE,
  ID_SECTOR_CEILING_HEIGHT,
  ID_SECTOR_CEILING_IMAGE,
  ID_SECTOR_LIGHT_LEVEL,
  ID_TEST_COMBOBOX,
  ID_SECTOR_IDENT,
};

windef_t sector_layout[] = {
  { win_label, "Sector#", -1, LABEL_WIDTH },
  { win_textedit, "", ID_SECTOR_IDENT, 50 },
  { win_label, "Light lvl:", -1, LABEL_WIDTH },
  { win_textedit, "", ID_SECTOR_LIGHT_LEVEL, 50 },
  { win_label, "Floor Hgt:", -1, LABEL_WIDTH },
  { win_textedit, "", ID_SECTOR_FLOOR_HEIGHT, 50 },
  { win_label, "Ceiling Hgt:", -1, LABEL_WIDTH },
  { win_textedit, "", ID_SECTOR_CEILING_HEIGHT, 50 },
//  { win_combobox, "Stone", ID_TEST_COMBOBOX, 90 }, {win_space},
  { win_sprite, "", ID_SECTOR_FLOOR_IMAGE, 64, 64 },
  { win_sprite, "", ID_SECTOR_CEILING_IMAGE, 64, 64 },
  { NULL }
};

mapsector_t *selected_sector(game_t *game) {
  if (!g_game) {
    return NULL;
  } else if (has_selection(game->state.hover, obj_sector)) {
    return &game->map.sectors[game->state.hover.index];
  } else if (has_selection(game->state.selected, obj_sector)) {
    return &game->map.sectors[game->state.selected.index];
  } else {
    return NULL;
  }
}

//static void init_combobox(window_t *cb) {
//  send_message(cb, CB_ADDSTRING, 0, "worldspawn");
//  send_message(cb, CB_ADDSTRING, 0, "trigger_once");
//  send_message(cb, CB_ADDSTRING, 0, "func_door");
//  send_message(cb, CB_ADDSTRING, 0, "light");
//  send_message(cb, CB_ADDSTRING, 0, "misc_model");
//  send_message(cb, CB_ADDSTRING, 0, "path_corner");
//  send_message(cb, CB_ADDSTRING, 0, "env_spark");
//  send_message(cb, CB_ADDSTRING, 0, "func_train");
//  send_message(cb, CB_ADDSTRING, 0, "info_player_start");
//  send_message(cb, CB_ADDSTRING, 0, "target_teleport");
//  send_message(cb, CB_SETCURSEL, 2, NULL);
//}

static toolbar_button_t but[] = {
  { icon16_select, edit_select },
  { icon16_points, edit_vertices },
  { icon16_things, edit_things },
  { icon16_sounds, edit_sounds },
};

void set_selection_mode(editor_state_t *editor, int mode);

result_t win_dummy(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = get_editor();
  switch (msg) {
    case WM_CREATE:
      send_message(win, TB_ADDBUTTONS, sizeof(but)/sizeof(*but), but);      
      return true;
    case WM_PAINT:
      draw_text_small("Nothing selected", 5, 5, COLOR_DARK_EDGE);
      draw_text_small("Nothing selected", 4, 4, COLOR_TEXT_NORMAL);
      return true;
    case TB_BUTTONCLICK:
      for (int i = 0; i < win->num_toolbar_buttons; i++) {
        toolbar_button_t *but = &win->toolbar_buttons[i];
        but->active = (but->ident == wparam);
      }
      set_selection_mode(editor, wparam);
      invalidate_window(win);
      return true;
    default:
      return false;
  }
}

result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = get_editor();
  mapsector_t *sector = selected_sector(g_game);
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      g_inspector = win;
      load_window_children(win, sector_layout);
//      init_combobox(get_window_item(win, ID_TEST_COMBOBOX));
      return true;
    case WM_PAINT:
      if (sector) {
        set_window_item_text(win, ID_SECTOR_LIGHT_LEVEL, "%d", sector->lightlevel);
        set_window_item_text(win, ID_SECTOR_FLOOR_HEIGHT, "%d", sector->floorheight);
        set_window_item_text(win, ID_SECTOR_FLOOR_IMAGE, sector->floorpic);
        set_window_item_text(win, ID_SECTOR_CEILING_HEIGHT, "%d", sector->ceilingheight);
        set_window_item_text(win, ID_SECTOR_CEILING_IMAGE, sector->ceilingpic);
      }
      return false;
    case WM_COMMAND:
      if (sector) {
        switch (wparam) {
          case MAKEDWORD(ID_SECTOR_LIGHT_LEVEL, EN_UPDATE):
            sector->lightlevel = atoi(((window_t *)lparam)->title);
            break;
          case MAKEDWORD(ID_SECTOR_FLOOR_HEIGHT, EN_UPDATE):
            sector->floorheight = atoi(((window_t *)lparam)->title);
            break;
          case MAKEDWORD(ID_SECTOR_CEILING_HEIGHT, EN_UPDATE):
            sector->ceilingheight = atoi(((window_t *)lparam)->title);
            break;
        }
        if (g_game) {
          build_wall_vertex_buffer(&g_game->map);
          build_floor_vertex_buffer(&g_game->map);
          invalidate_window(editor->window);
        }
      }
      return true;
  }
  return win_dummy(win, msg, wparam, lparam);
}
