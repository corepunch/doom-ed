#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"

#define init_wall_vertex(X, Y, Z, U, V) \
wall_created = true; \
map->walls.vertices[map->walls.num_vertices++] = \
(wall_vertex_t) {X,Y,Z,U*dist+u_offset,V*height+v_offset,n[0],n[1],n[2],color}

extern GLuint world_prog, ui_prog;
extern GLuint no_tex, white_tex;

float compute_normal_packed(float dx, float dy, int8_t out[3]) {
  // 2. Normal vector (perpendicular, e.g., CCW)
  float nx = -dy;
  float ny = dx;
  
  // 3. Normalize
  float length = sqrtf(nx * nx + ny * ny);
  if (length == 0.0f) {
    out[0] = out[1] = out[2] = 0; // degenerate case
    return 0;
  }
  
  nx /= length;
  ny /= length;
  
  // 4. Pack into signed 8-bit normalized (–127 to +127)
  out[0] = (int8_t)(nx * 127.0f);
  out[1] = (int8_t)(ny * 127.0f);
  out[2] = 0; // unused z channel (or could be a flag)
  
  // Optional: Clamp to [-127, 127] if paranoid
  return length;
}

void build_wall_vertex_buffer(map_data_t *map) {
  map->walls.sections = realloc(map->walls.sections, sizeof(mapsidedef2_t) * map->num_sidedefs);
  memset(map->walls.sections, 0, sizeof(mapsidedef2_t) * map->num_sidedefs);
  for (uint32_t i = 0; i < map->num_sidedefs; i++) {
    map->walls.sections[i].def = &map->sidedefs[i];
    map->walls.sections[i].sector = &map->sectors[map->sidedefs[i].sector];
  }
  
  map->walls.num_vertices = 0;
  // Create VAO for walls if not already created
  if (!map->walls.vao) {
    glGenVertexArrays(1, &map->walls.vao);
    glGenBuffers(1, &map->walls.vbo);
  }
  
  // Loop through all linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Skip if invalid front sidedef
    if (linedef->sidenum[0] >= map->num_sidedefs) {
      continue;
    }
    
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    mapsidedef2_t *front = &map->walls.sections[linedef->sidenum[0]];
    mapsidedef2_t *back = NULL;
    int8_t n[3];
    float dx = v2->x - v1->x;
    float dy = v2->y - v1->y;
    float dist = compute_normal_packed(-dx, -dy, n);
    bool two_sided = linedef->sidenum[1] != 0xFFFF;
    bool wall_created = false;
    int32_t color = two_sided ? 0x00ffff00 : 0;

    // Check if there's a back side to this linedef
    if (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs) {
      back = &map->walls.sections[linedef->sidenum[1]];
    }
    
    float u_offset = front->def->textureoffset;
    float v_offset = front->def->rowoffset;
    
    // Process front side
    // Add vertices for upper texture (if ceiling heights differ)
    if (back && front->sector->ceilingheight > back->sector->ceilingheight) {
      // Store vertex_start in the appropriate structure (this would need to be added to sidedef)
      front->upper_section.vertex_start = map->walls.num_vertices;
      front->upper_section.vertex_count = 4;
      front->upper_section.texture = get_texture(front->def->toptexture);
      
      float height = fabs((float)(back->sector->ceilingheight - front->sector->ceilingheight));
      
      init_wall_vertex(v1->x, v1->y, back->sector->ceilingheight, 0.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, back->sector->ceilingheight, 1.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, front->sector->ceilingheight, 1.0f, 0.0f);
      init_wall_vertex(v1->x, v1->y, front->sector->ceilingheight, 0.0f, 0.0f);
    }
    
    // Add vertices for lower texture (if floor heights differ)
    if (back && front->sector->floorheight < back->sector->floorheight) {
      front->lower_section.vertex_start = map->walls.num_vertices;
      front->lower_section.vertex_count = 4;
      front->lower_section.texture = get_texture(front->def->bottomtexture);
      
      float height = fabs((float)(back->sector->floorheight - front->sector->floorheight));
      
      init_wall_vertex(v1->x, v1->y, front->sector->floorheight, 0.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, front->sector->floorheight, 1.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, back->sector->floorheight, 1.0f, 0.0f);
      init_wall_vertex(v1->x, v1->y, back->sector->floorheight, 0.0f, 0.0f);
    }
    
    // Add vertices for middle texture
    if (get_texture(front->def->midtexture)) {
      front->mid_section.vertex_start = map->walls.num_vertices;
      front->mid_section.vertex_count = 4;
      front->mid_section.texture = get_texture(front->def->midtexture);
      
      float bottom = back ? MAX(front->sector->floorheight, back->sector->floorheight) : front->sector->floorheight;
      float top = back ? MIN(front->sector->ceilingheight, back->sector->ceilingheight) : front->sector->ceilingheight;
      float height = fabs(top - bottom);
      
      init_wall_vertex(v1->x, v1->y, bottom, 0.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, bottom, 1.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, top, 1.0f, 0.0f);
      init_wall_vertex(v1->x, v1->y, top, 0.0f, 0.0f);
    }
    
    // Process back side if it exists
    if (back) {
      u_offset = back->def->textureoffset;
      v_offset = back->def->rowoffset;
      // Add vertices for upper texture (if ceiling heights differ)
      if (back->sector->ceilingheight > front->sector->ceilingheight) {
        back->upper_section.vertex_start = map->walls.num_vertices;
        back->upper_section.vertex_count = 4;
        back->upper_section.texture = get_texture(back->def->toptexture);
        
        float height = fabs((float)(back->sector->ceilingheight - front->sector->ceilingheight));
        
        init_wall_vertex(v2->x, v2->y, front->sector->ceilingheight, 0.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, front->sector->ceilingheight, 1.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, back->sector->ceilingheight, 1.0f, 0.0f);
        init_wall_vertex(v2->x, v2->y, back->sector->ceilingheight, 0.0f, 0.0f);
      }
      
      // Add vertices for lower texture (if floor heights differ)
      if (back->sector->floorheight < front->sector->floorheight) {
        back->lower_section.vertex_start = map->walls.num_vertices;
        back->lower_section.vertex_count = 4;
        back->lower_section.texture = get_texture(back->def->bottomtexture);
        
        float height = fabs((float)(back->sector->floorheight - front->sector->floorheight));
        
        init_wall_vertex(v2->x, v2->y, back->sector->floorheight, 0.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, back->sector->floorheight, 1.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, front->sector->floorheight, 1.0f, 0.0f);
        init_wall_vertex(v2->x, v2->y, front->sector->floorheight, 0.0f, 0.0f);
      }
      
      // Add vertices for middle texture
      if (get_texture(back->def->midtexture)) {
        back->mid_section.vertex_start = map->walls.num_vertices;
        back->mid_section.vertex_count = 4;
        back->mid_section.texture = get_texture(back->def->midtexture);
        
        float bottom = MAX(front->sector->floorheight, back->sector->floorheight);
        float top = MIN(front->sector->ceilingheight, back->sector->ceilingheight);
        float height = fabs(top - bottom);
        
        init_wall_vertex(v2->x, v2->y, bottom, 0.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, bottom, 1.0f, 1.0f);
        init_wall_vertex(v1->x, v1->y, top, 1.0f, 0.0f);
        init_wall_vertex(v2->x, v2->y, top, 0.0f, 0.0f);
      }
    }
    if (!wall_created) { // create dummy wall, for top-down view
      front->mid_section.vertex_start = map->walls.num_vertices;
      front->mid_section.vertex_count = 4;
      front->mid_section.texture = get_texture(front->def->midtexture);
      
      float bottom = back ? MAX(front->sector->floorheight, back->sector->floorheight) : front->sector->floorheight;
      float top = back ? MIN(front->sector->ceilingheight, back->sector->ceilingheight) : front->sector->ceilingheight;
      float height = fabs(top - bottom);
      
      init_wall_vertex(v1->x, v1->y, bottom, 0.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, bottom, 1.0f, 1.0f);
      init_wall_vertex(v2->x, v2->y, bottom, 1.0f, 0.0f);
      init_wall_vertex(v1->x, v1->y, bottom, 0.0f, 0.0f);
    }
  }
  
  // Upload vertex data to GPU
  glBindVertexArray(map->walls.vao);
  glBindBuffer(GL_ARRAY_BUFFER, map->walls.vbo);
  glBufferData(GL_ARRAY_BUFFER, map->walls.num_vertices * sizeof(wall_vertex_t), map->walls.vertices, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, x)); // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, u)); // UV
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, nx)); // Normal
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(wall_vertex_t), OFFSET_OF(wall_vertex_t, color)); // Color
  glEnableVertexAttribArray(3);

