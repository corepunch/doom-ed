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

#define FONT_TEX_SIZE 256

#define MAX_TEXT_LENGTH 256

typedef struct {
  int16_t x, y;
  float u, v;
  uint32_t col;
} text_vertex_t;

// Console message structure
typedef struct {
  char text[MAX_MESSAGE_LENGTH];
  Uint32 timestamp;  // When the message was created
  bool active;       // Whether the message is still being displayed
} console_message_t;

// Font atlas structure
typedef struct {
  GLuint vao, vbo;
  GLuint texture;    // Atlas texture ID
  uint8_t char_from[256];    // Width of each character in pixels
  uint8_t char_to[256];    // Width of each character in pixels
  uint8_t char_height;   // Height of each character in pixels
  uint8_t chars_per_row; // Number of characters per row in atlas
  uint8_t total_chars;   // Total number of characters in atlas
} font_atlas_t;

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
  font_atlas_t small_font;  // Small 6x8 font atlas
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
static bool create_font_atlas(void);

// Initialize console system
void init_console(void) {
  memset(&console, 0, sizeof(console));
  console.show_console = true;
  console.message_count = 0;
  console.last_message_index = -1;
  create_font_atlas();
}

#define SMALL_FONT_WIDTH 8
#define SMALL_FONT_HEIGHT 8
// Create texture atlas for the small 6x8 font
static bool create_font_atlas(void) {
  extern unsigned char console_font_6x8[];
  // Font atlas dimensions
  const int char_width = SMALL_FONT_WIDTH;
  const int char_height = SMALL_FONT_HEIGHT;
  const int chars_per_row = 16;
  const int rows = 8;      // 16 * 8 = 128 ASCII characters (0-127)
  
  // Create a buffer for the atlas texture
  unsigned char* atlas_data = (unsigned char*)calloc(FONT_TEX_SIZE * FONT_TEX_SIZE, sizeof(unsigned char));
  if (!atlas_data) {
    printf("Error: Could not allocate memory for font atlas\n");
    return false;
  }
  
  // Fill the atlas with character data from the font_6x8 array
  for (int c = 0; c < 128; c++) {
    int atlas_x = (c % chars_per_row) * char_width;
    int atlas_y = (c / chars_per_row) * char_height;
    // Copy character bits from font data to atlas
    console.small_font.char_to[c] = 0;
    console.small_font.char_from[c] = 0xff;
    for (int y = 0; y < char_height; y++) {
      for (int x = 0; x < char_width; x++) {
        // Get bit from font data (assuming 1 byte per row, 8 rows per character)
        int bit_pos = x;
        int font_byte = console_font_6x8[c * char_height + y];
        int bit_value = ((font_byte >> (char_width - 1 - bit_pos)) & 1);
        // Set corresponding pixel in atlas (convert 1-bit to 8-bit)
        atlas_data[(atlas_y + y) * FONT_TEX_SIZE + atlas_x + x] = bit_value ? 255 : 0;
        if (bit_value) {
          console.small_font.char_from[c] = MIN(x, console.small_font.char_from[c]);
          console.small_font.char_to[c] = MAX(x+1, console.small_font.char_to[c]);
        }
      }
    }
  }
  
  // Create OpenGL texture for the atlas
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  // Use texture swizzling for proper rendering (R channel becomes alpha)
  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  
  // Upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_TEX_SIZE, FONT_TEX_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_data);
  
  // Store atlas information
  console.small_font.texture = texture;
  console.small_font.char_height = char_height;
  console.small_font.chars_per_row = chars_per_row;
  console.small_font.total_chars = chars_per_row * rows;
  
  // Free temporary buffer
  free(atlas_data);
  
  conprintf("Small font atlas created successfully");
  
  glGenVertexArrays(1, &console.small_font.vao);
  glGenBuffers(1, &console.small_font.vbo);
  
  glBindVertexArray(console.small_font.vao);
  glBindBuffer(GL_ARRAY_BUFFER, console.small_font.vbo);

  return true;
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

int get_text_width(const char* text) {
  int cursor_x = 0;
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
    
    // Advance cursor
    if (c >= 32 && c > 0) {
      // Use character width if available, otherwise use default
      int char_width = console.font[c].width;
      cursor_x += (char_width > 0 ? char_width : CONSOLE_FONT_WIDTH);
    } else {
      cursor_x += CONSOLE_FONT_WIDTH;  // Default width for unsupported chars
    }
  }
  return cursor_x;
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
    case MSG_PAINT:
      draw_console();
      break;
    default:
      break;
  }
  return false;
}

