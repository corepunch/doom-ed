#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <GL/gl.h>

int main(int argc, char* argv[]) {
  SDL_Window* window = NULL;
  SDL_GLContext ctx;
  
  printf("Testing OpenGL context creation...\n\n");
  
  // Initialize SDL video subsystem first
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }
  
  // Set OpenGL attributes before creating window
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  
  // Create window
  window = SDL_CreateWindow("GL Version Test",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            800, 600,
                            SDL_WINDOW_OPENGL);
  
  if (!window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }
  
  // Create OpenGL context
  ctx = SDL_GL_CreateContext(window);
  if (!ctx) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    window = NULL;
    return 1;
  }
  
  printf("GL_VERSION  : %s\n", glGetString(GL_VERSION));
  printf("GLSL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  
  printf("\nContext created successfully!\n");
  printf("Expected: OpenGL 3.2+ and GLSL 1.50+\n");
  
  // Clean up
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();
  
  return 0;
}
