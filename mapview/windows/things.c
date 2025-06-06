#include "map.h"
#include "editor.h"
#include "sprites.h"

#ifdef HEXEN
#include "../../hexen/info.h"
#include "../../hexen/ed_config.h"
#else
#include "../../doom/info.h"
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

sprite_t const *get_thing_sprite(int index, ed_thing_t const *things) {
  static sprite_t empty = {
    .name = "EMPTY",
    .width = 8,
    .height = 8,
    .texture = 1,
  };
  if (things[index].sprite) {
    sprite_t const *spr = find_sprite(things[index].sprite);
    return spr ? spr : &empty;
  } else {
    return &empty;
  }
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

#define NUM_THINGS (sizeof(ed_things)/sizeof(*ed_things))

result_t win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
  extern state_t states[NUMSTATES];
  extern char *sprnames[NUMSPRITES];
  int texture_idx;
  editor_state_t *editor = get_editor();
  switch (msg) {
    case WM_CREATE:
      win->userdata2 = lparam;
      win->userdata = layout(NUM_THINGS, win->frame.w, get_mobj_size, ed_things);
      break;
    case WM_PAINT:
      for (int i = 0; i < NUM_THINGS; i++) {
        int mobjindex;
        int pos = get_layout_item(win->userdata, i, &mobjindex);
        if (mobjindex == -1) continue;
        sprite_t const *spr = get_thing_sprite(mobjindex, ed_things);
        if (spr) {
          float scale = fminf(1,fminf(((float)THUMBNAIL_SIZE) / spr->width,
                                      ((float)THUMBNAIL_SIZE) / spr->height));
          draw_rect(spr->texture,LOWORD(pos),HIWORD(pos),spr->width * scale,spr->height * scale);
          ed_thing_t const *mobj = &ed_things[mobjindex];
          if (editor->selected_thing_type == mobj->doomednum) {
            extern int selection_tex;
            draw_rect_ex(selection_tex, LOWORD(pos), HIWORD(pos), spr->width * scale, spr->height * scale, true, 1);
          }
        }
      }
      break;
    case WM_RESIZE:
      free(win->userdata);
      win->userdata = layout(NUM_THINGS, win->frame.w, get_mobj_size, ed_things);
      return true;
    case WM_LBUTTONUP:
      if ((texture_idx=get_texture_at_point(win->userdata,
                                            LOWORD(wparam),
                                            HIWORD(wparam))) >= 0)
      {
        ed_thing_t const *mobj = &ed_things[texture_idx];
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
