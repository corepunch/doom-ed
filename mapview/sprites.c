#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <string.h>
#include <ctype.h>
#include "map.h"
#include "sprites.h"

#define MAX_SPRITES 10000

// Sprite shader sources
const char* sprite_vs_src = "#version 150 core\n"
"in vec2 position; in vec2 texcoord;\n"
"out vec2 tex;\n"
"uniform mat4 projection;\n"
"uniform vec2 offset;\n"
"uniform vec2 scale;\n"
"void main() {\n"
"  tex = texcoord;\n"
"  gl_Position = projection * vec4(position * scale + offset, 0.0, 1.0);\n"
"}";

const char* sprite_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform float alpha;\n"
"void main() {\n"
"  outColor = texture(tex0, tex);\n"
"  outColor.a *= alpha;\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

// Sprite vertices (quad)
float sprite_verts[] = {
  // pos.x  pos.y    tex.x tex.y
  0,   0,    0.0f, 0.0f, // bottom left
  0,   1,    0.0f, 1.0f,  // top left
  1,   1,    1.0f, 1.0f, // top right
  1,   0,    1.0f, 0.0f, // bottom right
};

// Sprite system state
typedef struct {
  GLuint program;        // Shader program
  GLuint vao;            // Vertex array object
  GLuint vbo;            // Vertex buffer object
  sprite_t sprites[MAX_SPRITES];
  int num_sprites;
  mat4 projection;       // Orthographic projection matrix
  GLuint crosshair_texture; // Custom crosshair texture (if needed)
} sprite_system_t;

sprite_system_t g_sprite_system = {0};

// Forward declarations
GLuint compile_shader(GLenum type, const char* src);
GLuint load_sprite_texture(void *data, int* width, int* height, int* offsetx, int* offsety);
GLuint generate_crosshair_texture(int size);

float black_bars = 0.f;

int load_sprite(const char *name) {
  int width, height, offsetx, offsety;
  GLuint texture = load_sprite_texture(cache_lump(name), &width, &height, &offsetx, &offsety);
  if (texture) {
    sprite_t* sprite = &g_sprite_system.sprites[g_sprite_system.num_sprites];
    strncpy(sprite->name, name, 16);
    sprite->texture = texture;
    sprite->width = width;
    sprite->height = height;
    sprite->offsetx = offsetx;
    sprite->offsety = offsety;
//    printf("Loaded sprite: %s (%dx%d)\n", sprite_lump->name, width, height);
    return g_sprite_system.num_sprites++;
  } else {
    return -1;
  }
}

float *get_sprite_matrix(void) {
  return g_sprite_system.projection[0];
}

// Initialize the sprite system
bool init_sprites(void) {
  sprite_system_t* sys = &g_sprite_system;
  
  // Create shader program
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, sprite_vs_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, sprite_fs_src);
  
  sys->program = glCreateProgram();
  glAttachShader(sys->program, vertex_shader);
  glAttachShader(sys->program, fragment_shader);
  glBindAttribLocation(sys->program, 0, "position");
  glBindAttribLocation(sys->program, 1, "texcoord");
  glLinkProgram(sys->program);
  
  // Create VAO, VBO, EBO
  glGenVertexArrays(1, &sys->vao);
  glBindVertexArray(sys->vao);
  
  glGenBuffers(1, &sys->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, sys->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_verts), sprite_verts, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  // Create orthographic projection matrix for screen-space rendering
  int width, height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &width, &height);
  float scale = (float)height / DOOM_HEIGHT;
  float render_width = DOOM_WIDTH * scale;
  float offset_x = (width - render_width) / (2.0f * scale);
  black_bars = offset_x;
  glm_ortho(-offset_x, DOOM_WIDTH+offset_x, DOOM_HEIGHT, 0, -1, 1, sys->projection);
  
  // Find and preload weapon sprites (starting with SHT)
  sys->num_sprites = 0;
  
  int s_start = find_lump_num("S_START");
  int s_end = find_lump_num("S_END");
  if (s_start >= 0 && s_end >= 0 && s_start < s_end) {
    // Iterate through all sprites
    for (int i = s_start + 1; i < s_end; i++) {
      load_sprite(get_lump_name(i));
    }
  }

