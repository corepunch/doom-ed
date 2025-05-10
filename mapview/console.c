#include "console.h"
#include "map.h"
#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <stdarg.h>
#include <stdbool.h>

#define MAX_CONSOLE_MESSAGES 32
#define MESSAGE_DISPLAY_TIME 5000  // milliseconds
#define MESSAGE_FADE_TIME 1000     // fade out duration in milliseconds
#define MAX_MESSAGE_LENGTH 256
#define CONSOLE_PADDING 10
#define LINE_HEIGHT 16
#define MAX_CONSOLE_LINES 10      // Maximum number of lines to display at once
#define CONSOLE_FONT_HEIGHT 8     // Height of the Doom font character
#define CONSOLE_FONT_WIDTH 8      // Width of each character
#define CONSOLE_SCALE 2.0f        // Scale factor for console text

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

// Shader sources for text rendering
const char* text_vs_src = "#version 150 core\n"
"in vec2 position;\n"
"in vec2 texcoord;\n"
"out vec2 tex_coord;\n"
"uniform mat4 projection;\n"
"uniform vec2 offset;\n"
"uniform vec2 scale;\n"
"void main(void) {\n"
"  tex_coord = texcoord;\n"
"  gl_Position = projection * vec4(position * scale + offset, 0.0, 1.0);\n"
"}";

const char* text_fs_src = "#version 150 core\n"
"in vec2 tex_coord;\n"
"out vec4 out_color;\n"
"uniform sampler2D tex;\n"
"uniform vec4 color;\n"
"void main(void) {\n"
"  vec4 texel = texture(tex, tex_coord);\n"
"  out_color = texel * color;\n"
"  if(out_color.a < 0.1) discard;\n"
"}";

// Vertex data for rendering characters (quad)
float text_vertices[] = {
  // pos.x  pos.y    tex.x tex.y
  0.0f,    0.0f,    0.0f, 0.0f, // bottom left
  0.0f,    1.0f,    0.0f, 1.0f,  // top left
  1.0f,    1.0f,    1.0f, 1.0f, // top right
  1.0f,    0.0f,    1.0f, 0.0f, // bottom right
};

// Console state
static struct {
  console_message_t messages[MAX_CONSOLE_MESSAGES];
  int message_count;
  int last_message_index;
  bool show_console;
  font_char_t font[128];  // ASCII characters
  SDL_Color text_color;
  
  // OpenGL rendering state
  GLuint program;        // Shader program
  GLuint vao;            // Vertex array object
  GLuint vbo;            // Vertex buffer object
  GLint projection_loc;  // Location of projection uniform
  GLint offset_loc;      // Location of offset uniform
  GLint scale_loc;       // Location of scale uniform
  GLint color_loc;       // Location of color uniform
  GLint tex_loc;         // Location of texture uniform
  float projection[16];  // Orthographic projection matrix (4x4)
} console = {0};

#ifdef HEXEN
// Hexen font lump name
static const char* HEXEN_FONT_LUMP = "FONTAY_S"; // "FONTA" or "FONTB" based on preference
#else
// Doom font lump names (original Doom font is stored in patches named STCFN*)
static const char* FONT_LUMPS_PREFIX = "STCFN";
#endif

// Forward declarations
static void draw_text_gl3(const char* text, int x, int y, float alpha);
static bool load_font_char(int font, int char_code);
static GLuint compile_shader(GLenum type, const char* src);
static void create_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* out);

// Initialize console system
void init_console(void) {
  memset(&console, 0, sizeof(console));
  console.show_console = true;
  console.message_count = 0;
  console.last_message_index = -1;
  
  // Default text color: light gray
  console.text_color.r = 210;
  console.text_color.g = 210;
  console.text_color.b = 210;
  console.text_color.a = 255;
  
  // Create shader program
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, text_vs_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, text_fs_src);
  
  console.program = glCreateProgram();
  glAttachShader(console.program, vertex_shader);
  glAttachShader(console.program, fragment_shader);
  glBindAttribLocation(console.program, 0, "position");
  glBindAttribLocation(console.program, 1, "texcoord");
  glLinkProgram(console.program);
  
  // Check shader program link status
  GLint success;
  glGetProgramiv(console.program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(console.program, 512, NULL, info_log);
    printf("Shader program linking failed: %s\n", info_log);
  }
  
  // Get uniform locations
  console.projection_loc = glGetUniformLocation(console.program, "projection");
  console.offset_loc = glGetUniformLocation(console.program, "offset");
  console.scale_loc = glGetUniformLocation(console.program, "scale");
  console.color_loc = glGetUniformLocation(console.program, "color");
  console.tex_loc = glGetUniformLocation(console.program, "tex");
  
  // Create VAO and VBO
  glGenVertexArrays(1, &console.vao);
  glBindVertexArray(console.vao);
  
  glGenBuffers(1, &console.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, console.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(text_vertices), text_vertices, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  // Create orthographic projection matrix
  int width, height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &width, &height);
  create_orthographic_matrix(0, width, height, 0, -1, 1, console.projection);
  
  // Cleanup shaders
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
}

