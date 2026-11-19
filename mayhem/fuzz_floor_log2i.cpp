// floor-log2i: ported legacy target (mayhemheroes), widened to a real input surface.
//
// The legacy harness only called basisu::helpers::floor_log2i(uint32) (~10 edges). It still does,
// on the input's 4-byte header, but the target now fuzzes the GPU block-texture decoders behind
// basisu::gpu_image::unpack() / unpack_hdr(): BC1/BC1_NV/BC1_AMD/BC3/BC4/BC5/BC7, BC6H, ETC1/ETC2/EAC
// (R11, RG11, alpha), PVRTC1, PVRTC2, ATC, FXT1 and UASTC LDR 4x4. The basisu tool
// runs these on block data (pre-filled with random bytes, then transcoded) under -unpack/-validate,
// and any app loading compressed GPU texture data hands them untrusted blocks.
//
// Input: byte 0 selects the format, bytes 1-2 the pixel width/height (1..64), byte 3 is unused
// (all 4 header bytes feed floor_log2i); the rest is the raw block data. An input with too few bytes for its block grid is rejected.
//
// Not included: the ASTC formats (LDR block sizes, ASTC/UASTC HDR 4x4, ASTC HDR 6x6). unpack_block()
// and unpack_block_hdr() assert that the Android decoder accepted the block (basisu_gpu_texture.cpp,
// `assert(status)` after decompress_ldr/decompress_hdr) and the header documents that unpack_block()
// "is primarily intended to unpack texture data created by the transcoder", so an arbitrary ASTC
// block trips that assert on nearly every input. ASTC block decoding is fuzzed by the `astc` target.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <climits>
#include <cstdarg>  // basisu_containers.h uses va_start without including it
#include <string>

#include "encoder/basisu_enc.h"  // includes basisu_containers.h (helpers::floor_log2i)
#include "encoder/basisu_gpu_texture.h"

using basisu::texture_format;

static const texture_format kFormats[] = {
  texture_format::cETC1, texture_format::cETC1S, texture_format::cETC2_RGB,
  texture_format::cETC2_RGBA, texture_format::cETC2_ALPHA, texture_format::cBC1,
  texture_format::cBC3, texture_format::cBC4, texture_format::cBC5, texture_format::cBC7,
  texture_format::cPVRTC1_4_RGB, texture_format::cPVRTC1_4_RGBA, texture_format::cATC_RGB,
  texture_format::cATC_RGBA_INTERPOLATED_ALPHA, texture_format::cFXT1_RGB,
  texture_format::cPVRTC2_4_RGBA, texture_format::cETC2_R11_EAC, texture_format::cETC2_RG11_EAC,
  texture_format::cUASTC4x4, texture_format::cBC1_NV, texture_format::cBC1_AMD,
  // HDR: decoded through unpack_hdr()
  texture_format::cBC6HSigned, texture_format::cBC6HUnsigned,
};

static uint32_t next_pow2(uint32_t v) {
  uint32_t p = 1;
  while (p < v) p <<= 1;
  return p;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static bool inited = (basisu::basisu_encoder_init(), true);
  (void)inited;
  if (size < 4) return -1;

  uint32_t hdr;
  memcpy(&hdr, data, 4);
  basisu::helpers::floor_log2i(hdr);  // the legacy target's call

  const texture_format fmt = kFormats[data[0] % (sizeof(kFormats) / sizeof(kFormats[0]))];
  uint32_t w = (data[1] % 64) + 1, h = (data[2] % 64) + 1;
  // PVRTC1 textures are power-of-2 sized (its blocks are stored in Morton order over a 2^n grid);
  // the basisu tool never builds a non-power-of-2 PVRTC1 image.
  if (fmt == texture_format::cPVRTC1_4_RGB || fmt == texture_format::cPVRTC1_4_RGBA) {
    w = next_pow2(w);
    h = next_pow2(h);
  }

  basisu::gpu_image gi(fmt, w, h);
  const size_t need = gi.get_size_in_bytes();
  if (size - 4 < need) return -1;
  memcpy(gi.get_ptr(), data + 4, need);

  if (basisu::is_hdr_texture_format(fmt)) {
    basisu::imagef img;
    gi.unpack_hdr(img);
  } else {
    basisu::image img;
    gi.unpack(img, false);  // the sRGB flag only affects ASTC
  }
  return 0;
}
