// The source-image loaders the basisu tool uses by default (Wuffs PNG/JPEG/QOI, encoder/3rdparty).
// Byte 0 picks the loader; the rest is the image file.
#include "encoder/basisu_enc.h"
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu::basisu_encoder_init(), true);
  (void)inited;
  if (size < 1) return -1;
  const uint8_t sel = data[0] % 3;
  data++; size--;
  basisu::image img;
  switch (sel) {
    case 0: basisu::load_png(data, size, img); break;
    case 1: basisu::load_jpg(data, size, img); break;
    default: basisu::load_qoi(data, size, img); break;
  }
  return 0;
}
