#include "map.h"
#include <OpenGL/gl3.h>

// Assuming you have the 1-bit data in '1bit_image' and its dimensions are width, height
unsigned char *expand_to_8bit(unsigned char *data, int width, int height) {
  int size = (width * height + 7) / 8;  // Number of bytes for the 1-bit image
  unsigned char *expanded_data = malloc(width * height);  // The expanded 8-bit data
  
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int byte_index = (y * width + x) / 8;
      int bit_index = (y * width + x) % 8;
      unsigned char bit = (data[byte_index] >> (7 - bit_index)) & 1;
      expanded_data[y * width + x] = bit ? 0xFF : 0x00;  // 1-bit to 8-bit grayscale
    }
  }
  
  return expanded_data;
}

GLuint make_1bit_tex(void *data, int width, int height) {
  unsigned char *texture_data = expand_to_8bit(data, width, height);
  
  // Create the texture
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  // Set texture parameters (example)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  // Upload the image as an 8-bit grayscale texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, texture_data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

  // Free the allocated memory after uploading
  free(texture_data);
  
  return texture;
}
