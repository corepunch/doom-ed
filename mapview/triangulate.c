#include "map.h"
#include <math.h>

// Helper function to check if a point is inside a triangle
int is_point_in_triangle(float px, float py,
                         float ax, float ay,
                         float bx, float by,
                         float cx, float cy) {
  float area = 0.5f * (-by * cx + ay * (-bx + cx) + ax * (by - cy) + bx * cy);
  // Check for division by zero
  if (fabs(area) < 0.00001f) return 0;
  
  float s = 1.0f / (2.0f * area) * (ay * cx - ax * cy + (cy - ay) * px + (ax - cx) * py);
  float t = 1.0f / (2.0f * area) * (ax * by - ay * bx + (ay - by) * px + (bx - ax) * py);
  return (s > 0 && t > 0 && 1 - s - t > 0) ? 1 : 0;
}

typedef struct {
  float x, y;
  int original_index;
  int is_ear;
} working_vertex_t;

// Helper function to check if a vertex forms an ear
void update_ear_status(int idx, working_vertex_t *verts, int count) {
  int prev = (idx - 1 + count) % count;
  int next = (idx + 1) % count;
  
  // Skip vertices that have been removed
  if (verts[idx].original_index == -1) {
    verts[idx].is_ear = 0;
    return;
  }
  
  // Check if this vertex forms a convex corner
  float ax = verts[prev].x - verts[idx].x;
  float ay = verts[prev].y - verts[idx].y;
  float bx = verts[next].x - verts[idx].x;
  float by = verts[next].y - verts[idx].y;
  
  // Cross product to determine if convex
  float cross = ax * by - ay * bx;
  
  // If concave, it can't be an ear
  if (cross < 0) {
    verts[idx].is_ear = 0;
    return;
  }
  
  // Check if any other vertex is inside this potential ear
  int is_ear = 1;
  for (int j = 0; j < count; j++) {
    // Skip adjacent vertices and removed vertices
    if (j == prev || j == idx || j == next || verts[j].original_index == -1) continue;
    
    if (is_point_in_triangle(verts[j].x, verts[j].y,
                             verts[prev].x, verts[prev].y,
                             verts[idx].x, verts[idx].y,
                             verts[next].x, verts[next].y)) {
      is_ear = 0;
      break;
    }
  }
  
  verts[idx].is_ear = is_ear;
}

void triangulate_sector(mapvertex_t *vertices, int num_vertices, float *out_vertices, int *out_count) {
  // Implementation of ear clipping algorithm for triangulation
  // Output will be suitable for GL_TRIANGLES rendering
  
  if (num_vertices < 3) {
    *out_count = 0;
    return;
  }
  
  // We'll need a working copy of the vertices
  working_vertex_t *working = (working_vertex_t*)malloc(num_vertices * sizeof(working_vertex_t));
  if (!working) {
    *out_count = 0;
    return; // Memory allocation failed
  }
  
  // Copy vertices to working array
  for (int i = 0; i < num_vertices; i++) {
    working[i].x = vertices[i].x;
    working[i].y = vertices[i].y;
    working[i].original_index = i;
    working[i].is_ear = 0; // Will be determined later
  }
  
  // Initialize ear status for all vertices
  for (int i = 0; i < num_vertices; i++) {
    update_ear_status(i, working, num_vertices);
  }
  
  int remaining = num_vertices;
  int vertex_count = 0;
  
  // Process until we have only a triangle left
  while (remaining > 3) {
    // Find the next ear
    int ear_idx = -1;
    for (int i = 0; i < num_vertices; i++) {
      if (working[i].original_index != -1 && working[i].is_ear) {
        ear_idx = i;
        break;
      }
    }
    
    if (ear_idx == -1) {
      // No ears found, which shouldn't happen in a simple polygon
      // Fall back to a simple triangulation
      break;
    }
    
    // Get the previous and next vertices
    int prev = ear_idx;
    do {
      prev = (prev - 1 + num_vertices) % num_vertices;
    } while (working[prev].original_index == -1);
    
    int next = ear_idx;
    do {
      next = (next + 1) % num_vertices;
    } while (working[next].original_index == -1);
    
    // Add vertices for the triangle
    // First vertex
    out_vertices[vertex_count++] = vertices[working[prev].original_index].x;
    out_vertices[vertex_count++] = vertices[working[prev].original_index].y;
    out_vertices[vertex_count++] = 0.0f;  // Z
    out_vertices[vertex_count++] = 0.0f;  // U
    out_vertices[vertex_count++] = 0.0f;  // V
    
    // Second vertex
    out_vertices[vertex_count++] = vertices[working[ear_idx].original_index].x;
    out_vertices[vertex_count++] = vertices[working[ear_idx].original_index].y;
    out_vertices[vertex_count++] = 0.0f;  // Z
    out_vertices[vertex_count++] = 1.0f;  // U
    out_vertices[vertex_count++] = 0.0f;  // V
    
    // Third vertex
    out_vertices[vertex_count++] = vertices[working[next].original_index].x;
    out_vertices[vertex_count++] = vertices[working[next].original_index].y;
    out_vertices[vertex_count++] = 0.0f;  // Z
    out_vertices[vertex_count++] = 0.5f;  // U
    out_vertices[vertex_count++] = 1.0f;  // V
    
    // Remove the ear from the polygon
    working[ear_idx].original_index = -1;
    remaining--;
    
    // Update ear status for adjacent vertices
    if (remaining > 3) {
      update_ear_status(prev, working, num_vertices);
      update_ear_status(next, working, num_vertices);
    }
  }
  
  // Add the last triangle if we still have one
  if (remaining == 3) {
    int indices[3] = {-1, -1, -1};
    int count = 0;
    
    for (int i = 0; i < num_vertices && count < 3; i++) {
      if (working[i].original_index != -1) {
        indices[count++] = working[i].original_index;
      }
    }
    
    if (count == 3) {
      // First vertex
      out_vertices[vertex_count++] = vertices[indices[0]].x;
      out_vertices[vertex_count++] = vertices[indices[0]].y;
      out_vertices[vertex_count++] = 0.0f;  // Z
      out_vertices[vertex_count++] = 0.0f;  // U
      out_vertices[vertex_count++] = 0.0f;  // V
      
      // Second vertex
      out_vertices[vertex_count++] = vertices[indices[1]].x;
      out_vertices[vertex_count++] = vertices[indices[1]].y;
      out_vertices[vertex_count++] = 0.0f;  // Z
      out_vertices[vertex_count++] = 1.0f;  // U
      out_vertices[vertex_count++] = 0.0f;  // V
      
      // Third vertex
      out_vertices[vertex_count++] = vertices[indices[2]].x;
      out_vertices[vertex_count++] = vertices[indices[2]].y;
      out_vertices[vertex_count++] = 0.0f;  // Z
      out_vertices[vertex_count++] = 0.5f;  // U
      out_vertices[vertex_count++] = 1.0f;  // V
    }
  }
  
  // Clean up
  free(working);
  
  *out_count = vertex_count;
}
