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
    case kWindowMessageCreate:
      win->flags |= WINDOW_TOOLBAR;
      win->userdata2 = lparam;
//      win->userdata = layout(NUM_THINGS, win->frame.w, get_mobj_size, win);
    {
      toolbar_button_t but[8]={0};
      for (int i = 0; i < 8; i++) {
        but[i].icon = 16+i;
        but[i].ident = i;
        but[i].active = win->cursor_pos == i;
      }
      send_message(win, TB_ADDBUTTONS, sizeof(but)/sizeof(*but), but);
    }
//      send_message(win, TB_ADDBUTTONS, sizeof(but)/sizeof(*but), but);
      break;
    case kWindowMessagePaint:
      for (int i = 0, j = 0; i < NUM_THINGS; i++) {
        sprite_t *spr = ed_things[i].sprite ? find_sprite(ed_things[i].sprite) : NULL;
        if (spr && ed_things[i].code1 == ed_thinggroups[win->cursor_pos].code) {
          uint16_t n = win->frame.w / THING_SIZE;
          uint16_t x = (j % n) * THING_SIZE;
          uint16_t y = (j / n) * (THING_SIZE+THING_LABEL_HEIGHT);
          uint16_t tx = x + (THING_SIZE-strwidth(ed_things[i].sprite))/2;
          rect_t r = fit_sprite(spr, &(rect_t){ x, y, THING_SIZE, THING_SIZE });
          draw_rect(spr->texture, r.x, r.y, r.w, r.h);
          draw_text_small(ed_things[i].sprite, tx, y + THING_SIZE+4, COLOR_TEXT_NORMAL);
          j++;
        }
      }
      break;
    case WM_RESIZE:
      invalidate_window(win);
      return true;
    case kWindowMessageLeftButtonUp:
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
    case TB_BUTTONCLICK:
      free(win->userdata);
      win->cursor_pos = wparam;
      for (int i = 0; i < win->num_toolbar_buttons; i++) {
        win->toolbar_buttons[i].active = wparam == i;
      }
      invalidate_window(win);
      return true;
  }
  return false;
}
