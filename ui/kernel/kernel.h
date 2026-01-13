#ifndef __UI_KERNEL_H__
#define __UI_KERNEL_H__

#include <stdbool.h>
#include <SDL2/SDL.h>

// Forward declarations
typedef union SDL_Event SDL_Event;

// Event message queue functions
int get_message(SDL_Event *evt);
void dispatch_message(SDL_Event *evt);
void repost_messages(void);

// SDL initialization
bool init_sdl(void);
int run(void);

// Global SDL objects
extern SDL_Window* window;
extern SDL_GLContext ctx;
extern SDL_Joystick* joystick;
extern bool running;
extern bool mode;
extern unsigned frame;

// Screen dimensions
extern int screen_width, screen_height;

#endif
