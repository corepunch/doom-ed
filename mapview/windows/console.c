#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <stdarg.h>
#include <stdbool.h>
#include "../console.h"
#include "../map.h"

#define MAX_CONSOLE_MESSAGES 32
#define MESSAGE_DISPLAY_TIME 5000  // milliseconds
#define MESSAGE_FADE_TIME 1000     // fade out duration in milliseconds
#define MAX_MESSAGE_LENGTH 256
#define MAX_CONSOLE_LINES 10      // Maximum number of lines to display at once
#define CONSOLE_FONT_HEIGHT 8     // Height of the Doom font character
#define CONSOLE_FONT_WIDTH 8      // Width of each character

// Console message structure
typedef struct {
  char text[MAX_MESSAGE_LENGTH];
  Uint32 timestamp;  // When the message was created
  bool active;       // Whether the message is still being displayed
} console_message_t;

// Font character structure
typedef struct {
  GLuint texture;    // OpenGL texture ID
  int x, y;
  int width;         // Width of this character
  int height;        // Height of this character
} font_char_t;

float *get_sprite_matrix(void);

// Console state
static struct {
  console_message_t messages[MAX_CONSOLE_MESSAGES];
  int message_count;
  int last_message_index;
  bool show_console;
  font_char_t font[128];  // ASCII characters
} console = {0};

#ifdef HEXEN
// Hexen font lump name
static const char* HEXEN_FONT_LUMP = "FONTAY_S"; // "FONTA" or "FONTB" based on preference
#else
// Doom font lump names (original Doom font is stored in patches named STCFN*)
static const char* FONT_LUMPS_PREFIX = "STCFN";
#endif

// Forward declarations
void draw_text_gl3(const char* text, int x, int y, float alpha);
static bool load_font_char(int font, int char_code);

// Initialize console system
void init_console(void) {
  memset(&console, 0, sizeof(console));
  console.show_console = true;
  console.message_count = 0;
  console.last_message_index = -1;
}

// Load the appropriate font (Doom or Hexen)
bool load_console_font(void) {
  bool success = true;
  
#ifdef HEXEN
  // Load Hexen font
  int fontdata = find_lump_num(HEXEN_FONT_LUMP);
  if (fontdata == -1) {
    printf("Error: Could not load Hexen font lump %s\n", HEXEN_FONT_LUMP);
    return false;
  }
  
  for (int i = 33; i <= 95; i++) {
    if (!load_font_char(fontdata, i)) {
      // Not all characters might be present, that's fine
      printf("Warning: Could not load font character %c (%d)\n", (char)i, i);
      // Not counting this as a failure since some characters might be legitimately missing
    }
  }
  
  conprintf("Console initialized with Hexen font");
#else
  // Doom: Find and load all STCFN## font characters
  // In Doom, only certain ASCII characters are stored (33-95, i.e., ! through _)
  for (int i = 33; i <= 95; i++) {
    if (!load_font_char(0, i)) {
      // Not all characters might be present, that's fine
      printf("Warning: Could not load font character %c (%d)\n", (char)i, i);
      // Not counting this as a failure since some characters might be legitimately missing
    }
  }
  
  conprintf("Console initialized with DOOM font");
#endif
  
  return success;
}

// Load a specific font character for Doom
static bool load_font_char(int font, int char_code) {
  int width, height, leftoffset, topoffset;
  GLuint load_sprite_texture(void *data, int* width, int* height, int* offsetx, int* offsety);
  
#ifdef HEXEN
  int texture = load_sprite_texture(cache_lump_num(font+char_code-32), &width, &height, &leftoffset, &topoffset);
#else
  // Construct font lump name (e.g., "STCFN065" for 'A')
  char lump_name[16];
  snprintf(lump_name, sizeof(lump_name), "%s%03d", FONT_LUMPS_PREFIX, char_code);
  int texture = load_sprite_texture(cache_lump(lump_name), &width, &height, &leftoffset, &topoffset);
#endif
  
  // Store character data
  console.font[char_code].texture = texture;
  console.font[char_code].x = leftoffset;
  console.font[char_code].y = topoffset;
  console.font[char_code].width = width;
  console.font[char_code].height = height;
  
  return true;
}

