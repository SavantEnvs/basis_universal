// Encoder frontend/backend (ETC1S, UASTC LDR/HDR 4x4, ASTC HDR 6x6, XUASTC LDR) via the public
// basisu::basis_compress2 API on a tiny fuzz-derived image, single-threaded.
// Header: [mode][quality][effort][flags][w][h] then pixel bytes (RGBA8, or 4 bytes -> 1 float channel set).
#include "encoder/basisu_comp.h"
#include <cstdint>

using namespace basisu;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu_encoder_init(), true);
  (void)inited;
  if (size < 6) return -1;
  static const basist::basis_tex_format modes[] = {
      basist::basis_tex_format::cETC1S,          basist::basis_tex_format::cUASTC_LDR_4x4,
      basist::basis_tex_format::cUASTC_HDR_4x4,  basist::basis_tex_format::cASTC_HDR_6x6,
      basist::basis_tex_format::cUASTC_HDR_6x6_INTERMEDIATE,
      basist::basis_tex_format::cXUASTC_LDR_4x4, basist::basis_tex_format::cXUASTC_LDR_6x6,
      basist::basis_tex_format::cXUASTC_LDR_8x8};
  const basist::basis_tex_format mode = modes[data[0] % (sizeof(modes) / sizeof(modes[0]))];
  const int quality = data[1] % 101;
  const int effort = data[2] % 3;  // keep encodes fast
  const uint8_t fl = data[3];
  const uint32_t w = 1 + data[4] % 24, h = 1 + data[5] % 24;
  data += 6; size -= 6;

  uint32_t flags = 0;
  if (fl & 1) flags |= cFlagKTX2;
  if (fl & 2) flags |= cFlagKTX2UASTCSuperCompression;
  if (fl & 4) flags |= cFlagSRGB;
  if (fl & 8) flags |= cFlagGenMipsClamp;
  if (fl & 16) flags |= cFlagYFlip;
  if (fl & 32) flags |= cFlagValidateOutput;
  flags |= (uint32_t)((fl >> 6) & 3) << cFlagXUASTCLDRSyntaxShift;

  const bool hdr = (mode == basist::basis_tex_format::cUASTC_HDR_4x4) ||
                   (mode == basist::basis_tex_format::cASTC_HDR_6x6) ||
                   (mode == basist::basis_tex_format::cUASTC_HDR_6x6_INTERMEDIATE);
  size_t out_size = 0;
  void* out = nullptr;
  size_t pos = 0;
  auto next = [&]() -> uint8_t { return size ? data[pos++ % size] : 0; };
  if (hdr) {
    basisu::vector<imagef> imgs(1);
    imgs[0].resize(w, h);
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++)
        for (uint32_t c = 0; c < 4; c++) {
          // Finite, non-negative HDR values in [0, 64): the encoder's documented input domain.
          const uint32_t v = (uint32_t)next() | ((uint32_t)next() << 8);
          imgs[0](x, y)[c] = (float)v * (64.0f / 65536.0f);
        }
    out = basis_compress2(mode, imgs, flags, quality, effort, &out_size);
  } else {
    basisu::vector<image> imgs(1);
    imgs[0].resize(w, h);
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++)
        for (uint32_t c = 0; c < 4; c++) imgs[0](x, y)[c] = next();
    out = basis_compress2(mode, imgs, flags, quality, effort, &out_size);
  }
  basis_free_data(out);
  return 0;
}
