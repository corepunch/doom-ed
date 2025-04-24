#include "map.h"
#include <math.h>

// Maximum expected number of vertices in a sector
#define MAX_VERTICES 1024
#define EPSILON 1e-6  // Small value for floating point comparisons

// Returns 2x the signed area of the triangle
static float signed_area(float ax, float ay, float bx, float by, float cx, float cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

// Check if point p is inside the triangle abc
static int point_in_triangle(float px, float py,
                             float ax, float ay,
                             float bx, float by,
                             float cx, float cy) {
  float area_abc = signed_area(ax, ay, bx, by, cx, cy);
  float area_pab = signed_area(px, py, ax, ay, bx, by);
  float area_pbc = signed_area(px, py, bx, by, cx, cy);
  float area_pca = signed_area(px, py, cx, cy, ax, ay);
  
  // All should be same sign (all positive or all negative)
  if (area_abc < 0) {
    return area_pab <= EPSILON && area_pbc <= EPSILON && area_pca <= EPSILON;
  } else {
    return area_pab >= -EPSILON && area_pbc >= -EPSILON && area_pca >= -EPSILON;
  }
}

// Check if vertex at index i is an ear
static int is_ear(mapvertex_t *vertices, int *indices, int count, int i) {
  int prev = (i > 0) ? i - 1 : count - 1;
  int next = (i < count - 1) ? i + 1 : 0;
  
  float ax = vertices[indices[prev]].x;
  float ay = vertices[indices[prev]].y;
  float bx = vertices[indices[i]].x;
  float by = vertices[indices[i]].y;
  float cx = vertices[indices[next]].x;
  float cy = vertices[indices[next]].y;
  
  // If the ear is not convex, it's not an ear
  if (signed_area(ax, ay, bx, by, cx, cy) <= EPSILON) return 0;
  
  // Check if any other vertex is inside this triangle
  for (int j = 0; j < count; j++) {
    if (j == prev || j == i || j == next) continue;
    
    float px = vertices[indices[j]].x;
    float py = vertices[indices[j]].y;
    
    if (point_in_triangle(px, py, ax, ay, bx, by, cx, cy)) {
      return 0;
    }
  }
  
  return 1;
}

// Modified function signature as requested
int triangulate_sector(mapvertex_t *vertices, int vertex_count, float *out_vertices) {
  if (vertex_count < 3) {
    return 0;  // Return 0 to indicate no triangles created
  }
  
  // Compute polygon signed area to determine winding
  float polygon_area = 0.0f;
  for (int i = 0; i < vertex_count; ++i) {
    int j = (i + 1) % vertex_count;
    polygon_area += (vertices[i].x * vertices[j].y) - (vertices[j].x * vertices[i].y);
  }
  if (polygon_area < 0) {
    // Reverse winding to make it counter-clockwise
    for (int i = 0; i < vertex_count / 2; ++i) {
      mapvertex_t temp = vertices[i];
      vertices[i] = vertices[vertex_count - 1 - i];
      vertices[vertex_count - 1 - i] = temp;
    }
  }
  
  // Use dynamic allocation instead of static buffers to handle any size
  int *indices = malloc(vertex_count * sizeof(int));
  if (!indices) return 0;  // Memory allocation failed
  
  int *triangle_indices = malloc(vertex_count * 3 * sizeof(int));
  if (!triangle_indices) {
    free(indices);
    return 0;  // Memory allocation failed
  }
  
  int triangle_index_count = 0;
  
  // Initialize indices array
  for (int i = 0; i < vertex_count; i++) {
    indices[i] = i;
  }
  
  int remaining = vertex_count;
  
  // Main ear cutting loop
  int safety_counter = 0;  // Prevent infinite loops
  while (remaining > 3 && safety_counter < vertex_count * 2) {
    safety_counter++;
    
    // Find the best ear (maximize triangle area for better triangulation)
    int ear_index = -1;
    float best_area = -1.0f;
    
    for (int i = 0; i < remaining; i++) {
      if (is_ear(vertices, indices, remaining, i)) {
        int prev_idx = (i > 0) ? i - 1 : remaining - 1;
        int next_idx = (i < remaining - 1) ? i + 1 : 0;
        
        float area = fabs(signed_area(
                                      vertices[indices[prev_idx]].x, vertices[indices[prev_idx]].y,
                                      vertices[indices[i]].x, vertices[indices[i]].y,
                                      vertices[indices[next_idx]].x, vertices[indices[next_idx]].y
                                      ));
        
        if (ear_index == -1 || area > best_area) {
          ear_index = i;
          best_area = area;
        }
      }
    }
    
    // If no ear is found, use first vertex as fallback
    if (ear_index == -1) {
      ear_index = 0;
    }
    
    // Get indices of the triangle
    int prev_index = (ear_index > 0) ? ear_index - 1 : remaining - 1;
    int next_index = (ear_index < remaining - 1) ? ear_index + 1 : 0;
    
    // Add the triangle
    triangle_indices[triangle_index_count++] = indices[prev_index];
    triangle_indices[triangle_index_count++] = indices[ear_index];
    triangle_indices[triangle_index_count++] = indices[next_index];
    
    // Remove the ear from the polygon
    for (int i = ear_index; i < remaining - 1; i++) {
      indices[i] = indices[i + 1];
    }
    remaining--;
  }
  
  // Add the final triangle
  if (remaining == 3) {
    triangle_indices[triangle_index_count++] = indices[0];
    triangle_indices[triangle_index_count++] = indices[1];
    triangle_indices[triangle_index_count++] = indices[2];
  }
  
  // Convert triangle indices to output vertices
  int out_vertex_count = 0;
  for (int i = 0; i < triangle_index_count; i++) {
    int idx = triangle_indices[i];
    out_vertices[out_vertex_count++] = vertices[idx].x;
    out_vertices[out_vertex_count++] = vertices[idx].y;
    out_vertices[out_vertex_count++] = 0;
    out_vertices[out_vertex_count++] = 0;
    out_vertices[out_vertex_count++] = 0;
  }
  
  // Free allocated memory
  free(indices);
  free(triangle_indices);
  
  return out_vertex_count;  // Return the number of vertices in the triangulation
}
