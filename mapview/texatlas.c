#include "map.h"

#define CELL_SIZE 16       // Size of each cell in the mask buffer
//#define DISPLAY_WIDTH 640  // Width of the display area
#define DISPLAY_HEIGHT 2000 // Height of the display area

// Cell availability: 0 = free, 1 = occupied
typedef struct {
  int width;          // Width in cells
  int height;         // Height in cells
  uint8_t* mask;      // Occupancy mask
} texture_layout_mask_t;

// Texture position entry - stores position of a texture on screen
typedef struct {
  int x;              // X position on screen
  int y;              // Y position on screen
  int width;          // Original texture width
  int height;         // Original texture height
  int cell_w;         // Width in cells
  int cell_h;         // Height in cells
  int texture_idx;    // Index of the corresponding texture
} texture_layout_entry_t;

// Result of layout generation
typedef struct {
  int num_entries;                   // Number of entries
  int display_width;                 // Display width
  int display_height;                // Display height
  texture_layout_entry_t entries[];  // Array of texture entries
} texture_layout_t;

// Check if a region in the mask is available
bool is_region_available(texture_layout_mask_t* mask, int start_x, int start_y, int width, int height) {
  // Ensure region fits within layout bounds
  if (start_x + width > mask->width || start_y + height > mask->height) {
    return false;
  }
  
  // Check if all cells in region are free
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int cell_idx = (start_y + y) * mask->width + (start_x + x);
      if (mask->mask[cell_idx] != 0) {
        return false;
      }
    }
  }
  
  return true;
}

// Mark a region in the mask as occupied
void mark_region_occupied(texture_layout_mask_t* mask, int start_x, int start_y, int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int cell_idx = (start_y + y) * mask->width + (start_x + x);
      mask->mask[cell_idx] = 1;
    }
  }
}

// Find space for a texture with a first-fit approach
bool find_texture_position(texture_layout_mask_t* mask, int tex_width, int tex_height, int* out_x, int* out_y) {
  // Calculate number of cells needed
  int cells_w = (tex_width + CELL_SIZE - 1) / CELL_SIZE;  // Round up
  int cells_h = (tex_height + CELL_SIZE - 1) / CELL_SIZE; // Round up
  
  // Iterate through all possible positions
  for (int y = 0; y < mask->height; y++) {
    for (int x = 0; x < mask->width; x++) {
      if (is_region_available(mask, x, y, cells_w, cells_h)) {
        // Found space, mark it and return position
        mark_region_occupied(mask, x, y, cells_w, cells_h);
        *out_x = x * CELL_SIZE;
        *out_y = y * CELL_SIZE;
        return true;
      }
    }
  }
  
  // No space found
  return false;
}

// Texture sorting structure for qsort
typedef struct {
  int index;          // Original index
  int width;          // Width of texture
  int height;         // Height of texture
  int area;           // Area of texture (width * height)
} texture_sort_entry_t;

// Comparison function for sorting textures by width (descending)
int compare_textures_by_width(const void* a, const void* b) {
  const texture_sort_entry_t* tex_a = (const texture_sort_entry_t*)a;
  const texture_sort_entry_t* tex_b = (const texture_sort_entry_t*)b;
  
  // Sort by width (descending)
  return tex_b->width - tex_a->width;
}

// Comparison function for sorting textures by height (descending)
int compare_textures_by_height(const void* a, const void* b) {
  const texture_sort_entry_t* tex_a = (const texture_sort_entry_t*)a;
  const texture_sort_entry_t* tex_b = (const texture_sort_entry_t*)b;
  
  // Sort by height (descending)
  return tex_b->height - tex_a->height;
}

// Comparison function for sorting textures by area (descending)
int compare_textures_by_area(const void* a, const void* b) {
  const texture_sort_entry_t* tex_a = (const texture_sort_entry_t*)a;
  const texture_sort_entry_t* tex_b = (const texture_sort_entry_t*)b;
  
  // Sort by area (descending)
  return tex_b->area - tex_a->area;
}

