#include <mapview/map.h>
#include <editor/editor.h>
#include <mapview/sprites.h>

#ifdef HEXEN
#include <hexen/info.h>
#include <hexen/ed_config.h>
#else
#include <doom/info.h>
#endif

struct texture_layout_s*
layout(int num_textures,
       int layout_width,
       texdef_t (*get_size)(int, void *),
       void *param);

int
get_layout_item(struct texture_layout_s* layout,
                int index,
                int *texutre_index);

int get_texture_at_point(struct texture_layout_s* layout, int x, int y);

#define NUM_THINGGROUPS (int)(sizeof(ed_thinggroups)/sizeof(*ed_thinggroups))

static const toolbar_item_t thinggroup_buttons[] = {
  { TOOLBAR_ITEM_BUTTON,  0, sysicon_character, 0, 0, NULL }, // Player
  { TOOLBAR_ITEM_BUTTON,  1, sysicon_enemy,     0, 0, NULL }, // Monster
  { TOOLBAR_ITEM_BUTTON,  2, sysicon_sword,     0, 0, NULL }, // Weapon
  { TOOLBAR_ITEM_BUTTON,  3, sysicon_ammo,      0, 0, NULL }, // Ammunition
  { TOOLBAR_ITEM_BUTTON,  4, sysicon_heart,     0, 0, NULL }, // Health & Armor
  { TOOLBAR_ITEM_BUTTON,  5, sysicon_star,      0, 0, NULL }, // Bonus
  { TOOLBAR_ITEM_BUTTON,  6, sysicon_key_c,     0, 0, NULL }, // Key
  { TOOLBAR_ITEM_BUTTON,  7, sysicon_puzzle,    0, 0, NULL }, // Puzzle Piece
  { TOOLBAR_ITEM_BUTTON,  8, sysicon_tile,      0, 0, NULL }, // Decoration
  { TOOLBAR_ITEM_BUTTON,  9, sysicon_terrain,   0, 0, NULL }, // Natural
  { TOOLBAR_ITEM_BUTTON, 10, sysicon_sound,     0, 0, NULL }, // Sounds
  { TOOLBAR_ITEM_BUTTON, 11, sysicon_tag_id,    0, 0, NULL }, // OTHER
};
#define THING_LABEL_HEIGHT 16

irect16_t fit_sprite(sprite_t const *spr, irect16_t const *target);

result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
  extern state_t states[NUMSTATES];
  extern char *sprnames[NUMSPRITES];
//  editor_state_t *editor = get_editor();
  switch (msg) {
    case evCreate:
      win->flags |= WINDOW_TOOLBAR;
      win->userdata2 = lparam;
      send_message(win, tbSetItems, NUM_THINGGROUPS, (toolbar_item_t *)thinggroup_buttons);
      break;
    case evPaint: {
      int num_things = (int)(sizeof(ed_things)/sizeof(*ed_things));
      for (int i = 0, j = 0; i < num_things; i++) {
        sprite_t *spr = ed_things[i].sprite ? find_sprite(ed_things[i].sprite) : NULL;
        if (spr && ed_things[i].code1 == ed_thinggroups[win->cursor_pos].code) {
          uint16_t n = win->frame.w / THING_SIZE;
          uint16_t x = (j % n) * THING_SIZE;
          uint16_t y = (j / n) * (THING_SIZE+THING_LABEL_HEIGHT);
          uint16_t tx = x + (THING_SIZE-strwidth(ed_things[i].sprite))/2;
          irect16_t r = fit_sprite(spr, &(irect16_t){ x, y, THING_SIZE, THING_SIZE });
          draw_rect(spr->texture, r);
          draw_text_small(ed_things[i].sprite, tx, y + THING_SIZE+4, get_sys_color(brTextNormal));
          j++;
        }
      }
      break;
    }
    case evResize:
      invalidate_window(win);
      return true;
    case evLeftButtonUp: {
      int num_things = (int)(sizeof(ed_things)/sizeof(*ed_things));
      for (int i = 0, j = 0; i < num_things; i++) {
        sprite_t *spr = ed_things[i].sprite ? find_sprite(ed_things[i].sprite) : NULL;
        if (spr && ed_things[i].code1 == ed_thinggroups[win->cursor_pos].code) {
          uint16_t n = win->frame.w / THING_SIZE;
          uint16_t x = (j % n) * THING_SIZE;
          uint16_t y = (j / n) * (THING_SIZE+THING_LABEL_HEIGHT);
          if (LOWORD(wparam) >= x && HIWORD(wparam) >= y &&
              LOWORD(wparam) < x + THING_SIZE &&
              HIWORD(wparam) < y + THING_SIZE)
          {
            end_dialog(win, ed_things[i].doomednum);
            return true;
          }
          j++;
        }
      }
      return true;
    }
    case tbButtonClick:
      win->cursor_pos = wparam;
      send_message(win, tbSetActiveButton, wparam, NULL);
      invalidate_window(win);
      return true;
  }
  return false;
}
