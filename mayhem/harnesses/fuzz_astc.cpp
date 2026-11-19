// .astc file reader (basisu::read_astc_file) + ASTC physical->logical unpack and LDR/HDR block decode
// (astc_helpers::unpack_block / decode_block) for every block in the file.
#include "encoder/basisu_enc.h"
#include "encoder/basisu_gpu_texture.h"
#include "transcoder/basisu_astc_helpers.h"
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu::basisu_encoder_init(), true);
  (void)inited;
  basisu::vector2D<astc_helpers::astc_block> blocks;
  uint32_t bw = 0, bh = 0, w = 0, h = 0;
  if (!basisu::read_astc_file(data, size, blocks, bw, bh, w, h)) return 0;
  uint32_t px[12 * 12 * 4];
  uint32_t done = 0;
  for (uint32_t y = 0; y < blocks.get_height(); y++)
    for (uint32_t x = 0; x < blocks.get_width(); x++) {
      if (++done > 4096) return 0;
      astc_helpers::log_astc_block lb;
      if (!astc_helpers::unpack_block(&blocks(x, y), lb, bw, bh)) continue;
      astc_helpers::decode_block(lb, px, bw, bh, astc_helpers::cDecodeModeHDR16);
      astc_helpers::decode_block(lb, px, bw, bh, astc_helpers::cDecodeModeRGB9E5);
      if (!lb.m_solid_color_flag_hdr)
        astc_helpers::decode_block(lb, px, bw, bh, astc_helpers::cDecodeModeLDR8);
    }
  return 0;
}
