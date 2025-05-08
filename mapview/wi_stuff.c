#include <SDL2/SDL.h>

#include "map.h"
#include "sprites.h"

#define NUMEPISODES  4
#define NUMMAPS    9

typedef struct {
  int    x;
  int    y;
} point_t;

static point_t lnodes[NUMEPISODES][NUMMAPS] =
{
  // Episode 0 World Map
  {
    { 180, 174 },  // location of level 0 (CJ)
    { 148, 143 },  // location of level 1 (CJ)
    { 69, 122 },  // location of level 2 (CJ)
    { 224, 107 },  // location of level 3 (CJ)
    { 116, 89 },  // location of level 4 (CJ)
    { 176, 55 },  // location of level 5 (CJ)
    { 71, 56 },  // location of level 6 (CJ)
    { 135, 29 },  // location of level 7 (CJ)
    { 71, 24 }  // location of level 8 (CJ)
  },
  
  // Episode 1 World Map should go here
  {
    { 254, 25 },  // location of level 0 (CJ)
    { 97, 50 },  // location of level 1 (CJ)
    { 188, 64 },  // location of level 2 (CJ)
    { 128, 78 },  // location of level 3 (CJ)
    { 214, 92 },  // location of level 4 (CJ)
    { 133, 130 },  // location of level 5 (CJ)
    { 208, 136 },  // location of level 6 (CJ)
    { 148, 140 },  // location of level 7 (CJ)
    { 235, 158 }  // location of level 8 (CJ)
  },
  
  // Episode 2 World Map should go here
  {
    { 156, 168 },  // location of level 0 (CJ)
    { 48, 154 },  // location of level 1 (CJ)
    { 174, 95 },  // location of level 2 (CJ)
    { 265, 75 },  // location of level 3 (CJ)
    { 130, 48 },  // location of level 4 (CJ)
    { 279, 23 },  // location of level 5 (CJ)
    { 198, 48 },  // location of level 6 (CJ)
    { 140, 25 },  // location of level 7 (CJ)
    { 281, 136 }  // location of level 8 (CJ)
  }
  
};

void get_lnode(int e, int m, int *x, int *y) {
  *x = lnodes[e][m].x;
  *y = lnodes[e][m].y;
}

void init_intermission(void) {
  load_sprite("WIMAP0");
  load_sprite("WIURH0");
  load_sprite("WISPLAT");
  
  load_sprite("WILV00");
  
  load_sprite("WIURH0");
}

int selected = -1;
int current = -1;

void get_lnode(int e, int m, int *x, int *y);
void draw_intermission(void) {
  int window_width, window_height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &window_width, &window_height);
  sprite_t* INTERPIC = find_sprite("WIMAP0");
  if (INTERPIC) {
    draw_sprite("WIMAP0", 0, 0, 1, 1.0f);
  }
  
  for (int i = 0; i < 9; i++) {
    int x, y;
    get_lnode(0, i, &x, &y);
    if (current == i) {
      draw_sprite("WIURH0", x, y, 1, 1.0f);
    } else if (selected == i) {
      draw_sprite("WISPLAT", x, y, 1, 1.0f);
    }
  }

//
//  draw_sprite("WILV00",
//              window_width / 2,
//              window_height / 2,
//              scale/2, 1.0f);
}

void goto_intermisson(void) {
  SDL_SetRelativeMouseMode(SDL_FALSE);
  
  free_map_data(&game.map);

  game.state = GS_WORLD;
}

void handle_intermission_input(float delta_time) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    extern SDL_Window* window;
    int mouse_x, mouse_y;
    GetMouseInVirtualCoords(&mouse_x, &mouse_y);
    if (event.type == SDL_QUIT) {
      extern bool running;
      running = false;
    }
    switch (event.type) {
      case SDL_MOUSEMOTION:
        selected = -1;
        for (int i = 0; i < 9; i++) {
          if (abs(mouse_x - lnodes[0][i].x) < 20 && abs(mouse_y - lnodes[0][i].y) < 20) {
            selected = i;
          }
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (selected >= 0) {
          char name[64]={0};
          snprintf(name, 64, "E1M%d", selected+1);
          goto_map(name);
          current = selected;
        }
        break;
    }
  }
}
