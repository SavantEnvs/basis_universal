// Built-in TGA reader (basisu::read_tga in encoder/basisu_enc.cpp): RLE, colormaps, 8/16/24/32 bpp.
#include "encoder/basisu_enc.h"
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > UINT32_MAX) return -1;
  int w = 0, h = 0, n = 0;
  uint8_t* p = basisu::read_tga(data, (uint32_t)size, w, h, n);
  free(p);
  return 0;
}
