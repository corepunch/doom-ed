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
  -0.5f,   -0.5f,    0.0f, 0.0f, // bottom left
  -0.5f,    0.5f,    0.0f, 1.0f,  // top left
  0.5f,    0.5f,    1.0f, 1.0f, // top right
  0.5f,   -0.5f,    1.0f, 0.0f, // bottom right
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
GLuint load_sprite_texture(FILE* wad_file, filelump_t* sprite_lump, int* width, int* height, int* offsetx, int* offsety, palette_entry_t const*);
GLuint generate_crosshair_texture(int size);

int find_sprite_lump(filelump_t* directory, int num_lumps, const char* name) {
  // Locate sprite lump markers
  int s_start = find_lump(directory, num_lumps, "S_START");
  int s_end = find_lump(directory, num_lumps, "S_END");
  if (s_start >= 0 && s_end >= 0 && s_start < s_end) {
    // Iterate through all sprites
    for (int i = s_start + 1; i < s_end; i++) {
      if (strncmp(directory[i].name, name, sizeof(lumpname_t)) == 0) {
        return i;
      }
    }
  }
  return find_lump(directory, num_lumps, name);
}

int load_sprite(FILE *wad_file, filelump_t* sprite_lump, palette_entry_t const *palette) {
  int width, height, offsetx, offsety;
  GLuint texture = load_sprite_texture(wad_file, sprite_lump, &width, &height, &offsetx, &offsety, palette);
  if (texture) {
    sprite_t* sprite = &g_sprite_system.sprites[g_sprite_system.num_sprites];
    strncpy(sprite->name, sprite_lump->name, 16);
    sprite->texture = texture;
    sprite->width = width;
    sprite->height = height;
    sprite->offsetx = offsetx;
    sprite->offsety = offsety;
    printf("Loaded sprite: %s (%dx%d)\n", sprite_lump->name, width, height);
    return g_sprite_system.num_sprites++;
  } else {
    return -1;
  }
}