//  printf("Built wall vertex buffer with %d vertices\n", map->walls.num_vertices);
}

// Helper function to draw a textured quad from the wall vertex buffer
void draw_textured_surface(wall_section_t const *surface, float light, int mode) {
  if (surface->texture) {
    glBindTexture(GL_TEXTURE_2D, surface->texture->texture);
    glUniform2f(glGetUniformLocation(world_prog, "tex0_size"), surface->texture->width, surface->texture->height);
  } else {
    glBindTexture(GL_TEXTURE_2D, no_tex);
    glUniform2f(glGetUniformLocation(world_prog, "tex0_size"), 1, 1);
  }
  glUniform1i(glGetUniformLocation(world_prog, "tex0"), 0);
  glUniform1f(glGetUniformLocation(world_prog, "light"), light);
  
  // Draw 4 vertices starting at vertex_start
  glDrawArrays(mode, surface->vertex_start, surface->vertex_count);
}

// Draw the map from player perspective
void draw_walls(map_data_t const *map,
                mapsector_t const *sector,
                viewdef_t const *viewdef)
{
  // Bind the wall VAO
  glBindVertexArray(map->walls.vao);
  
  glUniformMatrix4fv(glGetUniformLocation(world_prog, "mvp"), 1, GL_FALSE, viewdef->mvp[0]);
  glUniform3fv(glGetUniformLocation(world_prog, "viewPos"), 1, viewdef->viewpos);
  
  // Draw linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Check if front sidedef exists
    if (linedef->sidenum[0] >= map->num_sidedefs) {
      continue; // Skip if invalid front sidedef
    }
    
    mapsidedef2_t const *front = &map->walls.sections[linedef->sidenum[0]];
    float light = front->sector->lightlevel / 255.0f;
    
    extern int pixel;

    if (front->sector == sector) {
      // Draw front side
      if (CHECK_PIXEL(pixel, TOP, linedef->sidenum[0])) {
        draw_textured_surface(&front->upper_section, HIGHLIGHT(light), GL_TRIANGLE_FAN);
      } else if (front->upper_section.texture || strncmp(front->sector->ceilingpic, "F_SKY", 5)) {
        draw_textured_surface(&front->upper_section, light, GL_TRIANGLE_FAN);
      }
      if (CHECK_PIXEL(pixel, BOTTOM, linedef->sidenum[0])) {
        draw_textured_surface(&front->lower_section, HIGHLIGHT(light), GL_TRIANGLE_FAN);
      } else {
        draw_textured_surface(&front->lower_section, light, GL_TRIANGLE_FAN);
      }
      if (CHECK_PIXEL(pixel, MID, linedef->sidenum[0])) {
        draw_textured_surface(&front->mid_section, HIGHLIGHT(light), GL_TRIANGLE_FAN);
      } else {
        draw_textured_surface(&front->mid_section, light, GL_TRIANGLE_FAN);
      }
    }

    // Draw back side if it exists
    if (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs) {
      mapsidedef2_t const *back = &map->walls.sections[linedef->sidenum[1]];
      if (back->sector == sector) {
        draw_textured_surface(&back->upper_section, light, GL_TRIANGLE_FAN);
        draw_textured_surface(&back->lower_section, light, GL_TRIANGLE_FAN);
        draw_textured_surface(&back->mid_section, light, GL_TRIANGLE_FAN);
      }
    }
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}

