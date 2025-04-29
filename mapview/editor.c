#include <SDL2/SDL.h>
#include <OpenGL/gl3.h>
#include <cglm/struct.h>
#include <math.h>

#include "editor.h"

// Globals
extern GLuint ui_prog;
extern SDL_Window* window;

// Initialize editor state
void init_editor(editor_state_t *editor) {
  editor->active = false;
  editor->grid_size = 32;
  editor->drawing = false;
  editor->num_draw_points = 0;
  
  // Create VAO and VBO for editor rendering
  glGenVertexArrays(1, &editor->vao);
  glGenBuffers(1, &editor->vbo);
  
  // Set up VAO
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  
  // Enable vertex attributes
  glEnableVertexAttribArray(0); // Position
  glEnableVertexAttribArray(1); // Texture coords
  
  // Reset bindings
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Toggle editor mode on/off
void toggle_editor_mode(editor_state_t *editor) {
  editor->active = !editor->active;
  
  // Reset drawing state when exiting editor
  if (!editor->active) {
    editor->drawing = false;
    editor->num_draw_points = 0;
  }
  
  // Set mouse mode based on editor state
  SDL_SetRelativeMouseMode(editor->active ? SDL_FALSE : SDL_TRUE);
}

// Utility function to snap coordinates to grid
static void snap_to_grid(int *x, int *y, int grid_size) {
  *x = (*x / grid_size) * grid_size;
  *y = (*y / grid_size) * grid_size;
}

static void
get_mouse_position(editor_state_t const *editor,
                   player_t const *player,
                   int *snapped_x,
                   int *snapped_y)
{
  int mouse_x, mouse_y;
  // Get mouse position
  SDL_GetMouseState(&mouse_x, &mouse_y);
  
  mouse_x -= editor->grid_size/4;
  mouse_y -= editor->grid_size/4;

  // Convert to world coordinates
  int win_width, win_height;
  SDL_GetWindowSize(window, &win_width, &win_height);
  
  float w = SCREEN_WIDTH * 2;
  float h = SCREEN_HEIGHT * 2;
  
  float world_x = player->x + (mouse_x - win_width/2) * (w / win_width);
  float world_y = player->y + (win_height/2 - mouse_y) * (h / win_height);
  
  // Snap to grid
  *snapped_x = ((int)world_x / editor->grid_size) * editor->grid_size;
  *snapped_y = ((int)world_y / editor->grid_size) * editor->grid_size;
}

// Check if a point exists at the given coordinates (within a small threshold)
bool point_exists(int x, int y, map_data_t *map) {
  const int threshold = 8; // Distance threshold
  
  for (int i = 0; i < map->num_vertices; i++) {
    int dx = map->vertices[i].x - x;
    int dy = map->vertices[i].y - y;
    if (dx*dx + dy*dy < threshold*threshold) {
      return true;
    }
  }
  return false;
}

// Draw the grid
static void draw_grid(int grid_size, player_t const *player, int view_size) {
  int start_x = ((int)player->x / grid_size - view_size) * grid_size;
  int end_x = ((int)player->x / grid_size + view_size) * grid_size;
  int start_y = ((int)player->y / grid_size - view_size) * grid_size;
  int end_y = ((int)player->y / grid_size + view_size) * grid_size;
  
  // Set grid color (dark gray)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 0.3f, 0.3f, 0.3f, 1.0f);
  
  // Draw vertical grid lines
  for (int x = start_x; x <= end_x; x += grid_size) {
    // Create two vertices for each grid line
    wall_vertex_t verts[2] = {
      { x, start_y, 0, 0, 0, 0, 0, 0 },
      { x, end_y, 0, 0, 0, 0, 0, 0 }
    };
    
    // Draw the line
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
  
  // Draw horizontal grid lines
  for (int y = start_y; y <= end_y; y += grid_size) {
    // Create two vertices for each grid line
    wall_vertex_t verts[2] = {
      { start_x, y, 0, 0, 0, 0, 0, 0 },
      { end_x, y, 0, 0, 0, 0, 0, 0 }
    };
    
    // Draw the line
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
}

// Draw the current sector being created
static void draw_current_sector(editor_state_t const *editor) {
  if (editor->num_draw_points < 2) return;
  
  // Set line color (yellow)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 0.0f, 1.0f);
  
  // Draw lines connecting all points
  for (int i = 0; i < editor->num_draw_points - 1; i++) {
    wall_vertex_t verts[2] = {
      { editor->draw_points[i][0], editor->draw_points[i][1], 0, 0, 0, 0, 0, 0 },
      { editor->draw_points[i+1][0], editor->draw_points[i+1][1], 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
  
  // Highlight vertices
  glPointSize(5.0f);
  for (int i = 0; i < editor->num_draw_points; i++) {
    wall_vertex_t vert = {
      editor->draw_points[i][0],
      editor->draw_points[i][1],
      0, 0, 0, 0, 0, 0
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &vert.u);
    glDrawArrays(GL_POINTS, 0, 1);
  }
  glPointSize(1.0f);
}

// Draw walls with different colors based on whether they're one-sided or two-sided
static void draw_walls_editor(map_data_t const *map) {
  // Draw each linedef
  for (int i = 0; i < map->num_linedefs; i++) {
    maplinedef_t const *linedef = &map->linedefs[i];
    mapvertex_t const *v1 = &map->vertices[linedef->start];
    mapvertex_t const *v2 = &map->vertices[linedef->end];
    
    // Determine if this is a one-sided or two-sided wall
    bool two_sided = (linedef->sidenum[1] != 0xFFFF && linedef->sidenum[1] < map->num_sidedefs);
    
    // Set color - white for one-sided (outer) walls, red for two-sided (inner) walls
    if (!two_sided) {
      glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    // Create and draw vertices
    wall_vertex_t verts[2] = {
      { v1->x, v1->y, 0, 0, 0, 0, 0, 0 },
      { v2->x, v2->y, 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
    
    // Draw vertices as points
    glPointSize(3.0f);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glDrawArrays(GL_POINTS, 0, 1);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[1].x);
    glDrawArrays(GL_POINTS, 0, 1);
    glPointSize(1.0f);
  }
}

// Draw cursor position
static void draw_cursor(int x, int y) {
  // Set cursor color (green)
  glUniform4f(glGetUniformLocation(ui_prog, "color"), 0.0f, 1.0f, 0.0f, 1.0f);
  
  // Draw crosshair
  int size = 4;
  wall_vertex_t verts[4] = {
    { x - size, y, 0, 0, 0, 0, 0, 0 },
    { x + size, y, 0, 0, 0, 0, 0, 0 },
    { x, y - size, 0, 0, 0, 0, 0, 0 },
    { x, y + size, 0, 0, 0, 0, 0, 0 }
  };
  
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
  glDrawArrays(GL_LINES, 0, 2);
  
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[2].x);
  glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[2].u);
  glDrawArrays(GL_LINES, 0, 2);
}

// Draw the editor UI
void draw_editor(map_data_t const *map, editor_state_t const *editor, player_t const *player) {
  if (!editor->active) return;
  
  glBindVertexArray(editor->vao);
  glBindBuffer(GL_ARRAY_BUFFER, editor->vbo);
  glDisable(GL_DEPTH_TEST);
  
  // Set up orthographic projection for 2D view
  mat4 mvp;
  float scale = 1;
  float w = SCREEN_WIDTH * scale;
  float h = SCREEN_HEIGHT * scale;
  
  // Create projection and view matrices
  mat4 proj, view;
  glm_ortho(-w, w, -h, h, -1000, 1000, proj);
  
  // Center view on player
  glm_translate_make(view, (vec3){ -player->x, -player->y, 0.0f });
  
  // Combine matrices
  glm_mat4_mul(proj, view, mvp);
  
  // Set up rendering
  glUseProgram(ui_prog);
  glUniformMatrix4fv(glGetUniformLocation(ui_prog, "mvp"), 1, GL_FALSE, (const float*)mvp);
  glBindTexture(GL_TEXTURE_2D, 1); // Use default 1x1 white texture
  
  // Clear the screen to black
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//  void draw_minimap(map_data_t const *map, player_t const *player);
//  draw_minimap(map, player);
//  glBindVertexArray(map->walls.vao);
//  glDrawArrays(GL_LINES, 0, map->walls.num_vertices);
  
  // Draw grid
  draw_grid(editor->grid_size, player, 100);
  
  // Draw existing walls
  draw_walls_editor(map);
  
  // Draw sector being created
  draw_current_sector(editor);
  
  // Snap to grid
  int snapped_x, snapped_y;
  get_mouse_position(editor, player, &snapped_x, &snapped_y);

  // Draw cursor at the snapped position
  draw_cursor(snapped_x, snapped_y);
  
  // If currently drawing, show line from last point to cursor
  if (editor->drawing && editor->num_draw_points > 0) {
    glUniform4f(glGetUniformLocation(ui_prog, "color"), 1.0f, 1.0f, 0.0f, 0.5f);
    wall_vertex_t verts[2] = {
      { editor->draw_points[editor->num_draw_points-1][0],
        editor->draw_points[editor->num_draw_points-1][1], 0, 0, 0, 0, 0, 0 },
      { snapped_x, snapped_y, 0, 0, 0, 0, 0, 0 }
    };
    
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].x);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, sizeof(wall_vertex_t), &verts[0].u);
    glDrawArrays(GL_LINES, 0, 2);
  }
}

// Add a new vertex to the map
static uint16_t add_vertex(map_data_t *map, int x, int y) {
  // Check if we're at capacity
  if (map->num_vertices >= 65535) {
    printf("Maximum number of vertices reached!\n");
    return 0;
  }
  
  // Resize vertices array if needed
  mapvertex_t *new_vertices = realloc(map->vertices, (map->num_vertices + 1) * sizeof(mapvertex_t));
  if (!new_vertices) {
    printf("Failed to allocate memory for new vertex!\n");
    return 0;
  }
  map->vertices = new_vertices;
  
  // Set vertex data
  uint16_t index = map->num_vertices;
  map->vertices[index].x = x;
  map->vertices[index].y = y;
  map->num_vertices++;
  
  return index;
}

// Add a new linedef to the map
static void add_linedef(map_data_t *map, uint16_t start, uint16_t end) {
  // Check if we're at capacity
  if (map->num_linedefs >= 65535) {
    printf("Maximum number of linedefs reached!\n");
    return;
  }
  
  // Resize linedefs array if needed
  maplinedef_t *new_linedefs = realloc(map->linedefs, (map->num_linedefs + 1) * sizeof(maplinedef_t));
  if (!new_linedefs) {
    printf("Failed to allocate memory for new linedef!\n");
    return;
  }
  map->linedefs = new_linedefs;
  
  // Set linedef data
  uint16_t index = map->num_linedefs;
  map->linedefs[index].start = start;
  map->linedefs[index].end = end;
  map->linedefs[index].flags = 1;  // Impassable by default
  map->linedefs[index].special = 0;
  map->linedefs[index].tag = 0;
  map->linedefs[index].sidenum[0] = map->num_sidedefs > 0 ? 0 : 0xFFFF;  // Front sidedef
  map->linedefs[index].sidenum[1] = 0xFFFF;  // No back sidedef
  map->num_linedefs++;
}

// Add these helper functions for sidedef and sector creation
static uint16_t add_sidedef(map_data_t *map, uint16_t sector_index) {
  // Check if we're at capacity
  if (map->num_sidedefs >= 65535) {
    printf("Maximum number of sidedefs reached!\n");
    return 0xFFFF;
  }
  
  // Resize sidedefs array if needed
  mapsidedef_t *new_sidedefs = realloc(map->sidedefs, (map->num_sidedefs + 1) * sizeof(mapsidedef_t));
  if (!new_sidedefs) {
    printf("Failed to allocate memory for new sidedef!\n");
    return 0xFFFF;
  }
  map->sidedefs = new_sidedefs;
  
  // Set sidedef data
  uint16_t index = map->num_sidedefs;
  map->sidedefs[index].textureoffset = 0;
  map->sidedefs[index].rowoffset = 0;
  map->sidedefs[index].toptexture[0] = '-'; // No upper texture by default
  map->sidedefs[index].toptexture[1] = '\0';
  map->sidedefs[index].bottomtexture[0] = '-'; // No lower texture by default
  map->sidedefs[index].bottomtexture[1] = '\0';
  
  strcpy(map->sidedefs[index].midtexture, "BRONZE1");
//  map->sidedefs[index].midtexture[0] = '-'; // Default texture
//  map->sidedefs[index].midtexture[1] = '\0';

  map->sidedefs[index].sector = sector_index;
  map->num_sidedefs++;
  
  return index;
}

static uint16_t add_sector(map_data_t *map) {
  // Check if we're at capacity
  if (map->num_sectors >= 65535) {
    printf("Maximum number of sectors reached!\n");
    return 0xFFFF;
  }
  
  // Resize sectors array if needed
  mapsector_t *new_sectors = realloc(map->sectors, (map->num_sectors + 1) * sizeof(mapsector_t));
  if (!new_sectors) {
    printf("Failed to allocate memory for new sector!\n");
    return 0xFFFF;
  }
  map->sectors = new_sectors;
  
  // Set sector data
  uint16_t index = map->num_sectors;
  map->sectors[index].floorheight = 0;
  map->sectors[index].ceilingheight = 128;
  
  // Set default floor and ceiling textures
  strncpy(map->sectors[index].floorpic, "FLOOR", 8);
  strncpy(map->sectors[index].ceilingpic, "CEIL", 8);
  
  map->sectors[index].lightlevel = 160;   // Default light level
  map->sectors[index].special = 0;        // No special effect
  map->sectors[index].tag = 0;            // No tag
  map->num_sectors++;
  
  return index;
}

// Calculate if points form clockwise or counter-clockwise polygon
static bool is_clockwise(int points[][2], int num_points) {
  double sum = 0.0;
  for (int i = 0; i < num_points; i++) {
    int j = (i + 1) % num_points;
    sum += (points[j][0] - points[i][0]) * (points[j][1] + points[i][1]);
  }
  return sum > 0;
}

// Update the finish_sector function to create sidedefs and a sector
void finish_sector(map_data_t *map, editor_state_t *editor) {
  if (editor->num_draw_points < 3) {
    printf("Need at least 3 points to create a sector!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // First, check if we need to reverse the points to ensure counter-clockwise winding
  // (counter-clockwise is standard for Doom engine and determines the front side)
  bool clockwise = is_clockwise(editor->draw_points, editor->num_draw_points);
  if (clockwise) {
    // Reverse the points
    for (int i = 0; i < editor->num_draw_points / 2; i++) {
      int j = editor->num_draw_points - 1 - i;
      // Swap points
      int temp_x = editor->draw_points[i][0];
      int temp_y = editor->draw_points[i][1];
      editor->draw_points[i][0] = editor->draw_points[j][0];
      editor->draw_points[i][1] = editor->draw_points[j][1];
      editor->draw_points[j][0] = temp_x;
      editor->draw_points[j][1] = temp_y;
    }
  }
  
  // Create a new sector
  uint16_t sector_index = add_sector(map);
  if (sector_index == 0xFFFF) {
    printf("Failed to create sector, aborting sector creation!\n");
    editor->drawing = false;
    editor->num_draw_points = 0;
    return;
  }
  
  // Add all vertices
  uint16_t vertex_indices[MAX_DRAW_POINTS];
  for (int i = 0; i < editor->num_draw_points; i++) {
    vertex_indices[i] = add_vertex(map, editor->draw_points[i][0], editor->draw_points[i][1]);
  }
  
  // Add linedefs and sidedefs
  for (int i = 0; i < editor->num_draw_points; i++) {
    int next = (i + 1) % editor->num_draw_points;
    
    // Check if a linedef already exists between these two vertices
    bool linedef_exists = false;
    uint16_t existing_linedef_index = 0;
    
    for (int j = 0; j < map->num_linedefs; j++) {
      if ((map->linedefs[j].start == vertex_indices[i] && map->linedefs[j].end == vertex_indices[next]) ||
          (map->linedefs[j].start == vertex_indices[next] && map->linedefs[j].end == vertex_indices[i])) {
        linedef_exists = true;
        existing_linedef_index = j;
        break;
      }
    }
    
    if (linedef_exists) {
      // If linedef exists, check if it's already two-sided
      maplinedef_t *linedef = &map->linedefs[existing_linedef_index];
      
      // If it's already two-sided, we have an error (can't have more than 2 sides)
      if (linedef->sidenum[0] != 0xFFFF && linedef->sidenum[1] != 0xFFFF) {
        printf("WARNING: Linedef %d already has two sides!\n", existing_linedef_index);
        continue;
      }
      
      // Check if we need to add a sidedef to front or back
      if (linedef->start == vertex_indices[i] && linedef->end == vertex_indices[next]) {
        // This is the front side (same direction)
        if (linedef->sidenum[0] == 0xFFFF) {
          linedef->sidenum[0] = add_sidedef(map, sector_index);
        }
      } else {
        // This is the back side (opposite direction)
        if (linedef->sidenum[1] == 0xFFFF) {
          linedef->sidenum[1] = add_sidedef(map, sector_index);
        }
      }
      
      // Make sure the linedef is no longer marked as impassable (it's now two-sided)
      linedef->flags &= ~1;  // Remove impassable flag
      linedef->flags |= 4;   // Set two-sided flag
    } else {
      // Create a new linedef
      uint16_t linedef_index = map->num_linedefs;
      
      // Resize linedefs array if needed
      maplinedef_t *new_linedefs = realloc(map->linedefs, (map->num_linedefs + 1) * sizeof(maplinedef_t));
      if (!new_linedefs) {
        printf("Failed to allocate memory for new linedef!\n");
        continue;
      }
      map->linedefs = new_linedefs;
      
      // Set linedef data
      map->linedefs[linedef_index].start = vertex_indices[next];
      map->linedefs[linedef_index].end = vertex_indices[i];
      map->linedefs[linedef_index].flags = 1;  // Impassable by default
      map->linedefs[linedef_index].special = 0;
      map->linedefs[linedef_index].tag = 0;
      
      // Create a front sidedef for this linedef
      map->linedefs[linedef_index].sidenum[0] = add_sidedef(map, sector_index);
      map->linedefs[linedef_index].sidenum[1] = 0xFFFF;  // No back sidedef
      
      map->num_linedefs++;
    }
  }
  
  printf("Created sector %d with %d vertices and %d walls\n",
         sector_index, editor->num_draw_points, editor->num_draw_points);
  
  // Reset drawing state
  editor->drawing = false;
  editor->num_draw_points = 0;
  
  // Rebuild vertex buffers
  build_wall_vertex_buffer(map);
  build_floor_vertex_buffer(map);
}

// Handle mouse and keyboard input for the editor
void handle_editor_input(map_data_t *map, editor_state_t *editor, player_t *player) {
  SDL_Event event;
  
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      extern bool running;
      running = false;
    }
    else if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.scancode) {
        case SDL_SCANCODE_TAB:
          toggle_editor_mode(editor);
          break;
        case SDL_SCANCODE_ESCAPE:
          if (editor->drawing) {
            // Cancel current drawing
            editor->drawing = false;
            editor->num_draw_points = 0;
          } else if (editor->active) {
            // Exit editor mode
            toggle_editor_mode(editor);
          }
          break;
        case SDL_SCANCODE_G:
          // Toggle grid size (8, 16, 32, 64, 128)
          editor->grid_size *= 2;
          if (editor->grid_size > 128) editor->grid_size = 8;
          break;
        default:
          break;
      }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN && editor->active) {
      if (event.button.button == SDL_BUTTON_LEFT) {
        int snapped_x, snapped_y;
        get_mouse_position(editor, player, &snapped_x, &snapped_y);
        
        if (!editor->drawing) {
          // Start drawing new sector
          editor->drawing = true;
          editor->num_draw_points = 0;
        }
        
        // Check if we're clicking on the first point to close the loop
        if (editor->num_draw_points > 2 &&
            abs(snapped_x - editor->draw_points[0][0]) < editor->grid_size/2 &&
            abs(snapped_y - editor->draw_points[0][1]) < editor->grid_size/2) {
          // Finish the sector
          finish_sector(map, editor);
        } else if (editor->num_draw_points < MAX_DRAW_POINTS) {
          // Add point to current sector
          editor->draw_points[editor->num_draw_points][0] = snapped_x;
          editor->draw_points[editor->num_draw_points][1] = snapped_y;
          editor->num_draw_points++;
        }
      }
      else if (event.button.button == SDL_BUTTON_RIGHT) {
        // Cancel current drawing
        editor->drawing = false;
        editor->num_draw_points = 0;
      }
    }
    else if (event.type == SDL_MOUSEWHEEL && editor->active) {
      // Zoom in/out by adjusting view size
      // (This would require implementing zoom in the view matrix)
    }
  }
  
  // If in editor mode, handle keyboard movement
  if (editor->active) {
    const Uint8* keystates = SDL_GetKeyboardState(NULL);
    float move_speed = 32.0f;
    
    if (keystates[SDL_SCANCODE_W] || keystates[SDL_SCANCODE_UP]) {
      player->y += move_speed;
    }
    if (keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN]) {
      player->y -= move_speed;
    }
    if (keystates[SDL_SCANCODE_A] || keystates[SDL_SCANCODE_LEFT]) {
      player->x -= move_speed;
    }
    if (keystates[SDL_SCANCODE_D] || keystates[SDL_SCANCODE_RIGHT]) {
      player->x += move_speed;
    }
  }
}
