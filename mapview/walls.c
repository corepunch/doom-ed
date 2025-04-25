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
    mapsidedef2_t *front = &map->walls.sections[linedef->sidenum[0]];
    mapsidedef2_t *back = NULL;
    
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
  }
  
  // Upload vertex data to GPU
  glBindVertexArray(map->walls.vao);
  glBindBuffer(GL_ARRAY_BUFFER, map->walls.vbo);
  glBufferData(GL_ARRAY_BUFFER, map->walls.num_vertices * sizeof(wall_vertex_t), map->walls.vertices, GL_STATIC_DRAW);
  
  // Set up vertex attributes
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), (void*)0); // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), (void*)(3 * sizeof(int16_t))); // UV
  glEnableVertexAttribArray(1);
  
  printf("Built wall vertex buffer with %d vertices\n", map->walls.num_vertices);
}

// Helper function to draw a textured quad from the wall vertex buffer
void draw_textured_surface(wall_section_t const *surface, float light, int mode) {
  if (!surface->texture)
    return;
  
  glBindTexture(GL_TEXTURE_2D, surface->texture->texture);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
  glUniform2f(glGetUniformLocation(prog, "tex0_size"), surface->texture->width, surface->texture->height);
  glUniform4f(glGetUniformLocation(prog, "color"), light * BRIGHTNESS, light * BRIGHTNESS, light * BRIGHTNESS, 1);
  
  // Draw 4 vertices starting at vertex_start
  glDrawArrays(mode, surface->vertex_start, surface->vertex_count);
}

// Draw the map from player perspective
void draw_walls(map_data_t const *map, mat4 mvp) {
  // Bind the wall VAO
  glBindVertexArray(map->walls.vao);
  
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
    if (pixel == i) {
      light += 0.25;
    }

    // Draw front side
    draw_textured_surface(&front->upper_section, light, GL_TRIANGLE_FAN);
    draw_textured_surface(&front->lower_section, light, GL_TRIANGLE_FAN);
    draw_textured_surface(&front->mid_section, light, GL_TRIANGLE_FAN);

    // Draw back side if it exists
    if (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs) {
      mapsidedef2_t const *back = &map->walls.sections[linedef->sidenum[1]];
      draw_textured_surface(&back->upper_section, light, GL_TRIANGLE_FAN);
      draw_textured_surface(&back->lower_section, light, GL_TRIANGLE_FAN);
      draw_textured_surface(&back->mid_section, light, GL_TRIANGLE_FAN);
    }
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}

// Helper function to draw a textured quad from the wall vertex buffer
void draw_textured_surface_id(wall_section_t const *surface, int id, int mode) {
  if (!surface->texture)
    return;
  
  glBindTexture(GL_TEXTURE_2D, 1);
  glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
  glUniform2f(glGetUniformLocation(prog, "tex0_size"), 1, 1);
  
  uint8_t *c = (uint8_t *)&id;
  glUniform4f(glGetUniformLocation(prog, "color"), c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
  
  // Draw 4 vertices starting at vertex_start
  glDrawArrays(mode, surface->vertex_start, surface->vertex_count);
}

void draw_wall_ids(map_data_t const *map, mat4 mvp) {
  // Bind the wall VAO
  glBindVertexArray(map->walls.vao);
  
  // Draw linedefs
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    
    // Check if front sidedef exists
    if (linedef->sidenum[0] >= map->num_sidedefs) {
      continue; // Skip if invalid front sidedef
    }
    
    mapsidedef2_t const *front = &map->walls.sections[linedef->sidenum[0]];
    
    // Draw front side
    draw_textured_surface_id(&front->upper_section, i, GL_TRIANGLE_FAN);
    draw_textured_surface_id(&front->lower_section, i, GL_TRIANGLE_FAN);
    draw_textured_surface_id(&front->mid_section, i, GL_TRIANGLE_FAN);
  }
  
  // Reset texture binding
  glBindTexture(GL_TEXTURE_2D, 0);
}
