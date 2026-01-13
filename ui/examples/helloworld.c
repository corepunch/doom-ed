// UI Framework Hello World Example
// Demonstrates basic window creation and text display using the UI framework

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// Include UI framework headers
#include "../ui.h"

// Screen dimensions
int screen_width = 800;
int screen_height = 600;

// Global SDL window
SDL_Window* window = NULL;
SDL_GLContext ctx = NULL;

// Running flag
bool running = true;

// Simple window procedure for our hello world window
result_t hello_window_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case WM_CREATE:
      return true;
      
    case WM_PAINT: {
      // Draw hello world text
      const char *text = "Hello World!";
      int text_x = win->frame.w / 2 - 40;  // Center approximately
      int text_y = win->frame.h / 2 - 10;
      
      // Draw text with shadow effect
      draw_text_small(text, text_x + 1, text_y + 1, COLOR_DARK_EDGE);
      draw_text_small(text, text_x, text_y, COLOR_TEXT_NORMAL);
      return true;
    }
    
    case WM_DESTROY:
      running = false;
      return true;
      
    default:
      return false;
  }
}

// Initialize SDL and OpenGL
bool init_sdl_gl(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Set OpenGL version
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  // Create window
  window = SDL_CreateWindow(
    "UI Framework - Hello World",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    screen_width,
    screen_height,
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
  );

  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Create OpenGL context
  ctx = SDL_GL_CreateContext(window);
  if (!ctx) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  // Enable VSync
  SDL_GL_SetSwapInterval(1);

  return true;
}

// Simple main function
int main(int argc, char* argv[]) {
  printf("UI Framework Hello World Example\n");
  printf("=================================\n\n");

  // Initialize SDL and OpenGL
  if (!init_sdl_gl()) {
    printf("Failed to initialize!\n");
    return 1;
  }

  printf("SDL and OpenGL initialized successfully\n");
  printf("Creating window with UI framework...\n");

  // Create a desktop window
  rect_t desktop_rect = {0, 0, screen_width, screen_height};
  window_t *desktop = create_window(
    "Desktop",
    WINDOW_NOTITLE | WINDOW_NORESIZE | WINDOW_NOFILL,
    &desktop_rect,
    NULL,
    NULL,
    NULL
  );
  desktop->visible = true;

  // Create hello world window  
  rect_t hello_rect = {250, 200, 300, 200};
  window_t *hello_win = create_window(
    "Hello World Window",
    0,  // Default flags (has title, resizable, filled)
    &hello_rect,
    NULL,
    hello_window_proc,
    NULL
  );
  hello_win->visible = true;

  printf("Window created successfully!\n");
  printf("Press the close button to exit.\n\n");

  // Main event loop
  SDL_Event e;
  while (running) {
    // Process SDL events
    while (get_message(&e)) {
      dispatch_message(&e);
    }

    // Process window messages
    repost_messages();

    // Clear screen
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Draw all windows
    draw_windows(true);

    // Swap buffers
    SDL_GL_SwapWindow(window);
    
    // Small delay to avoid busy waiting
    SDL_Delay(16);  // ~60 FPS
  }

  // Cleanup
  printf("Shutting down...\n");
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  printf("Goodbye!\n");
  return 0;
}
