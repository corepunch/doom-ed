#include <SDL2/SDL.h>
#include "gl_compat.h"
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <string.h>
#include <math.h>
#include "map.h"
#include "sprites.h"
#include "editor.h"
#include "game/thing_info.h"

// Thing shader sources - we'll use view-aligned quads
const char* thing_vs_src = "#version 150 core\n"
"in vec2 position; in vec2 texcoord;\n"
"out vec2 tex;\n"
"uniform mat4 mvp;\n"
"uniform vec2 scale;\n"
"void main() {\n"
"  tex = texcoord;\n"
"  gl_Position = mvp * vec4(position * scale, 0.0, 1.0);\n"
"}";

const char* thing_fs_src = "#version 150 core\n"
"in vec2 tex;\n"
"out vec4 outColor;\n"
"uniform sampler2D tex0;\n"
"uniform float light;\n"
"void main() {\n"
"  outColor = texture(tex0, tex);\n"
"  outColor.rgb *= light;\n"
"  if(outColor.a < 0.1) discard;\n"
"}";

// Vertex data for a quad
float thing_verts[] = {
  // pos.x  pos.y    tex.x tex.y
  -0.5f,   -0.5f,    0.0f, 1.0f, // bottom left
  -0.5f,   0.5f,    0.0f, 0.0f, // top left
  0.5f,    0.5f,    1.0f, 0.0f, // top right
  0.5f,    -0.5f,    1.0f, 1.0f, // bottom right
};

// Thing rendering system
typedef struct {
  GLuint program;
  GLuint vao;
  GLuint vbo;
} thing_renderer_t;

static thing_renderer_t g_thing_renderer = {0};

// Forward declarations
GLuint compile_thing_shader(GLenum type, const char* src);
bool point_in_frustum(vec3 point, vec4 const planes[6]);

// Initialize the thing rendering system
bool init_things(void) {
  thing_renderer_t* renderer = &g_thing_renderer;
  
  // Create shader program
  GLuint vertex_shader = compile_thing_shader(GL_VERTEX_SHADER, thing_vs_src);
  GLuint fragment_shader = compile_thing_shader(GL_FRAGMENT_SHADER, thing_fs_src);
  
  renderer->program = glCreateProgram();
  glAttachShader(renderer->program, vertex_shader);
  glAttachShader(renderer->program, fragment_shader);
  glBindAttribLocation(renderer->program, 0, "position");
  glBindAttribLocation(renderer->program, 1, "texcoord");
  glLinkProgram(renderer->program);
  
  // Check for linking errors
  GLint success;
  glGetProgramiv(renderer->program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(renderer->program, 512, NULL, infoLog);
    printf("Thing shader program linking failed: %s\n", infoLog);
    return false;
  }
  
  // Create VAO and VBO
  glGenVertexArrays(1, &renderer->vao);
  glBindVertexArray(renderer->vao);
  
  glGenBuffers(1, &renderer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(thing_verts), thing_verts, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  // Initialize game-specific thing data
  if (!game_init_thing_info()) {
    printf("Failed to initialize game thing info\n");
    return false;
  }
  
  return true;
}

// Compile thing shader
GLuint compile_thing_shader(GLenum type, const char* src) {
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
    printf("Thing shader compilation error: %s\n", log);
    free(log);
  }
  
  return shader;
}

// Helper function to get sprite name based on thing type and angle
sprite_t *get_thing_sprite_name(uint16_t thing_type, uint16_t angle) {
  return game_get_thing_sprite(thing_type, angle);
}

// Returns a value from 1 to 8 for sprite angle selection
int GetSpriteRotationIndex(int thingAngleDeg, int playerAngleDeg) {
  // Normalize both to 0–359
  while (playerAngleDeg < 0) {
    playerAngleDeg += 360;
  }
  while (thingAngleDeg < 0) {
    thingAngleDeg += 360;
  }
  int relAngle = (playerAngleDeg - thingAngleDeg + 360) % 360;
  
  // Divide the circle into 8 equal 45-degree sectors
  // Add 22 to center each sector around its angle
  int spriteIndex = ((relAngle + 22) % 360) / 45;
  
  return spriteIndex;
}