//  // If no sprite markers, try loading directly by name
//  int shotgun_sprite = find_lump("SHTGA0");
//  if (shotgun_sprite >= 0) {
//    load_sprite(wad_file, &directory[shotgun_sprite]);
//  }
//  
//  // Try loading crosshair directly
//  int crosshair_sprite = find_lump("CROSA0");
//  if (crosshair_sprite >= 0) {
//    load_sprite(wad_file, &directory[crosshair_sprite]);
//  }
//  
//  int stbar_lump = find_lump("STBAR");
//  if (stbar_lump >= 0) {
//    load_sprite(wad_file, &directory[stbar_lump]);
//  }
  
  
#ifdef HEXEN
  load_sprite("H2BAR");
  load_sprite("H2TOP");
  load_sprite("INVBAR");
  load_sprite("STATBAR");
  load_sprite("KEYBAR");
#else
  load_sprite("STBAR");
#endif
  
  // Initialize the crosshair texture to 0 (will be generated on demand if needed)
  sys->crosshair_texture = 0;
  
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  return sys->num_sprites > 0;
}

// Compile a shader
GLuint compile_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, 0);
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
// Sprite header structure (same as patch_t)
typedef struct {
  int16_t width;      // width of the sprite
  int16_t height;     // height of the sprite
  int16_t leftoffset; // left offset of sprite
  int16_t topoffset;  // top offset of sprite
  int32_t columnofs[]; // column offsets (flexible array member)
} spriteheader_t;

// Load a sprite texture from memory
GLuint load_sprite_texture(void *data, int* width, int* height, int* offsetx, int* offsety) {
  if (!data) return 0;
  
  // Cast to sprite structure for header access
  spriteheader_t* sprite_header = data;
  
  // Get dimensions and offsets
  *width = sprite_header->width;
  *height = sprite_header->height;
  *offsetx = sprite_header->leftoffset;
  *offsety = sprite_header->topoffset;
  
  // Allocate memory for texture data (RGBA)
  uint8_t* tex_data = calloc(sprite_header->width * sprite_header->height * 4, 1);
  if (!tex_data) {
    return 0;
  }
  
  // Column offsets are directly accessible in the sprite header
  int32_t* columnofs = sprite_header->columnofs;
  
  // Base address for byte calculations
  uint8_t* sprite_bytes = (uint8_t*)data;
  
  // Process each column
  for (int x = 0; x < sprite_header->width; x++) {
    // Calculate position of this column's data
    uint8_t* column_data = sprite_bytes + columnofs[x];
    
    // Process each post
    while (1) {
      uint8_t top_delta = *column_data++;
      
      if (top_delta == 0xFF) break; // End of column
      
      uint8_t length = *column_data++;
      
      // Skip dummy byte
      column_data++;
      
      // Read pixel data
      for (int y = 0; y < length; y++) {
        uint8_t color_index = *column_data++;
        
        int pos = ((top_delta + y) * sprite_header->width + x) * 4;
        tex_data[pos] = palette[color_index].r;
        tex_data[pos + 1] = palette[color_index].g;
        tex_data[pos + 2] = palette[color_index].b;
        tex_data[pos + 3] = (color_index == 247) ? 0 : 255; // 247 is transparent in DOOM sprites
      }
      
      // Skip dummy byte
      column_data++;
    }
  }
  
  // Create OpenGL texture
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  
  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  // Upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sprite_header->width, sprite_header->height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
  
  // Free temporary data
  free(tex_data);
  
  return texture_id;
}

// Helper function to find sprite in cache
sprite_t* find_sprite(const char* name) {
  sprite_system_t* sys = &g_sprite_system;
  
  for (int i = 0; i < sys->num_sprites; i++) {
    if (strncmp(sys->sprites[i].name, name, 4) == 0) {
      return &sys->sprites[i];
    }
  }
  
  return NULL;
}

// Find lump from file (helper function)
int find_lump_from_file(FILE* wad_file, const char* name) {
  // Get file size
  fseek(wad_file, 0, SEEK_END);
//  long file_size = ftell(wad_file);
  fseek(wad_file, 0, SEEK_SET);
  
  // Read WAD header
  wadheader_t header;
  fread(&header, sizeof(wadheader_t), 1, wad_file);
  
  // Seek to directory
  fseek(wad_file, header.infotableofs, SEEK_SET);
  
  // Return the file position of the matching lump
  for (uint32_t i = 0; i < header.numlumps; i++) {
    long lump_pos = ftell(wad_file);
    
    filelump_t lump;
    fread(&lump, sizeof(filelump_t), 1, wad_file);
    
    if (strncmp(lump.name, name, 8) == 0) {
      return (int)lump_pos;
    }
  }
  
  return -1;
}