// Helper function to draw a textured quad from the wall vertex buffer
void draw_textured_surface_id(wall_section_t const *surface, uint32_t id, int mode) {
  uint8_t *c = (uint8_t *)&id;

  glBindTexture(GL_TEXTURE_2D, white_tex);
  glUniform1i(glGetUniformLocation(ui_prog, "tex0"), 0);
  glUniform2f(glGetUniformLocation(ui_prog, "tex0_size"), 1, 1);
  glUniform4f(glGetUniformLocation(ui_prog, "color"), c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
  
  // Draw 4 vertices starting at vertex_start
  glDrawArrays(mode, surface->vertex_start, surface->vertex_count);
}

void
draw_wall_ids(map_data_t const *map,
              mapsector_t const *sector,
              viewdef_t const *viewdef)
{
  glBindVertexArray(map->walls.vao);
  glDisableVertexAttribArray(3);
  glVertexAttrib4f(3, 0, 0, 0, 0);
  // Draw linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Check if front sidedef exists
    if (linedef->sidenum[0] != 0xFFFF) {
      uint32_t sidenum = linedef->sidenum[0];
      mapsidedef2_t const *front = &map->walls.sections[linedef->sidenum[0]];
      if (front->sector == sector) {
        draw_textured_surface_id(&front->upper_section, sidenum|PIXEL_TOP, GL_TRIANGLE_FAN);
        draw_textured_surface_id(&front->lower_section, sidenum|PIXEL_BOTTOM, GL_TRIANGLE_FAN);
        draw_textured_surface_id(&front->mid_section, sidenum|PIXEL_MID, GL_TRIANGLE_FAN);
      }
    }

    // Draw back side if it exists
    if (linedef->sidenum[1] != 0xFFFF) {
      uint32_t sidenum = linedef->sidenum[1];
      mapsidedef2_t const *back = &map->walls.sections[linedef->sidenum[1]];
      if (back->sector == sector) {
        draw_textured_surface_id(&back->upper_section, sidenum|PIXEL_TOP, GL_TRIANGLE_FAN);
        draw_textured_surface_id(&back->lower_section, sidenum|PIXEL_BOTTOM, GL_TRIANGLE_FAN);
        draw_textured_surface_id(&back->mid_section, sidenum|PIXEL_MID, GL_TRIANGLE_FAN);
      }
    }
  }
  
  // Reset texture binding
  glEnableVertexAttribArray(3);
  glBindTexture(GL_TEXTURE_2D, 0);
}