// Draw things in the map
void draw_things(map_data_t const *map, viewdef_t const *viewdef, bool rotate) {
  thing_renderer_t* renderer = &g_thing_renderer;
  
  glDisable(GL_CULL_FACE);
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Use thing shader program
  glUseProgram(renderer->program);
  
  // Bind VAO
  glBindVertexArray(renderer->vao);
  
  // Set uniforms that don't change per-thing
  glUniformMatrix4fv(glGetUniformLocation(renderer->program, "mvp"), 1, GL_FALSE, viewdef->mvp[0]);

  // Loop through all things in the map
  for (int i = 0; i < map->num_things; i++) {
    mapthing_t const *thing = &map->things[i];
    
    // Skip player start positions
    if (game_is_player_start(thing->type))
      continue;
    
    // Find the sector this thing is in to get light level
    if (thing->height == -1) {
      continue; // Skip if not in any sector
    }
    
    float z_pos = 0;
    float light = 1;

    if (thing->height < map->num_sectors) {
      mapsector_t const *sector = &map->sectors[thing->height];
      
      // Calculate distance to player (for potential culling)
      //    float dist_to_player = dist_sq(player->x, player->y, thing->x, thing->y);
      
      // Simple culling - skip if too far away
      //    if (dist_to_player > 100000.0f) { // Adjust threshold as needed
      //      continue;
      //    }
      
      // Calculate thing height (based on floor height)
      z_pos = sector->floorheight;
      light = sector->lightlevel / 255.0f;
    }
    
    // Get appropriate sprite name based on thing type and angle
    int angle = GetSpriteRotationIndex(thing->angle, viewdef->player.angle);
    sprite_t* sprite = get_thing_sprite_name(thing->type, rotate?angle:0);
    // Set up matrices for this thing
    mat4 model, mv;
    glm_mat4_identity(model);
    
    // Translate to thing position
    glm_translate(model, (vec3){thing->x, thing->y, z_pos+sprite->offsety-sprite->height/2});
    
    // Rotate to face the defined angle
//    float angle_rad = glm_rad(thing->angle);
//    glm_rotate_z(model, angle_rad, model);
    
    float dx = viewdef->viewpos[0] - thing->x;
    float dy = viewdef->viewpos[1] - thing->y;

    if (rotate) {
      glm_rotate_z(model, atan2f(dy, dx)-M_PI_2, model);
      glm_rotate_x(model, M_PI_2, model);
    }
    
    if (!point_in_frustum((vec3){thing->x, thing->y, z_pos}, viewdef->frustum))
      continue;

    glm_mat4_mul(viewdef->mvp, model, mv);

    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "mvp"), 1, GL_FALSE, (const float*)mv);
    
    // Set scale uniform
    glUniform2f(glGetUniformLocation(renderer->program, "scale"), sprite->width, sprite->height);
    
    // Set light level
    glUniform1f(glGetUniformLocation(renderer->program, "light"), rotate?light*1.5:1.25);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sprite->texture);
    glUniform1i(glGetUniformLocation(renderer->program, "tex0"), 0);
    
    // Draw the thing
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  }
    
  // Reset state
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}

// Cleanup thing rendering resources
void cleanup_things(void) {
  thing_renderer_t* renderer = &g_thing_renderer;
  
  glDeleteProgram(renderer->program);
  glDeleteVertexArrays(1, &renderer->vao);
  glDeleteBuffers(1, &renderer->vbo);
}

void assign_thing_sector(map_data_t const *map, mapthing_t *thing) {
  mapsector_t const *sector = find_player_sector(map, thing->x, thing->y);
  if(sector) {
    thing->height = sector - map->sectors;
  } else {
    thing->height = -1;
  }
}
