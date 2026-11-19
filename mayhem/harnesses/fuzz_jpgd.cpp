// jpgd (encoder/jpgd.cpp): baseline/progressive JPEG decoder via its public in-memory API.
#include "encoder/jpgd.h"
#include <cstdint>
#include <cstdlib>
#include <climits>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1 || size > INT_MAX) return -1;
  const int req = (data[0] & 1) ? 4 : 1;
  data++; size--;
  int w = 0, h = 0, comps = 0;
  unsigned char* p = jpgd::decompress_jpeg_image_from_memory(data, (int)size, &w, &h, &comps, req);
  free(p);
  return 0;
}
