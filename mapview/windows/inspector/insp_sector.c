#include "../../editor.h"

enum {
  ID_SECTOR_FLOOR_HEIGHT = 1000,
  ID_SECTOR_FLOOR_IMAGE,
  ID_SECTOR_CEILING_HEIGHT,
  ID_SECTOR_CEILING_IMAGE,
  ID_TEST_COMBOBOX,
};

windef_t sector_layout[] = {
  //  { "TEXT", "Type:", -1, 50 },
  { "TEXT", "Floor Hgt:", -1, LABEL_WIDTH },
  { "EDITTEXT", "", ID_SECTOR_FLOOR_HEIGHT, 50 },
  { "TEXT", "Ceiling Hgt:", -1, LABEL_WIDTH },
  { "EDITTEXT", "", ID_SECTOR_CEILING_HEIGHT, 50 },
//  { "COMBOBOX", "Stone", ID_TEST_COMBOBOX, 90 }, {"SPACE"},
  { "SPRITE", "", ID_SECTOR_FLOOR_IMAGE, 64, 64 },
  { "SPRITE", "", ID_SECTOR_CEILING_IMAGE, 64, 64 },
  { NULL }
};

mapsector_t *selected_sector(editor_state_t *editor) {
  if (editor->hover.sector != 0xFFFF) {
    return &game.map.sectors[editor->hover.sector];
  } else if (editor->selected.sector != 0xFFFF) {
    return &game.map.sectors[editor->selected.sector];
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

result_t win_sector(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = win->userdata;
  mapsector_t *sector;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      ((editor_state_t*)lparam)->inspector = win;
      load_window_children(win, sector_layout);
//      init_combobox(get_window_item(win, ID_TEST_COMBOBOX));
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_sectors && (sector = selected_sector(editor))) {
        //        sprite_t *spr = get_thing_sprite_name(thing->type, 0);
        set_window_item_text(win, ID_SECTOR_FLOOR_HEIGHT, "%d", sector->floorheight);
        set_window_item_text(win, ID_SECTOR_FLOOR_IMAGE, sector->floorpic);
        set_window_item_text(win, ID_SECTOR_CEILING_HEIGHT, "%d", sector->ceilingheight);
        set_window_item_text(win, ID_SECTOR_CEILING_IMAGE, sector->ceilingpic);
        //        set_window_item_text(win, ID_THING_POS_Y, "%d", thing->y);
        //        for (int i = 0; i < sizeof(checkboxes)/sizeof(*checkboxes); i++) {
        //          window_t *checkbox = get_window_item(win, checkboxes[i]);
        //          uint32_t value = thing->options&(1<<i);
        //          send_message(checkbox, BM_SETCHECK, value, NULL);
        //        }
      }
      return false;
    case WM_COMMAND:
      if (editor->sel_mode == edit_sectors && (sector = selected_sector(editor))) {
//        for (int i = 0; i < sizeof(checkboxes)/sizeof(*checkboxes); i++) {
//          if (wparam == MAKEDWORD(checkboxes[i], BN_CLICKED)) {
//            if (send_message(lparam, BM_GETCHECK, 0, NULL)) {
//              thing->options |= 1 << i;
//            } else {
//              thing->options &= ~(1 << i);
//            }
//          }
//        }
        if (wparam == MAKEDWORD(ID_SECTOR_FLOOR_HEIGHT, EN_UPDATE)) {
          sector->floorheight = atoi(((window_t *)lparam)->title);
          build_wall_vertex_buffer(&game.map);
          build_floor_vertex_buffer(&game.map);
          invalidate_window(editor->window);
          invalidate_window(editor->game);
        }
        if (wparam == MAKEDWORD(ID_SECTOR_CEILING_HEIGHT, EN_UPDATE)) {
          sector->ceilingheight = atoi(((window_t *)lparam)->title);
          build_wall_vertex_buffer(&game.map);
          build_floor_vertex_buffer(&game.map);
          invalidate_window(editor->window);
          invalidate_window(editor->game);
        }
      }
      return true;
  }
  return false;
}
