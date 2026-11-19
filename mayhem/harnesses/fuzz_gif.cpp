// Fuzz the GIF decode path (lib/GifToBmpConverter over AnimatedGIF): the input is the GIF file,
// held in memory (mayhem/harnesses/stubs/memfile). Converts it to a 2-bit cover BMP (crop on/off,
// chosen by the first byte) and to a size-limited 1-bit BMP, both into a discarding Print.
#include <GifToBmpConverter.h>
#include <HalStorage.h>
#include <Print.h>

#include <cstddef>
#include <cstdint>

namespace {
class NullPrint final : public Print {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override { return n; }
};
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const uint8_t opts = data[0];
  const uint8_t* gif = data + 1;
  const size_t len = size - 1;

  {
    HalFile file(gif, len);
    NullPrint out;
    GifToBmpConverter::gifFileToBmpStream(file, out, opts & 1, (opts >> 1) & 1);
  }
  {
    HalFile file(gif, len);
    NullPrint out;
    // Thumbnail-style target box; dimensions derived from the options byte.
    const int maxW = 16 + (opts >> 2) * 4;
    const int maxH = 16 + (opts >> 4) * 8;
    GifToBmpConverter::gifFileTo1BitBmpStreamWithSize(file, out, maxW, maxH);
  }
  return 0;
}
