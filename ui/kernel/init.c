// Graphics context initialization and management
// Abstraction layer over SDL/OpenGL

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "../user/gl_compat.h"
#include "kernel.h"

// Global SDL/OpenGL context (defined here, declared in kernel.h)
SDL_Window *window = NULL;
SDL_GLContext ctx = NULL;
bool running = true;

// Screen dimensions (defined here, declared in kernel.h)
int screen_width = 1440;  // Default values, can be updated
int screen_height = 960;

// Initialize graphics context (SDL + OpenGL)
bool ui_init_graphics(const char *title, int width, int height) {
  // Initialize SDL video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Set OpenGL attributes
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  // Create SDL window
  window = SDL_CreateWindow(
    title,
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    width,
    height,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
  );

  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_Quit();
    return false;
  }

  // Create OpenGL context
  ctx = SDL_GL_CreateContext(window);
  if (!ctx) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);

  return true;
}

// Shutdown graphics context
void ui_shutdown_graphics(void) {
  if (ctx) {
    SDL_GL_DeleteContext(ctx);
    ctx = NULL;
  }
  
  if (window) {
    SDL_DestroyWindow(window);
    window = NULL;
  }
  
  SDL_Quit();
}

// Delay execution (abstraction over SDL_Delay)
void ui_delay(unsigned int milliseconds) {
  SDL_Delay(milliseconds);
}
