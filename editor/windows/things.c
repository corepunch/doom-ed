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

#define NUM_THINGS (sizeof(ed_things)/sizeof(*ed_things))
#define THING_LABEL_HEIGHT 16

rect_t fit_sprite(sprite_t const *spr, rect_t const *target);

result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
  extern state_t states[NUMSTATES];
  extern char *sprnames[NUMSPRITES];
//  editor_state_t *editor = get_editor();
  switch (msg) {
    case evCreate:
      win->flags |= WINDOW_TOOLBAR;
      win->userdata2 = lparam;
    {
      toolbar_item_t but[8];
      for (int i = 0; i < 8; i++) {
        but[i] = (toolbar_item_t){ TOOLBAR_ITEM_BUTTON, i, 16+i, 0, 0, NULL };
      }
      send_message(win, tbSetItems, 8, but);
    }
      break;
    case evPaint:
      for (int i = 0, j = 0; i < NUM_THINGS; i++) {
        sprite_t *spr = ed_things[i].sprite ? find_sprite(ed_things[i].sprite) : NULL;
        if (spr && ed_things[i].code1 == ed_thinggroups[win->cursor_pos].code) {
          uint16_t n = win->frame.w / THING_SIZE;
          uint16_t x = (j % n) * THING_SIZE;
          uint16_t y = (j / n) * (THING_SIZE+THING_LABEL_HEIGHT);
          uint16_t tx = x + (THING_SIZE-strwidth(ed_things[i].sprite))/2;
          rect_t r = fit_sprite(spr, &(rect_t){ x, y, THING_SIZE, THING_SIZE });
          draw_rect(spr->texture, &r);
          draw_text_small(ed_things[i].sprite, tx, y + THING_SIZE+4, get_sys_color(brTextNormal));
          j++;
        }
      }
      break;
    case evResize:
      invalidate_window(win);
      return true;
    case evLeftButtonUp:
      for (int i = 0, j = 0; i < NUM_THINGS; i++) {
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
    case tbButtonClick:
      free(win->userdata);
      win->cursor_pos = wparam;
      send_message(win, tbSetActiveButton, wparam, NULL);
      invalidate_window(win);
      return true;
  }
  return false;
}
