// OpenEXR loader (encoder/basisu_tinyexr.cpp + tinyexr): basisu::read_exr on an in-memory file.
#include "encoder/basisu_enc.h"
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu::basisu_encoder_init(), true);
  (void)inited;
  basisu::imagef img;
  basisu::read_exr(data, size, img);
  return 0;
}
