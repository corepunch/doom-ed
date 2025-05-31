#include "../../editor.h"

enum {
  ID_LINE_TYPE = 1000,
  ID_LINE_ARG1,
  ID_LINE_ARG2,
  ID_LINE_ARG3,
  ID_LINE_ARG4,
  ID_LINE_ARG5,
  ID_LINE_START,
  ID_LINE_END,
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

#define ICON 44

windef_t line_layout[] = {
  { "TEXT", "Type:", -1 },
  { "EDITTEXT", "", ID_LINE_TYPE, 32 }, { "SPACE" },
  { "TEXT", "Arguments:", -1, WIDTH_FILL },
  { "EDITTEXT", "", ID_LINE_ARG1, 24 },
  { "EDITTEXT", "", ID_LINE_ARG2, 24 },
  { "EDITTEXT", "", ID_LINE_ARG3, 24 },
  { "EDITTEXT", "", ID_LINE_ARG4, 24 },
  { "EDITTEXT", "", ID_LINE_ARG5, 24 },
  { "SPACE" },
  { "TEXT", "Vertices:", -1 },
  { "EDITTEXT", "", ID_LINE_START, 40 },
  { "EDITTEXT", "", ID_LINE_END, 40 },
  { "SPACE" },
  { "TEXT", "Front:", -1, WIDTH_FILL },
  { "TEXT", "x:", -1 },
  { "EDITTEXT", "", ID_LINE_FRONT_X, 32 },
  { "TEXT", "y:", -1 },
  { "EDITTEXT", "", ID_LINE_FRONT_Y, 32 }, { "SPACE" },
  { "SPRITE", "", ID_LINE_FRONT_BTM, ICON, ICON },
  { "SPRITE", "", ID_LINE_FRONT_MID, ICON, ICON },
  { "SPRITE", "", ID_LINE_FRONT_TOP, ICON, ICON },
  { "TEXT", "Back:", -1, WIDTH_FILL },
  { "TEXT", "x:", -1 },
  { "EDITTEXT", "", ID_LINE_BACK_X, 32 },
  { "TEXT", "y:", -1 },
  { "EDITTEXT", "", ID_LINE_BACK_Y, 32 }, { "SPACE" },
  { "SPRITE", "", ID_LINE_BACK_BTM, ICON, ICON },
  { "SPRITE", "", ID_LINE_BACK_MID, ICON, ICON },
  { "SPRITE", "", ID_LINE_BACK_TOP, ICON, ICON },
  { NULL }
};

maplinedef_t *selected_line(game_t *game) {
  if (!g_game) {
    return NULL;
  } else if (game->state.hover.line != 0xFFFF) {
    return &game->map.linedefs[game->state.hover.line];
  } else if (game->state.selected.line != 0xFFFF) {
    return &game->map.linedefs[game->state.selected.line];
  } else {
    return NULL;
  }
}

result_t win_line(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = get_editor();
  maplinedef_t *line;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      editor->inspector = win;
      load_window_children(win, line_layout);
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_lines && (line = selected_line(g_game))) {
        set_window_item_text(win, ID_LINE_TYPE, "%d", line->special);
        set_window_item_text(win, ID_LINE_START, "%d", line->start);
        set_window_item_text(win, ID_LINE_END, "%d", line->end);
        for (int i = 0; i < 5; i++) {
          set_window_item_text(win, ID_LINE_ARG1+i, "%d", line->args[i]);
        }
        if (line->sidenum[0] != 0xFFFF && g_game) {
          mapsidedef_t *front = &g_game->map.sidedefs[line->sidenum[0]];
          set_window_item_text(win, ID_LINE_FRONT_X, "%d", front->textureoffset);
          set_window_item_text(win, ID_LINE_FRONT_Y, "%d", front->rowoffset);
          set_window_item_text(win, ID_LINE_FRONT_TOP, front->toptexture);
          set_window_item_text(win, ID_LINE_FRONT_MID, front->midtexture);
          set_window_item_text(win, ID_LINE_FRONT_BTM, front->bottomtexture);
        }
        if (line->sidenum[1] != 0xFFFF && g_game) {
          mapsidedef_t *back = &g_game->map.sidedefs[line->sidenum[1]];
          set_window_item_text(win, ID_LINE_BACK_X, "%d", back->textureoffset);
          set_window_item_text(win, ID_LINE_BACK_Y, "%d", back->rowoffset);
          set_window_item_text(win, ID_LINE_BACK_TOP, back->toptexture);
          set_window_item_text(win, ID_LINE_BACK_MID, back->midtexture);
          set_window_item_text(win, ID_LINE_BACK_BTM, back->bottomtexture);
        }
      }
      return false;
    case WM_COMMAND:
      if (editor->sel_mode == edit_lines && (line = selected_line(g_game))) {
#define SET_OFFSET(ID, SIDE, OFFSET) \
        if (wparam == MAKEDWORD(ID, EN_UPDATE) && line->sidenum[SIDE] != 0xFFFF && g_game) { \
          mapsidedef_t *side = &g_game->map.sidedefs[line->sidenum[SIDE]]; \
          side->OFFSET = atoi(((window_t *)lparam)->title); \
          build_wall_vertex_buffer(&g_game->map); \
          build_floor_vertex_buffer(&g_game->map); \
          invalidate_window(editor->window); \
        }
        SET_OFFSET(ID_LINE_FRONT_X, 0, textureoffset);
        SET_OFFSET(ID_LINE_FRONT_Y, 0, rowoffset);
        SET_OFFSET(ID_LINE_BACK_X, 1, textureoffset);
        SET_OFFSET(ID_LINE_BACK_Y, 1, rowoffset);
      }
      return true;
  }
  return false;
}
