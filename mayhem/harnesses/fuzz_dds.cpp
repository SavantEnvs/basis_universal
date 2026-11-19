// Uncompressed .dds reader (tinydds + basisu_gpu_texture.cpp). The library API takes a file name only,
// so each input is written to a private temp file under $TMPDIR, read, and unlinked.
#include "encoder/basisu_enc.h"
#include "encoder/basisu_gpu_texture.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static bool inited = (basisu::basisu_encoder_init(), true);
  (void)inited;
  const char* tmp = getenv("TMPDIR");
  std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/fuzz_dds_XXXXXX.dds";
  int fd = mkstemps(&path[0], 4);
  if (fd < 0) return 0;
  size_t off = 0;
  while (off < size) {
    ssize_t w = write(fd, data + off, size - off);
    if (w <= 0) break;
    off += (size_t)w;
  }
  close(fd);
  basisu::vector<basisu::image> ldr;
  basisu::vector<basisu::imagef> hdr;
  basisu::read_uncompressed_dds_file(path.c_str(), ldr, hdr);
  unlink(path.c_str());
  return 0;
}