// Initialize the sprite system
bool init_sprites(map_data_t *map, FILE* wad_file, filelump_t* directory, int num_lumps) {
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
  glm_ortho(0, width, height, 0, -1, 1, sys->projection);
  
  // Find and preload weapon sprites (starting with SHT)
  sys->num_sprites = 0;

  
  int s_start = find_lump(directory, num_lumps, "S_START");
  int s_end = find_lump(directory, num_lumps, "S_END");
  if (s_start >= 0 && s_end >= 0 && s_start < s_end) {
    // Iterate through all sprites
    for (int i = s_start + 1; i < s_end; i++) {
      load_sprite(wad_file, &directory[i], map->palette);
    }
  }

//  // If no sprite markers, try loading directly by name
//  int shotgun_sprite = find_sprite_lump(directory, num_lumps, "SHTGA0");
//  if (shotgun_sprite >= 0) {
//    load_sprite(wad_file, &directory[shotgun_sprite], map->palette);
//  }
//  
//  // Try loading crosshair directly
//  int crosshair_sprite = find_sprite_lump(directory, num_lumps, "CROSA0");
//  if (crosshair_sprite >= 0) {
//    load_sprite(wad_file, &directory[crosshair_sprite], map->palette);
//  }
//  
//  int stbar_lump = find_sprite_lump(directory, num_lumps, "STBAR");
//  if (stbar_lump >= 0) {
//    load_sprite(wad_file, &directory[stbar_lump], map->palette);
//  }
  
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

// Load a sprite texture from WAD
GLuint load_sprite_texture(FILE* wad_file, filelump_t* sprite_lump, int* width, int* height, int* offsetx, int* offsety, palette_entry_t const *palette) {
  // Seek to sprite lump
  fseek(wad_file, sprite_lump->filepos, SEEK_SET);
  
  // Read header
  int16_t sprite_width, sprite_height;
  int16_t left_offset, top_offset;
  
  fread(&sprite_width, sizeof(int16_t), 1, wad_file);
  fread(&sprite_height, sizeof(int16_t), 1, wad_file);
  fread(&left_offset, sizeof(int16_t), 1, wad_file);
  fread(&top_offset, sizeof(int16_t), 1, wad_file);
  
  *width = sprite_width;
  *height = sprite_height;
  *offsetx = left_offset;
  *offsety = top_offset;
  
  // Allocate memory for column offsets
  int32_t* columnofs = malloc(sprite_width * sizeof(int32_t));
  if (!columnofs) return 0;
  
  // Read column offsets
  fread(columnofs, sizeof(int32_t), sprite_width, wad_file);
  
  // Allocate memory for texture data (RGBA)
  uint8_t* tex_data = calloc(sprite_width * sprite_height * 4, 1);
  if (!tex_data) {
    free(columnofs);
    return 0;
  }
    
  // Process each column
  for (int x = 0; x < sprite_width; x++) {
    // Seek to column data
    fseek(wad_file, sprite_lump->filepos + columnofs[x], SEEK_SET);
    
    // Process each post
    while (1) {
      uint8_t top_delta;
      fread(&top_delta, 1, 1, wad_file);
      
      if (top_delta == 0xFF) break; // End of column
      
      uint8_t length;
      fread(&length, 1, 1, wad_file);
      
      // Skip dummy byte
      fseek(wad_file, 1, SEEK_CUR);
      
      // Read pixel data
      for (int y = 0; y < length; y++) {
        uint8_t color_index;
        fread(&color_index, 1, 1, wad_file);
        
        int pos = ((top_delta + y) * sprite_width + x) * 4;
        tex_data[pos] = palette[color_index].r;
        tex_data[pos + 1] = palette[color_index].g;
        tex_data[pos + 2] = palette[color_index].b;
        tex_data[pos + 3] = (color_index == 247) ? 0 : 255; // 247 is transparent in DOOM sprites
      }
      
      // Skip dummy byte
      fseek(wad_file, 1, SEEK_CUR);
    }
  }
  
  // Create OpenGL texture
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sprite_width, sprite_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
  
  free(columnofs);
  free(tex_data);
  
  return texture;
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
  // Create a simple crosshair texture
  int crosshair_size = size; // Size of the crosshair texture
  uint8_t* crosshair_data = calloc(crosshair_size * crosshair_size * 4, 1);
  
  if (!crosshair_data) return 0;
  
  // Draw a simple plus-shaped crosshair
  int thickness = 3;//crosshair_size / 8;
  if (thickness < 1) thickness = 1;
  
  // Set color (white with full opacity)
  uint8_t r = 255, g = 255, b = 255, a = 255;
  
  // Draw horizontal line
  for (int y = crosshair_size/2 - thickness/2; y < crosshair_size/2 + thickness/2; y++) {
    for (int x = 0; x < crosshair_size; x++) {
      int idx = (y * crosshair_size + x) * 4;
      crosshair_data[idx] = r;     // R
      crosshair_data[idx + 1] = g; // G
      crosshair_data[idx + 2] = b; // B
      crosshair_data[idx + 3] = a; // A
    }
  }
  
  // Draw vertical line
  for (int x = crosshair_size/2 - thickness/2; x < crosshair_size/2 + thickness/2; x++) {
    for (int y = 0; y < crosshair_size; y++) {
      int idx = (y * crosshair_size + x) * 4;
      crosshair_data[idx] = r;     // R
      crosshair_data[idx + 1] = g; // G
      crosshair_data[idx + 2] = b; // B
      crosshair_data[idx + 3] = a; // A
    }
  }
  
  // Create empty space in the center
  int center_size = thickness * 2;
  for (int y = crosshair_size/2 - center_size/2; y < crosshair_size/2 + center_size/2; y++) {
    for (int x = crosshair_size/2 - center_size/2; x < crosshair_size/2 + center_size/2; x++) {
      int idx = (y * crosshair_size + x) * 4;
      crosshair_data[idx + 3] = 0; // Transparent
    }
  }
  
  // Create OpenGL texture
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, crosshair_size, crosshair_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, crosshair_data);
  
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

// Draw the shotgun at the bottom center of the screen
void draw_weapon(void) {
  int window_width, window_height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &window_width, &window_height);
  
  // Find appropriate shotgun sprite frame (usually "SHTGA0")
  const char* shotgun_sprite = "SHTGA0";
  sprite_t* sprite = find_sprite(shotgun_sprite);
  
  if (!sprite) {
    // Try alternative names if not found
    const char* alternatives[] = {"SHTFA0", "SHTGB0", "SHTFC0"};
    for (int i = 0; i < 3; i++) {
      sprite = find_sprite(alternatives[i]);
      if (sprite) {
        shotgun_sprite = alternatives[i];
        break;
      }
    }
  }

  // Scale up the weapon a bit
  float scale = 2.0f;

  sprite_t* STBAR = find_sprite("STBAR");
  if (STBAR) {
    draw_sprite("STBAR", window_width / 2.0f, window_height-STBAR->height/2*scale, 2, 1.0f);
  }

//  if (sprite) {
//    
//    // Position at bottom center of screen, slightly raised
//    float x = window_width / 2.0f;
//    float y = window_height - (sprite->height / 2 + STBAR->height) * scale;
//    
//    // Draw with full opacity
//    draw_sprite(shotgun_sprite, x, y, scale, 1.0f);
//  }
  
}

// Draw a crosshair in the center of the screen
void draw_crosshair(void) {
  sprite_system_t* sys = &g_sprite_system;
  int window_width, window_height;
  SDL_GetWindowSize(SDL_GL_GetCurrentWindow(), &window_width, &window_height);
  
  // First try to find a pre-existing crosshair sprite
  sprite_t* sprite = find_sprite("CROSA0");
  
  if (sprite) {
    // Use the existing crosshair sprite
    float x = window_width / 2.0f;
    float y = window_height / 2.0f;
    float scale = 1.0f;
    float alpha = 0.8f; // Slightly transparent for better visibility
    
    draw_sprite("CROSA0", x, y, scale, alpha);
  } else {
    // No crosshair sprite found, generate one on demand
    if (sys->crosshair_texture == 0) {
      // Generate a crosshair texture of 16x16 pixels
      sys->crosshair_texture = generate_crosshair_texture(16);
      
      if (sys->crosshair_texture == 0) {
        printf("Failed to generate crosshair texture\n");
        return;
      }
      
      // Add it to our sprite cache
      if (sys->num_sprites < MAX_SPRITES) {
        sprite_t* new_sprite = &sys->sprites[sys->num_sprites];
        strncpy(new_sprite->name, "CROSSH", 16);
        new_sprite->texture = sys->crosshair_texture;
        new_sprite->width = 16;
        new_sprite->height = 16;
        new_sprite->offsetx = 0;
        new_sprite->offsety = 0;
        sys->num_sprites++;
        
        printf("Generated crosshair sprite (16x16)\n");
      }
    }
    
    // Draw the generated crosshair
    sprite = find_sprite("CROSSH");
    if (sprite) {
      float x = window_width / 2.0f;
      float y = window_height / 2.0f;
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
void draw_rect(int tex, float x, float y, float w, float h) {
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
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  
  // Reset state
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}
