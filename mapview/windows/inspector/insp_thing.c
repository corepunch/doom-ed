#include "../../editor.h"
#include "../../sprites.h"

#define MTF_EASY    1
#define MTF_NORMAL    2
#define MTF_HARD    4
#define MTF_AMBUSH    8
#define MTF_DORMANT    16
#define MTF_FIGHTER    32
#define MTF_CLERIC    64
#define MTF_MAGE    128
#define MTF_GSINGLE    256
#define MTF_GCOOP    512
#define MTF_GDEATHMATCH  1024

enum {
  ID_THING_TYPE = 1000,
  ID_THING_POS_X,
  ID_THING_POS_Y,
  ID_THING_ANGLE,
  ID_THING_SPRITE,
  ID_THING_EASY,
  ID_THING_NORMAL,
  ID_THING_HARD,
  ID_THING_FIGHTER,
  ID_THING_CLERIC,
  ID_THING_MAGE,
  ID_THING_GSINGLE,
  ID_THING_GCOOP,
  ID_THING_GDEATHMATCH,
  ID_THING_AMBUSH,
  ID_THING_DORMANT,
};

windef_t thing_layout[] = {
  { "TEXT", "Type:", -1, LABEL_WIDTH },
  { "BUTTON", "Click me", ID_THING_TYPE, 50 },
  { "TEXT", "Position X:", -1, LABEL_WIDTH },
  { "EDITTEXT", "", ID_THING_POS_X, 50 },
  { "TEXT", "Position Y:", -1, LABEL_WIDTH },
  { "EDITTEXT", "", ID_THING_POS_Y, 50 },
  { "TEXT", "Angle:", -1, LABEL_WIDTH },
  { "EDITTEXT", "", ID_THING_ANGLE, 50 },
  { "SPRITE", "", ID_THING_SPRITE, 64, 64 },
  { "SPACE" },
  { "CHECKBOX", "Easy", ID_THING_EASY, 64 },
  { "CHECKBOX", "Medium", ID_THING_NORMAL, 64 },
  { "CHECKBOX", "Hard", ID_THING_HARD, 64 },
  { "CHECKBOX", "Fighter", ID_THING_FIGHTER, 64 },
  { "CHECKBOX", "Cleric", ID_THING_CLERIC, 64 },
  { "CHECKBOX", "Mage", ID_THING_MAGE, 64 },
  { "CHECKBOX", "Single", ID_THING_GSINGLE, 64 },
  { "CHECKBOX", "Coop", ID_THING_GCOOP, 64 },
  { "CHECKBOX", "Deathmatch", ID_THING_GDEATHMATCH, 64 },
  { "CHECKBOX", "Ambush", ID_THING_AMBUSH, 64 },
  { "CHECKBOX", "Dormant", ID_THING_DORMANT, 64 },
  { NULL }
};

uint32_t thing_checkboxes[] = {
  ID_THING_EASY,
  ID_THING_NORMAL,
  ID_THING_HARD,
  ID_THING_AMBUSH,
  ID_THING_DORMANT,
  ID_THING_FIGHTER,
  ID_THING_CLERIC,
  ID_THING_MAGE,
  ID_THING_GSINGLE,
  ID_THING_GCOOP,
  ID_THING_GDEATHMATCH,
};

mapthing_t *selected_thing(game_t *game) {
  if (has_selection(game->state.hover, obj_thing)) {
    return &game->map.things[game->state.hover.index];
  } else if (has_selection(game->state.selected, obj_thing)) {
    return &game->map.things[game->state.selected.index];
  } else {
    return NULL;
  }
}

result_t win_thing(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  editor_state_t *editor = get_editor();
  mapthing_t *thing;
  switch (msg) {
    case WM_CREATE:
      win->userdata = lparam;
      g_inspector = win;
      load_window_children(win, thing_layout);
      return true;
    case WM_PAINT:
      if (editor->sel_mode == edit_things && (thing = selected_thing(g_game))) {
        sprite_t *spr = get_thing_sprite_name(thing->type, 0);
        set_window_item_text(win, ID_THING_SPRITE, spr?spr->name:"");
        set_window_item_text(win, ID_THING_TYPE, "%d", thing->height);
        set_window_item_text(win, ID_THING_POS_X, "%d", thing->x);
        set_window_item_text(win, ID_THING_POS_Y, "%d", thing->y);
        set_window_item_text(win, ID_THING_ANGLE, "%d", thing->angle);
        for (int i = 0; i < sizeof(thing_checkboxes)/sizeof(*thing_checkboxes); i++) {
          window_t *checkbox = get_window_item(win, thing_checkboxes[i]);
          uint32_t value = thing->options&(1<<i);
          send_message(checkbox, BM_SETCHECK, value, NULL);
        }
      }
      return false;
    case WM_COMMAND:
      if (editor->sel_mode == edit_things && (thing = selected_thing(g_game))) {
        for (int i = 0; i < sizeof(thing_checkboxes)/sizeof(*thing_checkboxes); i++) {
          if (wparam == MAKEDWORD(thing_checkboxes[i], BN_CLICKED)) {
            if (send_message(lparam, BM_GETCHECK, 0, NULL)) {
              thing->options |= 1 << i;
            } else {
              thing->options &= ~(1 << i);
            }
          }
        }
        if (wparam == MAKEDWORD(ID_THING_POS_X, EN_UPDATE)) {
          thing->x = atoi(((window_t *)lparam)->title);
          invalidate_window(editor->window);
        }
        if (wparam == MAKEDWORD(ID_THING_POS_Y, EN_UPDATE)) {
          thing->y = atoi(((window_t *)lparam)->title);
          invalidate_window(editor->window);
        }
        if (wparam == MAKEDWORD(ID_THING_ANGLE, EN_UPDATE)) {
          thing->angle = atoi(((window_t *)lparam)->title);
          invalidate_window(editor->window);
        }
      }
      return true;
  }
  return false;
}
