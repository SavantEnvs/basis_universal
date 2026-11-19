// KTX2 container + every transcoder block decoder, through the public basist::ktx2_transcoder API.
// Byte 0 picks the output texture format; the rest is the .ktx2 file.
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
  ktx2_transcoder t;
  if (!t.init(data, (uint32_t)size)) return 0;
  if (!t.start_transcoding()) return 0;
  const uint32_t layers = t.get_layers() ? t.get_layers() : 1;
  for (uint32_t l = 0; l < t.get_levels() && l < 16; l++)
    for (uint32_t ly = 0; ly < layers && ly < 8; ly++)
      for (uint32_t f = 0; f < t.get_faces(); f++) {
        ktx2_image_level_info li;
        if (!t.get_image_level_info(li, l, ly, f)) continue;
        // Bound the output allocation (this is the caller's buffer, not the parser's).
        if ((uint64_t)li.m_orig_width * li.m_orig_height > 1024 * 1024) continue;
        if (!basis_is_format_supported(fmt, t.get_basis_tex_format())) continue;
        const uint32_t bpb = basis_get_bytes_per_block_or_pixel(fmt);
        const uint32_t n = basis_transcoder_format_is_uncompressed(fmt)
                               ? li.m_orig_width * li.m_orig_height
                               : li.m_total_blocks;
        std::vector<uint8_t> out((size_t)n * bpb + 64);
        t.transcode_image_level(l, ly, f, out.data(), n, fmt);
      }
  return 0;
}
