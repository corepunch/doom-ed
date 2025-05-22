#include "../../editor.h"

enum {
  ID_LINE_TYPE = 1000,
  ID_LINE_FRONT_X,
  ID_LINE_FRONT_Y,
  ID_LINE_FRONT_BTM,
  ID_LINE_FRONT_MID,
  ID_LINE_FRONT_TOP,
  ID_LINE_BACK_X,
  ID_LINE_BACK_Y,
  ID_LINE_BACK_BTM,
  ID_LINE_BACK_MID,
  ID_LINE_BACK_TOP,
};

#define COL 40
#define LBL 12
#define X1 (4)
#define X2 (4+(COL+6))
#define X3 (4+(COL+6)*2)

#define TYPE_LINE 4
#define LINE1 TYPE_LINE+20
#define LINE2 LINE1+20
#define LINE3 LINE2+28
#define LINE4 LINE3+14
#define LINE5 LINE3+COL+24
#define LINE6 LINE5+14

windef_t line_layout[] = {
  { "TEXT", "Type:", -1, 4, TYPE_LINE+3, 50, 10 },
  { "EDITTEXT", "", ID_LINE_TYPE, 64, TYPE_LINE, 32, 10 },
  { "TEXT", "Front Ofs:", -1, 4, LINE1+3, 50, 10 },
  { "TEXT", "Back Ofs:", -1, 4, LINE2+3, 50, 10 },
  { "EDITTEXT", "", ID_LINE_FRONT_X, 64, LINE1, 32, 10 },
  { "EDITTEXT", "", ID_LINE_FRONT_Y, 102, LINE1, 32, 10 },
  { "EDITTEXT", "", ID_LINE_BACK_X, 64, LINE2, 32, 10 },
  { "EDITTEXT", "", ID_LINE_BACK_Y, 102, LINE2, 32, 10 },
  { "TEXT", "Front Sidedef:", -1, 4, LINE3, 50, 10 },
  { "SPRITE", "", ID_LINE_FRONT_BTM, X1, LINE4, COL, COL },
  { "SPRITE", "", ID_LINE_FRONT_MID, X2, LINE4, COL, COL },
  { "SPRITE", "", ID_LINE_FRONT_TOP, X3, LINE4, COL, COL },
  { "TEXT", "Back Sidedef:", -1, 4, LINE5, 50, 10 },
  { "SPRITE", "", ID_LINE_BACK_BTM, X1, LINE6, COL, COL },
  { "SPRITE", "", ID_LINE_BACK_MID, X2, LINE6, COL, COL },
  { "SPRITE", "", ID_LINE_BACK_TOP, X3, LINE6, COL, COL },
  { NULL }
};

maplinedef_t *selected_line(editor_state_t *editor) {
  if (editor->hover.line != 0xFFFF) {
    return &game.map.linedefs[editor->hover.line];
  } else if (editor->selected.line != 0xFFFF) {
    return &game.map.linedefs[editor->selected.line];
  } else {
    return NULL;
  }
}

bool win_line(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = win->userdata;
  maplinedef_t *line;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      ((editor_state_t*)lparam)->inspector = win;
      load_window_children(win, line_layout);
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_lines && (line = selected_line(editor))) {
        set_window_item_text(win, ID_LINE_TYPE, "%d", line->special);
        if (line->sidenum[0] != 0xFFFF) {
          mapsidedef_t *front = &game.map.sidedefs[line->sidenum[0]];
          set_window_item_text(win, ID_LINE_FRONT_X, "%d", front->textureoffset);
          set_window_item_text(win, ID_LINE_FRONT_Y, "%d", front->rowoffset);
          set_window_item_text(win, ID_LINE_FRONT_TOP, front->toptexture);
          set_window_item_text(win, ID_LINE_FRONT_MID, front->midtexture);
          set_window_item_text(win, ID_LINE_FRONT_BTM, front->bottomtexture);
        }
        if (line->sidenum[1] != 0xFFFF) {
          mapsidedef_t *back = &game.map.sidedefs[line->sidenum[1]];
          set_window_item_text(win, ID_LINE_BACK_X, "%d", back->textureoffset);
          set_window_item_text(win, ID_LINE_BACK_Y, "%d", back->rowoffset);
          set_window_item_text(win, ID_LINE_BACK_TOP, back->toptexture);
          set_window_item_text(win, ID_LINE_BACK_MID, back->midtexture);
          set_window_item_text(win, ID_LINE_BACK_BTM, back->bottomtexture);
        }
      }
      return false;
    case WM_COMMAND:
      if (editor->sel_mode == edit_lines && (line = selected_line(editor))) {
        mapsidedef_t *front = &game.map.sidedefs[line->sidenum[0]];
        mapsidedef_t *back = &game.map.sidedefs[line->sidenum[1]];
        //        for (int i = 0; i < sizeof(checkboxes)/sizeof(*checkboxes); i++) {
        //          if (wparam == MAKEDWORD(checkboxes[i], BN_CLICKED)) {
        //            if (send_message(lparam, BM_GETCHECK, 0, NULL)) {
        //              thing->options |= 1 << i;
        //            } else {
        //              thing->options &= ~(1 << i);
        //            }
        //          }
        //        }
        if (wparam == MAKEDWORD(ID_LINE_FRONT_X, EN_UPDATE)) {
          front->textureoffset = atoi(((window_t *)lparam)->title);
          build_wall_vertex_buffer(&game.map);
          build_floor_vertex_buffer(&game.map);
          invalidate_window(editor->window);
          invalidate_window(editor->game);
        }
        if (wparam == MAKEDWORD(ID_LINE_FRONT_Y, EN_UPDATE)) {
          front->rowoffset = atoi(((window_t *)lparam)->title);
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
