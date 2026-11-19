// Radiance .hdr reader (basisu::read_rgbe in encoder/basisu_enc.cpp): header text + RLE scanlines.
#include "encoder/basisu_enc.h"
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  basisu::uint8_vec v;
  v.resize(size);
  if (size) memcpy(v.data(), data, size);
  basisu::imagef img;
  basisu::rgbe_header_info hdr;
  basisu::read_rgbe(v, img, hdr);
  return 0;
}