// Generate a texture layout for display
texture_layout_t*
generate_texture_layout(mapside_texture_t* textures,
                        int num_textures,
                        int DISPLAY_WIDTH)
{
  // Determine layout dimensions based on cell size
  int cells_w = DISPLAY_WIDTH / CELL_SIZE;
  int cells_h = DISPLAY_HEIGHT / CELL_SIZE;
  
  // Create mask buffer
  texture_layout_mask_t mask = {
    .width = cells_w,
    .height = cells_h,
    .mask = (uint8_t*)calloc(cells_w * cells_h, sizeof(uint8_t))
  };
  
  // Allocate result structure
  texture_layout_t* layout = (texture_layout_t*)malloc(sizeof(texture_layout_t)+num_textures * sizeof(texture_layout_entry_t));
  layout->num_entries = 0;
  layout->display_width = DISPLAY_WIDTH;
  layout->display_height = DISPLAY_HEIGHT;
  
  // Create sorted indices array
  texture_sort_entry_t* sorted_textures = (texture_sort_entry_t*)malloc(num_textures * sizeof(texture_sort_entry_t));
  for (int i = 0; i < num_textures; i++) {
    sorted_textures[i].index = i;
    sorted_textures[i].width = textures[i].width;
    sorted_textures[i].height = textures[i].height;
    sorted_textures[i].area = textures[i].width * textures[i].height * textures[i].height;
  }
  
  // Sort textures by width (descending)
  qsort(sorted_textures, num_textures, sizeof(texture_sort_entry_t), compare_textures_by_area);
  
  // Place each texture in the layout (in sorted order)
  for (int i = 0; i < num_textures; i++) {
    int original_idx = sorted_textures[i].index;
    mapside_texture_t* tex = &textures[original_idx];
    int pos_x, pos_y;
    
    // Try to find space for this texture
    int cell_w = (tex->width + CELL_SIZE - 1) / CELL_SIZE;
    int cell_h = (tex->height + CELL_SIZE - 1) / CELL_SIZE;
    
    if (find_texture_position(&mask, tex->width, tex->height, &pos_x, &pos_y)) {
      // Store entry information
      texture_layout_entry_t* entry = &layout->entries[layout->num_entries++];
      entry->x = pos_x;
      entry->y = pos_y;
      entry->width = tex->width;
      entry->height = tex->height;
      entry->cell_w = cell_w;
      entry->cell_h = cell_h;
      entry->texture_idx = original_idx;
    } else {
      // Layout is full - you might want to handle this case differently
      // e.g., create a scrollable layout or multiple pages
      printf("Warning: Could not fit texture %d in layout!\n", original_idx);
    }
  }
  
  // Clean up
  free(sorted_textures);
  free(mask.mask);
  
  return layout;
}

// Function to draw all textures according to the layout
void draw_texture_layout(texture_layout_t* layout, mapside_texture_t* textures, float scale) {
  for (int i = 0; i < layout->num_entries; i++) {
    texture_layout_entry_t* entry = &layout->entries[i];
    mapside_texture_t* tex = &textures[entry->texture_idx];
    
    // Draw the texture according to its position in the layout
    draw_rect(tex->texture, entry->x, entry->y, tex->width * scale, tex->height * scale);
  }
}

// Function to draw the layout with a specific texture highlighted
void draw_texture_layout_with_selection(texture_layout_t* layout,
                                        mapside_texture_t* textures,
                                        char const *selected_texture,
                                        float scale,
                                        float wx, float wy) {
  extern int black_tex, selection_tex;
  
  for (int i = 0; i < layout->num_entries; i++) {
    texture_layout_entry_t* entry = &layout->entries[i];
    mapside_texture_t* tex = &textures[entry->texture_idx];
    
    // Draw the texture
    draw_rect(tex->texture, entry->x * scale + wx, entry->y * scale, tex->width * scale, tex->height * scale);
    draw_rect_ex(black_tex, entry->x * scale + wx, entry->y * scale, tex->width * scale, tex->height * scale, true);
  }
  
  for (int i = 0; i < layout->num_entries; i++) {
    texture_layout_entry_t* entry = &layout->entries[i];
    mapside_texture_t* tex = &textures[entry->texture_idx];
    if (!strncmp(tex->name, selected_texture, sizeof(texname_t))) {
      draw_rect_ex(selection_tex, entry->x * scale + wx, entry->y * scale, tex->width * scale, tex->height * scale, true);
    }
  }
}

