#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "map.h"

#define init_wall_vertex(X, Y, Z, U, V) \
map->walls.vertices[map->walls.num_vertices++] = \
(wall_vertex_t) {X,Y,Z,U*dist+u_offset,V*height+v_offset}

extern GLuint prog;

void build_wall_vertex_buffer(map_data_t *map) {
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
    mapsidedef_sections_t *front = &map->walls.sections[linedef->sidenum[0]];
    mapsidedef_sections_t *back = NULL;

    float const dist = sqrtf((v1->x-v2->x) * (v1->x-v2->x) + (v1->y-v2->y) * (v1->y-v2->y));

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
  }
  
  // Upload vertex data to GPU
  glBindVertexArray(map->walls.vao);
  glBindBuffer(GL_ARRAY_BUFFER, map->walls.vbo);
  glBufferData(GL_ARRAY_BUFFER, map->walls.num_vertices * sizeof(wall_vertex_t), map->walls.vertices, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(wall_vertex_t), (void*)0); // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(wall_vertex_t), (void*)(3 * sizeof(float))); // UV
  glEnableVertexAttribArray(1);
  
  printf("Built wall vertex buffer with %d vertices\n", map->walls.num_vertices);
}

float get_wall_direction_shading(vec2s v1, vec2s v2) {
  vec2s dir = glms_vec2_normalize(glms_vec2_sub(v2, v1));
  float shading = fabsf(dir.x); // Y contributes more as it becomes vertical
  return shading; // 0 = horizontal, 1 = vertical
}

// Helper function to draw a textured quad from the wall vertex buffer
void draw_textured_quad(uint32_t vertex_start, mapside_texture_t *texture, float light) {
  if (!texture)
    return;
  
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
  glUniform2f(glGetUniformLocation(prog, "tex0_size"), texture->width, texture->height);
  glUniform3f(glGetUniformLocation(prog, "color"), light * BRIGHTNESS, light * BRIGHTNESS, light * BRIGHTNESS);

  // Draw 4 vertices starting at vertex_start
  glDrawArrays(GL_TRIANGLE_FAN, vertex_start, 4);
}

// Helper function to draw a sidedef
void draw_sidedef(const mapsidedef_sections_t *sidedef,
                  const mapsector_t *sector,
                  vec2s a, vec2s b)
{
//  float dist = glms_vec2_distance(a, b);
  float light = sector->lightlevel / 255.0f;
  float shade = get_wall_direction_shading(a, b) * 0.4 + 0.6;
  float u_offset = sidedef->def->textureoffset / 64.0f;
  float v_offset = sidedef->def->rowoffset / 64.0f;
  light *= shade;
  glUniform2f(glGetUniformLocation(prog, "texOffset"), u_offset, v_offset);
  glUniform3f(glGetUniformLocation(prog, "color"), light * BRIGHTNESS, light * BRIGHTNESS, light * BRIGHTNESS);
  // Draw upper texture if available
  if (sidedef->upper_section.texture) {
    // Apply texture offsets
//    glUniform2f(glGetUniformLocation(prog, "texScale"),
//                dist / sidedef->upper_section.texture->width,
//                fabs(top - bottom) / sidedef->upper_section.texture->height);
    draw_textured_quad(sidedef->upper_section.vertex_start, sidedef->upper_section.texture, light);
  }
  
  // Draw lower texture if available
  if (sidedef->lower_section.texture) {
    draw_textured_quad(sidedef->lower_section.vertex_start, sidedef->lower_section.texture, light);
  }
  
  // Draw middle texture if available
  if (sidedef->mid_section.texture) {
    draw_textured_quad(sidedef->mid_section.vertex_start, sidedef->mid_section.texture, light);
  }
}
// Draw the map from player perspective
void draw_walls(map_data_t const *map, mat4 mvp) {
  glUseProgram(prog);
  glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  
  // Bind the wall VAO
  glBindVertexArray(map->walls.vao);
  
  // Draw linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Check if front sidedef exists
    if (linedef->sidenum[0] >= map->num_sidedefs) {
      continue; // Skip if invalid front sidedef
    }
    
    mapsidedef_sections_t const *front = &map->walls.sections[linedef->sidenum[0]];
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    vec2s a = {v1->x,v1->y}, b = {v2->x,v2->y};

    // Draw front side
    draw_sidedef(front, front->sector, a, b);
    
    // Draw back side if it exists
    if (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs) {
      mapsidedef_sections_t const *back = &map->walls.sections[linedef->sidenum[1]];
      draw_sidedef(back, back->sector, a, b);
    }
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}
