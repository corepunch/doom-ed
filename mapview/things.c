#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <string.h>
#include <math.h>
#include "map.h"
#include "sprites.h"

#ifdef HEXEN
#include "../hexen/info.h"
#else
#include "../hexen/info.h"
#endif

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

thing_renderer_t g_thing_renderer = {0};

// Forward declarations
GLuint compile_thing_shader(GLenum type, const char* src);
sprite_t *get_thing_sprite_name(int thing_type, int angle);
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
sprite_t *get_thing_sprite_name(int thing_type, int angle) {
  static char sprite_name[9] = {0}; // e.g., "TROO_A0"
  const char* prefix = "UNKN";
  state_t *s = NULL;
  
  for (int i = 0; i < NUMMOBJTYPES; i++) {
    if (mobjinfo[i].doomednum == thing_type) {
      s = &states[mobjinfo[i].spawnstate];
      if (s->cache) {
        return s->cache;
      }
      prefix = sprnames[s->sprite];
      break;
    }
  }
  
  if (s) {
    // Determine angle frame (A-H based on 45-degree increments)
    // Doom uses 8 angle sprites, with 0 being east, 45 northeast, etc.
    char angle_char;
    int angle_index = ((angle + 22) % 360) / 45;
    angle_char = 'A' + angle_index;
    
    // Format sprite name (typically PREFIX + ANGLE + FRAME, e.g., "TROOA1")
    snprintf(sprite_name, sizeof(sprite_name), "%s%c1", prefix, angle_char);
    
    extern sprite_t* find_sprite(const char* name);
    sprite_t* sprite = find_sprite(sprite_name);
    s->cache = sprite;
    if(sprite) return sprite;
    
    snprintf(sprite_name, sizeof(sprite_name), "%sA0", prefix);
    sprite = find_sprite(sprite_name);
    s->cache = sprite;
    if(sprite) return sprite;
  }
  
  static sprite_t emtpy = {
    .width = 8,
    .height = 8,
    .texture = 1,
  };
  
  if (s) s->cache = &emtpy;

  return &emtpy;
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
    
    // Skip player
#ifdef HEXEN
    if (thing->type == MT_MAPSPOT)
#else
    if (thing->type == MT_PLAYER)
#endif
      continue;
    
    // Find the sector this thing is in to get light level
    if (thing->flags == -1) {
      continue; // Skip if not in any sector
    }

    mapsector_t const *sector = &map->sectors[thing->flags];

    // Calculate distance to player (for potential culling)
//    float dist_to_player = dist_sq(player->x, player->y, thing->x, thing->y);
    
    // Simple culling - skip if too far away
//    if (dist_to_player > 100000.0f) { // Adjust threshold as needed
//      continue;
//    }
    
    // Get appropriate sprite name based on thing type and angle
    sprite_t* sprite = get_thing_sprite_name(thing->type, thing->angle);
    
    // Calculate thing height (based on floor height)
    float z_pos = sector->floorheight;
    
    // Set up matrices for this thing
    mat4 model, mv;
    glm_mat4_identity(model);
    
    // Translate to thing position
    glm_translate(model, (vec3){thing->x, thing->y, z_pos+sprite->height/2});
    
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
    glUniform2f(glGetUniformLocation(renderer->program, "scale"),
                sprite->width, sprite->height);
    
    // Set light level
    float light = sector->lightlevel / 255.0f;
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

sprite_t const *get_thing_sprite(int index, mobjinfo_t const *mobjinfo) {
  mobjinfo_t const *mobj = &mobjinfo[index];
  state_t const *state = &states[mobj->spawnstate];
  const char *name = sprnames[state->sprite];
  return find_sprite(name);
}

int get_mobj_size(int index, void *_things) {
//  mobjinfo_t const *mobj = &((mobjinfo_t const *)_things)[index];
  sprite_t const *sptr = get_thing_sprite(index, _things);
  if (sptr) {
    return MAKEDWORD(sptr->width, sptr->height);
  } else {
    return 0;
  }
}

struct texture_layout_s*
layout(int num_textures,
       int layout_width,
       int (*get_size)(int, void *),
       void *param);

int
get_layout_item(struct texture_layout_s* layout,
                int index,
                int *texutre_index);

int get_texture_at_point(struct texture_layout_s* layout, int x, int y);

// Cleanup thing rendering resources
void cleanup_things(void) {
  thing_renderer_t* renderer = &g_thing_renderer;
  
  glDeleteProgram(renderer->program);
  glDeleteVertexArrays(1, &renderer->vao);
  glDeleteBuffers(1, &renderer->vbo);
}

static int num_items = 0;
static mobjinfo_t ed_objs[NUMMOBJTYPES];

bool win_things(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
  extern state_t states[NUMSTATES];
  extern char *sprnames[NUMSPRITES];
  switch (msg) {
    case MSG_CREATE:
      num_items = 0;
      for (int i = 0; i < NUMMOBJTYPES; i++) {
        if (mobjinfo[i].doomednum == -1)
          continue;
        ed_objs[num_items++] = mobjinfo[i];
      }
      win->userdata = layout(num_items, win->frame.w, get_mobj_size, ed_objs);
      break;
    case MSG_PAINT:
      for (int i = 0; i < num_items; i++) {
        int mobjindex;
        int pos = get_layout_item(win->userdata, i, &mobjindex);
        if (mobjindex == -1) continue;
        sprite_t const *sptr = get_thing_sprite(mobjindex, ed_objs);
        if (sptr) {
          int x = sptr->offsetx + LOWORD(pos) + win->scroll[0];
          int y = sptr->offsety + HIWORD(pos) + win->scroll[1];
          draw_sprite(sptr->name, x, y, 1, 1);
        }
      }
      break;
    case MSG_RESIZE:
      free(win->userdata);
      win->userdata = layout(num_items, win->frame.w, get_mobj_size, ed_objs);
      return true;
    case MSG_LBUTTONUP: {
      int texture_idx =
      get_texture_at_point(win->userdata,
                           (LOWORD(wparam) - win->scroll[0]),
                           (HIWORD(wparam) - win->scroll[1]));
      if (texture_idx >= 0) {
        printf("%d %s\n", texture_idx, get_thing_sprite(texture_idx, ed_objs)->name);
//        memcpy(udata->cache->selected, udata->cache->textures[texture_idx].name, sizeof(texname_t));
      }
//      invalidate_window(win);
      return true;
    }
  }
  return false;
}