// Function to check if a point is inside a texture
int get_texture_at_point(texture_layout_t* layout, int point_x, int point_y) {
  for (int i = 0; i < layout->num_entries; i++) {
    texture_layout_entry_t* entry = &layout->entries[i];
    
    // Check if point is inside this texture
    if (point_x >= entry->x && point_x < entry->x + entry->width &&
        point_y >= entry->y && point_y < entry->y + entry->height) {
      return entry->texture_idx;
    }
  }
  
  return -1;  // No texture at this point
}

enum {
  key_left, key_right, key_up, key_down
};

// Function to find the closest texture in a given direction from the current selection
int find_texture_in_direction(texture_layout_t* layout,
                              mapside_texture_t* textures,
                              const char* current_texture_name,
                              int direction) {
  int current_idx = -1;
  int current_x = 0, current_y = 0;
  int current_width = 0, current_height = 0;
  
  // First, find the currently selected texture and its position
  for (int i = 0; i < layout->num_entries; i++) {
    texture_layout_entry_t* entry = &layout->entries[i];
    mapside_texture_t* tex = &textures[entry->texture_idx];
    
    if (!strncmp(tex->name, current_texture_name, sizeof(texname_t))) {
      current_idx = i;
      current_x = entry->x + entry->width/2;  // Center point of texture
      current_y = entry->y + entry->height/2;
      current_width = entry->width;
      current_height = entry->height;
      break;
    }
  }
  
  if (current_idx == -1) {
    return -1; // Current texture not found
  }
  
  // Define direction vectors
  int dir_x = 0, dir_y = 0;
  switch (direction) {
    case key_left:
      dir_x = -1;
      break;
    case key_right:
      dir_x = 1;
      break;
    case key_up:
      dir_y = -1;
      break;
    case key_down:
      dir_y = 1;
      break;
    default:
      return -1;
  }
  
  // Find best candidate in that direction
  
  int x = layout->entries[current_idx].x+1;
  int y = layout->entries[current_idx].y+1;
  
  while (true) {
    x += dir_x * 8;
    y += dir_y * 8;
    int n = get_texture_at_point(layout, x, y);
    if (n != layout->entries[current_idx].texture_idx) {
      return n;
    }
  }

  return -1;
}

void
draw_textures_interface(mapside_texture_t* textures,
                        int num_textures,
                        char *selected_texture,
                        float x, float y, float width,
                        bool mouse_clicked,
                        int keydown)
{
  float scale = 0.25f;
  
  texture_layout_t* layout = generate_texture_layout(textures, num_textures, width / scale);

  // Draw the layout with selection highlight
  draw_texture_layout_with_selection(layout, textures, selected_texture, scale, x, y);

  extern float black_bars;
  int mouse_x, mouse_y;
  GetMouseInVirtualCoords(&mouse_x, &mouse_y);

  // Handle mouse click (in mouse event handler)
  if (mouse_clicked) {
    int texture_idx = get_texture_at_point(layout, (mouse_x - x) / scale, (mouse_y - y) / scale);
    if (texture_idx >= 0) {
      memcpy(selected_texture, textures[texture_idx].name, sizeof(texname_t));
    }
  } else {
    int n = find_texture_in_direction(layout, textures, selected_texture, keydown);
    if (n != -1) {
      memcpy(selected_texture, textures[n].name, sizeof(texname_t));
    }
  }
  
  free(layout);
}
