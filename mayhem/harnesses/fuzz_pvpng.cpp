// pv_png (encoder/pvpngreader.cpp): the built-in PNG chunk parser + miniz inflate, via its
// public in-memory API (get_png_info / load_png).
#include <cstddef>
#include <cstdint>
#include "encoder/pvpngreader.h"
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return -1;
  const uint32_t desired = data[0] % 5;  // 0 = native, 1..4 channels
  data++; size--;
  pv_png::png_info info;
  if (!pv_png::get_png_info(data, size, info)) return 0;
  uint32_t w = 0, h = 0, n = 0;
  void* p = pv_png::load_png(data, size, desired, w, h, n);
  free(p);
  return 0;
}