#define MINIMAP_SCALE 2

#define ROTATE_MAP

void minimap_matrix(player_t const *player, mat4 mvp) {
  mat4 proj, view;
  
  float w = SCREEN_WIDTH*MINIMAP_SCALE;
  float h = SCREEN_HEIGHT*MINIMAP_SCALE;
  
  glm_ortho(w, -w, h, -h, -1000, 1000, proj);
  
  // Set up view matrix to center on player
#ifndef ROTATE_MAP
  glm_translate_make(view, (vec3){ -player->x, -player->y, 0.0f });
#else
  mat4 trans, rot;
  // Set up the translation matrix to center on the player
  glm_translate_make(trans, (vec3){ -player->x, -player->y, 0.0f });
  
  // Step 2: Rotate the world so player's angle is at the top
  float angle_rad = glm_rad(player->angle+90); // rotate counter-clockwise to make player face up
  glm_rotate_make(rot, angle_rad, (vec3){0.0f, 0.0f, 1.0f});
  
  // Step 3: Combine rotation and translation
  glm_mat4_mul(rot, trans, view); // view = rot * trans
#endif
  // Step 4: Combine with projection
  glm_mat4_mul(proj, view, mvp);  // mvp = proj * view
}

void draw_minimap(map_data_t const *map, player_t const *player) {
  mat4 mvp;
  
  minimap_matrix(player, mvp);
  
  glUseProgram(ui_prog);
  glBindTexture(GL_TEXTURE_2D, no_tex);
  glUniformMatrix4fv(glGetUniformLocation(ui_prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 1.0f, 0.25f);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glBindVertexArray(map->walls.vao);
  glDrawArrays(GL_LINES, 0, map->walls.num_vertices);
  glDisable(GL_BLEND);
}