// Print a message to the console
void conprintf(const char* format, ...) {
  va_list args;
  console_message_t* msg;
  
  // Find the next message slot
  int index = (console.last_message_index + 1) % MAX_CONSOLE_MESSAGES;
  console.last_message_index = index;
  
  // If we haven't filled the array yet, increment the count
  if (console.message_count < MAX_CONSOLE_MESSAGES) {
    console.message_count++;
  }
  
  // Get the message slot
  msg = &console.messages[index];
  
  // Format the message
  va_start(args, format);
  vsnprintf(msg->text, MAX_MESSAGE_LENGTH, format, args);
  va_end(args);
  
  // Set the timestamp
  msg->timestamp = SDL_GetTicks();
  msg->active = true;
  
  // Also print to stdout for debugging
  printf("%s\n", msg->text);
}

// Draw a single character
static void draw_char_gl3(char c, int x, int y, float alpha) {
#ifdef HEXEN
  int char_code = toupper(c);
#else
  int char_code = (int)(c);
#endif
  
  // Only handle characters we have textures for
  if (char_code < 0 || char_code >= 128 || console.font[char_code].texture == 0) {
    // Fallback for unsupported characters - draw a solid rectangle
    if (c != ' ') {  // Skip spaces
                     // We'll use the shader but with a white 1x1 texture
                     // Generate a 1x1 white texture if needed
      extern GLuint white_tex;
      draw_rect(white_tex, x, y, CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT);
    }
    return;
  }
  
  font_char_t* ch = &console.font[char_code];
  
  draw_rect(ch->texture, x - console.font[char_code].x, y - console.font[char_code].y, ch->width, ch->height);
}

// Draw text string using GL3
void draw_text_gl3(const char* text, int x, int y, float alpha) {
  int cursor_x = x;
  
  // Set up GL state
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  
  // Draw each character
#ifdef HEXEN
  // Hexen font is already uppercase
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
#else
    // Doom font is uppercase only, convert lowercase to uppercase
    //    for (int i = 0; text[i] != '\0'; i++) {
    //      char c = toupper(text[i]);
#endif
    draw_char_gl3(c, cursor_x, y, alpha);
    
    // Advance cursor
    if (c >= 32 && c > 0) {
      // Use character width if available, otherwise use default
      int char_width = console.font[c].width;
      cursor_x += (char_width > 0 ? char_width : CONSOLE_FONT_WIDTH);
    } else {
      cursor_x += CONSOLE_FONT_WIDTH;  // Default width for unsupported chars
    }
  }
  
  // Restore GL state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

// Draw the console
void draw_console(void) {
  if (!console.show_console) return;
  
  Uint32 current_time = SDL_GetTicks();
  int y = CONSOLE_PADDING;
  int lines_shown = 0;
  
  // Start from the most recent message and go backwards
  for (int i = 0; i < console.message_count && lines_shown < MAX_CONSOLE_LINES; i++) {
    int msg_index = (console.last_message_index - i + MAX_CONSOLE_MESSAGES) % MAX_CONSOLE_MESSAGES;
    console_message_t* msg = &console.messages[msg_index];
    
    // Check if the message should still be displayed
    Uint32 age = current_time - msg->timestamp;
    if (age < MESSAGE_DISPLAY_TIME) {
      // Calculate alpha based on age (fade out during the last second)
      float alpha = 1.0f;
      if (age > MESSAGE_DISPLAY_TIME - MESSAGE_FADE_TIME) {
        alpha = (MESSAGE_DISPLAY_TIME - age) / (float)MESSAGE_FADE_TIME;
      }
      
      // Draw the message
      draw_text_gl3(msg->text, CONSOLE_PADDING, y, alpha);
      
      // Move to next line
      y += LINE_HEIGHT;
      lines_shown++;
    } else {
      // Mark message as inactive
      msg->active = false;
    }
  }
}

// Clean up console resources
void shutdown_console(void) {
  // Delete all font textures
  for (int i = 0; i < 128; i++) {
    if (console.font[i].texture != 0) {
      glDeleteTextures(1, &console.font[i].texture);
      console.font[i].texture = 0;
    }
  }
}

// Toggle console visibility
void toggle_console(void) {
  console.show_console = !console.show_console;
}

bool win_console(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  switch (msg) {
    case MSG_DRAW:
      draw_console();
      break;
    default:
      break;
  }
  return false;
}