// Generate a simple crosshair texture
GLuint generate_crosshair_texture(int size) {
  // Clamp size to minimum of 8
  if (size < 8) size = 8;
  
  uint32_t* crosshair_data = calloc(size * size * 4, 1);
  if (!crosshair_data) return 0;

  for (int i = 0; i < size-1; i++) {
    crosshair_data[i+size*(size/2-1)] = -1;
    crosshair_data[i*size+(size/2-1)] = -1;
  }
  
  crosshair_data[(1+size)*(size/2-1)] = 0;
  crosshair_data[(1+size)*(size/2-1)-1] = 0;
  crosshair_data[(1+size)*(size/2-1)+1] = 0;
  crosshair_data[(1+size)*(size/2-1)-size] = 0;
  crosshair_data[(1+size)*(size/2-1)+size] = 0;

  // Create OpenGL texture
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, crosshair_data);
  
  free(crosshair_data);
  return texture;
}

// Draw a sprite at the specified screen position
void draw_sprite(const char* name, float x, float y, float scale, float alpha) {
  sprite_system_t* sys = &g_sprite_system;
  sprite_t* sprite = find_sprite(name);
  
  if (!sprite) {
    printf("Sprite not found: %s\n", name);
    return;
  }
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Disable depth testing for UI elements
  glDisable(GL_DEPTH_TEST);
  
  // Use sprite shader program
  glUseProgram(sys->program);
  
  // Set uniforms
  glUniformMatrix4fv(glGetUniformLocation(sys->program, "projection"), 1, GL_FALSE, (const float*)sys->projection);
  glUniform2f(glGetUniformLocation(sys->program, "offset"), x-sprite->offsetx*scale, y-sprite->offsety*scale);
  glUniform2f(glGetUniformLocation(sys->program, "scale"), sprite->width * scale, sprite->height * scale);
  glUniform1f(glGetUniformLocation(sys->program, "alpha"), alpha);
  
  // Bind sprite texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sprite->texture);
  glUniform1i(glGetUniformLocation(sys->program, "tex0"), 0);
  
  // Bind VAO and draw
  glBindVertexArray(sys->vao);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

void get_weapon_wobble_offset(int* offset_x, int* offset_y, float speed) {
  unsigned ticks = SDL_GetTicks();
  float time = ticks / 1000.0f;
  
  float freq = 6.0f;             // How fast the bob oscillates
  float amp_x = 4.0f;            // Side-to-side movement
  float amp_y = 2.0f;            // Vertical bobbing
  
  float phase = time * freq;
  
  *offset_x = (int)(sin(phase) * amp_x * speed);
  *offset_y = (int)(fabs(sin(phase)) * amp_y * speed); // U-shaped vertical bob
}

// Draw the shotgun at the bottom center of the screen
void draw_weapon(void) {
  int window_width, window_height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &window_width, &window_height);
  
  // Find appropriate shotgun sprite frame (usually "SHTGA0")
#ifdef HEXEN
//  int tick = SDL_GetTicks();
//  char shotgun_sprite[64]={0};
//  sprintf(shotgun_sprite, "CFLM%c0", 'A'+tick%8);
  const char* shotgun_sprite = "MSTFA0";
#else
  const char* shotgun_sprite = "SHTGA0";
#endif
  sprite_t* sprite = find_sprite(shotgun_sprite);
  
//  if (!sprite) {
//    // Try alternative names if not found
//    const char* alternatives[] = {"SHTFA0", "SHTGB0", "SHTFC0"};
//    for (int i = 0; i < 3; i++) {
//      sprite = find_sprite(alternatives[i]);
//      if (sprite) {
//        shotgun_sprite = alternatives[i];
//        break;
//      }
//    }
//  }

  if (sprite) {
    // Draw with full opacity
    
    int x, y;
    get_weapon_wobble_offset(&x, &y, MAX(fabs(game.player.vel_x), fabs(game.player.vel_y))/75);
    
//#ifndef HEXEN
//    y += 20;
//#else
    y += 50;
//#endif
    
    draw_sprite(shotgun_sprite, x, y, 1, 1.0f);

  }

#ifdef HEXEN
  sprite_t* STBAR = find_sprite("H2BAR");
  if (STBAR) {
    draw_sprite("H2BAR", 0, 134, 1, 1);
//    draw_sprite("INVBAR", 38, 162, 1, 1);
  }
#else
  sprite_t* STBAR = find_sprite("STBAR");
  if (STBAR) {
    draw_sprite("STBAR", 0, DOOM_HEIGHT-STBAR->height, 1, 1.0f);
  }
#endif
}

#define CROSSHAIR_SIZE 10

