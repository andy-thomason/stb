

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <fstream>
#include <vector>
#include <iostream>


// This is an example of a box filter. It generates increasingly "blurry" images due to
// aliasing. It is very simple however.

// A general resample to any submultiple of w, h (eg. w/2, h/4)
// Note that "stride" is the number of bytes per line which may not be w*3 or new_w*3
// because of alignment requirements.
stbi_uc *resampleIntegerRGB(stbi_uc *rgb_in, int w, int h, int stride, int new_w, int new_h, int new_stride) {
  stbi_uc *result = (stbi_uc*)stbi__malloc(new_h * new_stride);
  int x_ratio = w / new_w;
  int y_ratio = h / new_h;
  int area_ratio = x_ratio * y_ratio;
  
  for (int y = 0; y != new_h; ++y) {
    for (int x = 0; x != new_w; ++x) {
      // take the average of the pixels in the NxM box
      int r = 0, g = 0, b = 0;
      for (int j = 0; j != x_ratio; ++j) {
        for (int i = 0; i != y_ratio; ++i) {
          r += rgb_in[0 + (x*x_ratio+i)*3 + (y*y_ratio+j)*stride];
          g += rgb_in[1 + (x*x_ratio+i)*3 + (y*y_ratio+j)*stride];
          b += rgb_in[2 + (x*x_ratio+i)*3 + (y*y_ratio+j)*stride];
        }
      }
      result[0 + x*3 + y*new_stride] = stbi_uc(r / area_ratio);
      result[1 + x*3 + y*new_stride] = stbi_uc(g / area_ratio);
      result[2 + x*3 + y*new_stride] = stbi_uc(b / area_ratio);
    }
  }
  return result;
}

// To make it more efficient, we can use constant template parameters and a simd-like buffer
// The compiler (in -O3) will convert the inner loops to SIMD
template<int x_ratio, int y_ratio>
stbi_uc *resampleConstRGB(stbi_uc *rgb_in, int w, int h, int stride, int new_stride) {
  int new_w = w / x_ratio;
  int new_h = h / y_ratio;
  stbi_uc *result = (stbi_uc*)stbi__malloc(new_h * new_stride);
  constexpr uint32_t area_ratio = x_ratio * y_ratio;
  int row_len = new_w * x_ratio * 3;
  std::vector<uint32_t> row(row_len);
  
  for (int y = 0; y != new_h; ++y) {
    for (int k = 0; k != row_len; ++k) {
      row[k] = rgb_in[k + (y*x_ratio+0)*stride];
    }
    for (int j = 1; j != y_ratio; ++j) {
      for (int k = 0; k != row_len; ++k) {
        row[k] += rgb_in[k + (y*x_ratio+j)*stride];
      }
    }
    for (int x = 0; x != new_w; ++x) {
      for (int comp = 0; comp != 3; ++comp) {
        uint32_t r = row[x * (x_ratio * 3) + comp];
        for (int i = 1; i != x_ratio; ++i) {
          r += row[x * (x_ratio * 3) + i * 3 + comp];
        }
        result[comp + x*3 + y*new_stride] = stbi_uc(r / area_ratio);
      }
    }
  }
  return result;
}


int main() {
  std::ifstream is("16-million-atoms.jpg", std::ios::binary|std::ios::ate);
  std::vector<char> bytes(is.tellg());
  is.seekg(0);
  is.read(bytes.data(), bytes.size());
  int w = 0, h = 0, comp = 0;
  stbi_uc *rgb = stbi_load_from_memory((stbi_uc const *)bytes.data(), int(bytes.size()), &w, &h, &comp, 3);

  // Simples option: resample into a smaller buffer

  printf("%dx%d\n", w, h);

  for (int i = 1; i <= 8; ++i) {
    int new_w = w / i;
    int new_h = h / i;
    int new_stride = (new_w * 3 + 3) & ~3; // round up to multiple of four bytes.
    stbi_uc *new_rgb = (stbi_uc*)resampleIntegerRGB(rgb, w, h, w*3, new_w, new_h, new_stride);
    char buf[128];
    snprintf(buf, sizeof(buf), "var%dx%d.png", i, i);
    printf("%s\n", buf);
    int res = stbi_write_png(buf, new_w, new_h, 3, new_rgb, new_stride);
    STBI_FREE(new_rgb);
  }

  {
    int new_stride = (w/2*3+3) & ~3;
    stbi_uc *new_rgb = resampleConstRGB<2, 2>(rgb, w, h, w*3, new_stride);
    int res = stbi_write_png("const2x2.png", w/2, h/2, 3, new_rgb, new_stride);
    STBI_FREE(new_rgb);
  }

  STBI_FREE(rgb);
  
}


