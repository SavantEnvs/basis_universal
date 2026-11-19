// .basis container + slice decoders, through the public basist::basisu_transcoder API.
// Byte 0 picks the output texture format; the rest is the .basis file. The whole-file CRC16
// (validate_file_checksums) is skipped, as the basisu tool's -fuzz_testing mode does.
#include "transcoder/basisu_transcoder.h"
#include <cstdint>
#include <vector>
using namespace basist;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu_transcoder_init(), true);
  (void)inited;
  if (size < 2) return -1;
  const transcoder_texture_format fmt =
      (transcoder_texture_format)(data[0] % (uint32_t)transcoder_texture_format::cTFTotalTextureFormats);
  data++; size--;
  if (size > UINT32_MAX) return -1;
  const uint32_t sz = (uint32_t)size;
  basisu_transcoder dec;
  if (!dec.validate_header(data, sz)) return 0;
  basisu_file_info fi;
  if (!dec.get_file_info(data, sz, fi)) return 0;
  if (!dec.start_transcoding(data, sz)) return 0;
  const uint32_t images = dec.get_total_images(data, sz);
  for (uint32_t i = 0; i < images && i < 16; i++) {
    basisu_image_info ii;
    if (!dec.get_image_info(data, sz, ii, i)) continue;
    for (uint32_t l = 0; l < ii.m_total_levels && l < 16; l++) {
      basisu_image_level_info li;
      if (!dec.get_image_level_info(data, sz, li, i, l)) continue;
      if ((uint64_t)li.m_orig_width * li.m_orig_height > 1024 * 1024) continue;
      if (!basis_is_format_supported(fmt, fi.m_tex_format)) continue;
      const uint32_t bpb = basis_get_bytes_per_block_or_pixel(fmt);
      const uint32_t n = basis_transcoder_format_is_uncompressed(fmt)
                             ? li.m_orig_width * li.m_orig_height
                             : li.m_total_blocks;
      std::vector<uint8_t> out((size_t)n * bpb + 64);
      dec.transcode_image_level(data, sz, i, l, out.data(), n, fmt);
    }
  }
  return 0;
}
