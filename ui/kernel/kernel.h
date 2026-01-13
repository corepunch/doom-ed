#ifndef __UI_KERNEL_H__
#define __UI_KERNEL_H__

#include <stdbool.h>
#include <SDL2/SDL.h>

// Event type abstraction
typedef SDL_Event ui_event_t;

// Event message queue functions
int get_message(ui_event_t *evt);
void dispatch_message(ui_event_t *evt);
void repost_messages(void);

// SDL initialization
bool init_sdl(void);
int run(void);

// Graphics context initialization (abstracted)
bool ui_init_graphics(const char *title, int width, int height);
void ui_shutdown_graphics(void);

// Timing functions
void ui_delay(unsigned int milliseconds);

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
