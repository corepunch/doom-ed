//
//  ui_sdl.c
//  UI Framework - SDL initialization and event loop
//
//  Extracted from DOOM-ED mapview/renderer.c
//

#include "ui_framework.h"
#include <SDL2/SDL.h>
#include <stdio.h>

extern bool running; // Application provides this

bool init_sdl_window(SDL_Window **window, int screen_width, int screen_height) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  *window = SDL_CreateWindow("Application",
                             SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             screen_width, screen_height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS);
  
  if (!*window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }
  
  return true;
}

void cleanup_sdl(SDL_Window *window) {
  if (window) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
}

int run_event_loop(bool *running) {
  while (*running) {
    SDL_Event event;
    while (get_message(&event)) {
      dispatch_message(&event);
    }
    repost_messages();
  }
  return 0;
}