void push_sprite_args(int tex, int x, int y, int w, int h, float alpha);

int get_small_text_width(const char* text) {
  if (!text || !*text) return 0; // Early return for empty strings
  
  int text_length = (int)strlen(text);
  if (text_length > MAX_TEXT_LENGTH) text_length = MAX_TEXT_LENGTH;
  
  int cursor_x = 0;
  
  // Pre-calculate all vertices for the entire string
  for (int i = 0; i < text_length; i++) {
    char c = text[i];
    uint8_t w = console.small_font.char_to[c] - console.small_font.char_from[c];
    // Advance cursor position
    cursor_x += w+1;
  }
  return cursor_x;
}

void draw_text_small(const char* text, int x, int y, uint32_t col) {
  if (!text || !*text) return; // Early return for empty strings
  
  int text_length = (int)strlen(text);
  if (text_length > MAX_TEXT_LENGTH) text_length = MAX_TEXT_LENGTH;
  
  static text_vertex_t buffer[MAX_TEXT_LENGTH * 6]; // 6 vertices per character (2 triangles)
  int vertex_count = 0;
  
  int cursor_x = x;
  
  // Pre-calculate all vertices for the entire string
  for (int i = 0; i < text_length; i++) {
    char c = text[i];
    
    // Calculate texture coordinates
    int char_code = (int)c;
    int atlas_x = (char_code % console.small_font.chars_per_row) * SMALL_FONT_WIDTH;
    int atlas_y = (char_code / console.small_font.chars_per_row) * SMALL_FONT_HEIGHT;
    
    // Convert to normalized UV coordinates (0-255 range for uint8_t)
    float u1 = (atlas_x + console.small_font.char_from[c])/256.f;
    float v1 = (atlas_y)/256.f;
    float u2 = (atlas_x + console.small_font.char_to[c])/256.f;
    float v2 = (atlas_y + SMALL_FONT_HEIGHT)/256.f;
    
    uint8_t w = console.small_font.char_to[c] - console.small_font.char_from[c];
    uint8_t h = SMALL_FONT_HEIGHT;
    
    // Skip spaces to save rendering effort
    if (c != ' ') {
      // First triangle (bottom-left, top-left, bottom-right)
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y, u1, v1, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y + h, u1, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y, u2, v1, col };
      
      // Second triangle (top-left, top-right, bottom-right)
      buffer[vertex_count++] = (text_vertex_t) { cursor_x, y + h, u1, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y + h, u2, v2, col };
      buffer[vertex_count++] = (text_vertex_t) { cursor_x + w, y, u2, v1, col };
    }
    
    // Advance cursor position
    cursor_x += w+1;
  }
  
  // Early return if nothing to draw
  if (vertex_count == 0) return;
  
  // Set up GL state
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  
  // Get locations for shader uniforms
  push_sprite_args(console.small_font.texture, 0, 0, 1, 1, 1);
  
  // Bind font texture
  glBindTexture(GL_TEXTURE_2D, console.small_font.texture);
  
  // Bind and update the VBO with the vertex data
  glBindVertexArray(console.small_font.vao);
  glBindBuffer(GL_ARRAY_BUFFER, console.small_font.vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(text_vertex_t), buffer, GL_DYNAMIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, x)); // Position
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, u)); // UV
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(text_vertex_t), (void*)offsetof(text_vertex_t, col)); // UV

  // Draw all vertices in one call
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);
  
  // Clean up
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  
  // Restore GL state
//  glEnable(GL_DEPTH_TEST);
//  glDisable(GL_BLEND);
}
