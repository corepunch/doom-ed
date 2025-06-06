#include "map.h"
#include "editor.h"
#include "sprites.h"

#ifdef HEXEN
#include "../../hexen/info.h"
#include "../../hexen/ed_config.h"
#else
#include "../../doom/info.h"
#endif

static int num_items = 0;
static mobjinfo_t ed_objs[NUMMOBJTYPES];

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

sprite_t const *get_thing_sprite(int index, mobjinfo_t const *mobjinfo) {
  mobjinfo_t const *mobj = &mobjinfo[index];
  state_t const *state = &states[mobj->spawnstate];
  const char *name = sprnames[state->sprite];
  return find_sprite(name);
}

texdef_t get_mobj_size(int index, void *_things) {
  //  mobjinfo_t const *mobj = &((mobjinfo_t const *)_things)[index];
  sprite_t const *spr = get_thing_sprite(index, _things);
  if (spr) {
    float scale = fminf(1,fminf(((float)THUMBNAIL_SIZE) / spr->width,
                                ((float)THUMBNAIL_SIZE) / spr->height));
    return (texdef_t){
      .name = spr->name,
      .width = spr->width * scale,
      .height = spr->height * scale
    };
  } else {
    return (texdef_t){0};
  }
}

result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
  extern state_t states[NUMSTATES];
  extern char *sprnames[NUMSPRITES];
  int texture_idx;
  editor_state_t *editor = get_editor();
  switch (msg) {
    case WM_CREATE:
      num_items = 0;
      for (int i = 0; i < NUMMOBJTYPES; i++) {
        if (mobjinfo[i].doomednum == -1)
          continue;
        ed_objs[num_items++] = mobjinfo[i];
      }
      win->userdata2 = lparam;
      win->userdata = layout(num_items, win->frame.w, get_mobj_size, ed_objs);
      break;
    case WM_PAINT:
      for (int i = 0; i < num_items; i++) {
        int mobjindex;
        int pos = get_layout_item(win->userdata, i, &mobjindex);
        if (mobjindex == -1) continue;
        sprite_t const *spr = get_thing_sprite(mobjindex, ed_objs);
        if (spr) {
          float scale = fminf(1,fminf(((float)THUMBNAIL_SIZE) / spr->width,
                                      ((float)THUMBNAIL_SIZE) / spr->height));
          draw_rect(spr->texture,LOWORD(pos),HIWORD(pos),spr->width * scale,spr->height * scale);
          mobjinfo_t const *mobj = &ed_objs[mobjindex];
          if (editor->selected_thing_type == mobj->doomednum) {
            extern int selection_tex;
            draw_rect_ex(selection_tex, LOWORD(pos), HIWORD(pos), spr->width * scale, spr->height * scale, true, 1);
          }
        }
      }
      break;
    case WM_RESIZE:
      free(win->userdata);
      win->userdata = layout(num_items, win->frame.w, get_mobj_size, ed_objs);
      return true;
    case WM_LBUTTONUP:
      if ((texture_idx=get_texture_at_point(win->userdata,
                                            LOWORD(wparam),
                                            HIWORD(wparam))) >= 0)
      {
        mobjinfo_t const *mobj = &ed_objs[texture_idx];
        // printf("%d %s\n", texture_idx, get_thing_sprite(texture_idx, ed_objs)->name);
        // if (g_game && has_selection(editor->selected, obj_thing)) {
        //   g_game->map.things[editor->selected.index].type = mobj->doomednum;
        //   invalidate_window(editor->window);
        // }
        // editor->selected_thing_type = mobj->doomednum;
        // memcpy(udata->cache->selected, udata->cache->textures[texture_idx].name, sizeof(texname_t));
        end_dialog(win, mobj->doomednum);
        return true;
      }
      invalidate_window(win);
      return true;
  }
  return false;
}