// Draw a crosshair in the center of the screen
void draw_crosshair(void) {
  sprite_system_t* sys = &g_sprite_system;
  
  // First try to find a pre-existing crosshair sprite
  sprite_t* sprite = find_sprite("CROSA0");
  
  if (sprite) {
    // Use the existing crosshair sprite
    float x = DOOM_WIDTH/2;
    float y = DOOM_HEIGHT/2;
    float scale = 1.0f;
    float alpha = 0.6f; // Slightly transparent for better visibility
    
    draw_sprite("CROSA0", x, y, scale, alpha);
  } else {
    // No crosshair sprite found, generate one on demand
    if (sys->crosshair_texture == 0) {
      // Generate a crosshair texture of 16x16 pixels
      sys->crosshair_texture = generate_crosshair_texture(CROSSHAIR_SIZE);
      
      if (sys->crosshair_texture == 0) {
        printf("Failed to generate crosshair texture\n");
        return;
      }
      
      // Add it to our sprite cache
      if (sys->num_sprites < MAX_SPRITES) {
        sprite_t* new_sprite = &sys->sprites[sys->num_sprites];
        strncpy(new_sprite->name, "CROSSH", 16);
        new_sprite->texture = sys->crosshair_texture;
        new_sprite->width = CROSSHAIR_SIZE;
        new_sprite->height = CROSSHAIR_SIZE;
        new_sprite->offsetx = CROSSHAIR_SIZE/2;
        new_sprite->offsety = CROSSHAIR_SIZE/2;
        sys->num_sprites++;
        
        printf("Generated crosshair sprite (16x16)\n");
      }
    }
    
    // Draw the generated crosshair
    sprite = find_sprite("CROSSH");
    if (sprite) {
      float x = DOOM_WIDTH/2;
      float y = DOOM_HEIGHT/2;
      float scale = 2.0f; // Scale up the small texture
      float alpha = 1.0f;
      
      // Enable blending for transparency
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      
      // Disable depth testing for UI elements
      glDisable(GL_DEPTH_TEST);
      
      // Use sprite shader program
      glUseProgram(sys->program);
      
      // Set uniforms
      glUniformMatrix4fv(glGetUniformLocation(sys->program, "projection"), 1, GL_FALSE, (const float*)sys->projection);
      glUniform2f(glGetUniformLocation(sys->program, "offset"), x, y);
      glUniform2f(glGetUniformLocation(sys->program, "scale"), sprite->width * scale, sprite->height * scale);
      glUniform1f(glGetUniformLocation(sys->program, "alpha"), alpha);
      
      // Bind sprite texture
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, sprite->texture);
      glUniform1i(glGetUniformLocation(sys->program, "tex0"), 0);
      
      // Bind VAO and draw
      glBindVertexArray(sys->vao);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      
      // Reset state
      glEnable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
    }
  }
}

// Clean up sprite system resources
void cleanup_sprites(void) {
  sprite_system_t* sys = &g_sprite_system;
  
  // Delete textures
  for (int i = 0; i < sys->num_sprites; i++) {
    glDeleteTextures(1, &sys->sprites[i].texture);
  }
  
  // Delete shader program and buffers
  glDeleteProgram(sys->program);
  glDeleteVertexArrays(1, &sys->vao);
  glDeleteBuffers(1, &sys->vbo);
  
  // Reset sprite count
  sys->num_sprites = 0;
}


// Draw a sprite at the specified screen position
void draw_rect_ex(int tex, float x, float y, float w, float h, int type) {
  sprite_system_t* sys = &g_sprite_system;
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Disable depth testing for UI elements
  glDisable(GL_DEPTH_TEST);
  
  // Use sprite shader program
  glUseProgram(sys->program);
  
  // Set uniforms
  glUniformMatrix4fv(glGetUniformLocation(sys->program, "projection"), 1, GL_FALSE, (const float*)sys->projection);
  glUniform2f(glGetUniformLocation(sys->program, "offset"), x, y);
  glUniform2f(glGetUniformLocation(sys->program, "scale"), w, h);
  glUniform1f(glGetUniformLocation(sys->program, "alpha"), 1);
  
  // Bind sprite texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(sys->program, "tex0"), 0);
  
  // Bind VAO and draw
  glBindVertexArray(sys->vao);
  glDrawArrays(type?GL_LINE_LOOP:GL_TRIANGLE_FAN, 0, 4);
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

// Draw a sprite at the specified screen position
void draw_rect(int tex, float x, float y, float w, float h) {
  draw_rect_ex(tex, x, y, w, h, false);
}