// Create a simple orthographic projection matrix
static void create_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* out) {
  // Column-major order as expected by OpenGL
  float tx = -(right + left) / (right - left);
  float ty = -(top + bottom) / (top - bottom);
  float tz = -(far + near) / (far - near);
  
  out[0] = 2.0f / (right - left);
  out[1] = 0.0f;
  out[2] = 0.0f;
  out[3] = 0.0f;
  
  out[4] = 0.0f;
  out[5] = 2.0f / (top - bottom);
  out[6] = 0.0f;
  out[7] = 0.0f;
  
  out[8] = 0.0f;
  out[9] = 0.0f;
  out[10] = -2.0f / (far - near);
  out[11] = 0.0f;
  
  out[12] = tx;
  out[13] = ty;
  out[14] = tz;
  out[15] = 1.0f;
}

// Compile a shader
static GLuint compile_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  
  // Check for errors
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    GLint log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    char* log = malloc(log_length);
    glGetShaderInfoLog(shader, log_length, NULL, log);
    printf("Shader compilation error: %s\n", log);
    free(log);
  }
  
  return shader;
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
      static GLuint white_texture = 0;
      if (white_texture == 0) {
        unsigned char white_pixel[4] = {255, 255, 255, 255};
        glGenTextures(1, &white_texture);
        glBindTexture(GL_TEXTURE_2D, white_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
      }
      
      glBindTexture(GL_TEXTURE_2D, white_texture);
      glUniform2f(console.scale_loc, CONSOLE_FONT_WIDTH * CONSOLE_SCALE, CONSOLE_FONT_HEIGHT * CONSOLE_SCALE);
      glUniform2f(console.offset_loc, x, y);
      glUniform4f(console.color_loc,
                  console.text_color.r / 255.0f,
                  console.text_color.g / 255.0f,
                  console.text_color.b / 255.0f,
                  alpha);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    return;
  }
  
  font_char_t* ch = &console.font[char_code];
  
  // Bind the texture
  glBindTexture(GL_TEXTURE_2D, ch->texture);
  
  // Set uniforms
  glUniform2f(console.scale_loc, ch->width * CONSOLE_SCALE, ch->height * CONSOLE_SCALE);
  glUniform2f(console.offset_loc,
              x - console.font[char_code].x * CONSOLE_SCALE,
              y - console.font[char_code].y * CONSOLE_SCALE);
  glUniform4f(console.color_loc,
              console.text_color.r / 255.0f,
              console.text_color.g / 255.0f,
              console.text_color.b / 255.0f,
              alpha);
  
  // Draw the character quad
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Draw text string using GL3
static void draw_text_gl3(const char* text, int x, int y, float alpha) {
  int cursor_x = x;
  
  // Set up GL state
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  
  // Use the shader program
  glUseProgram(console.program);
  
  // Set projection matrix uniform
  glUniformMatrix4fv(console.projection_loc, 1, GL_FALSE, console.projection);
  
  // Set texture unit
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(console.tex_loc, 0);
  
  // Bind VAO
  glBindVertexArray(console.vao);
  
  // Draw each character
#ifdef HEXEN
  // Hexen font is already uppercase
  for (int i = 0; text[i] != '\0'; i++) {
    char c = text[i];
#else
    // Doom font is uppercase only, convert lowercase to uppercase
    for (int i = 0; text[i] != '\0'; i++) {
      char c = toupper(text[i]);
#endif
      draw_char_gl3(c, cursor_x, y, alpha);
      
      // Advance cursor
      if (c >= 32 && c > 0) {
        // Use character width if available, otherwise use default
        int char_width = console.font[c].width;
        cursor_x += (char_width > 0 ? char_width : CONSOLE_FONT_WIDTH) * CONSOLE_SCALE;
      } else {
        cursor_x += CONSOLE_FONT_WIDTH * CONSOLE_SCALE;  // Default width for unsupported chars
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
    
    // Delete shader program and buffers
    glDeleteProgram(console.program);
    glDeleteVertexArrays(1, &console.vao);
    glDeleteBuffers(1, &console.vbo);
  }
  
  // Toggle console visibility
  void toggle_console(void) {
    console.show_console = !console.show_console;
  }
  
  // Set console text color
  void set_console_color(uint8_t r, uint8_t g, uint8_t b) {
    console.text_color.r = r;
    console.text_color.g = g;
    console.text_color.b = b;
  }
  
  // Add these to your console.c file at the top with other static variables
  static struct {
    Uint32 ticks[64];
    Uint32 last_fps_update;   // Last time the FPS was calculated
    Uint32 frame_count;       // Number of frames since last update
    float current_fps;        // Current FPS value to display
    char fps_text[32];        // Buffer for the FPS text
    Uint32 counter;
  } fps_state = {0};
  
  // Draw FPS counter
  void draw_fps(int x, int y) {
    Uint32 ticks = SDL_GetTicks();
    fps_state.ticks[fps_state.counter++&63] = ticks - fps_state.last_fps_update;
    fps_state.last_fps_update = ticks;
    
    Uint32 totals = 0;
    for (int i = 0; i < 64; i++) {
      totals += fps_state.ticks[i];
    }
    
    fps_state.current_fps = 64 * 1000.f / totals;
    
    int width, height;
    SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &width, &height);
    
    // Format the FPS text
    snprintf(fps_state.fps_text, sizeof(fps_state.fps_text), "FPS: %.1f", fps_state.current_fps);
    
    extern int sectors_drawn;
    char sec[64]={0};
    snprintf(sec, sizeof(sec), "SECTORS: %d", sectors_drawn);
    
    x = width - 120*CONSOLE_SCALE;
    
    // Draw the FPS text
    draw_text_gl3(fps_state.fps_text, x, y, 1.0f);
    draw_text_gl3(sec, x, y+20, 1.0f);
  }
