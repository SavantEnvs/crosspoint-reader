// Fuzz lib/PngToBmpConverter, the decoder for PNG cover images in EPUB/TXT books
// (Epub.cpp/Txt.cpp cover generation). Input byte 0 selects the entry point and options:
// bit0 -> pngFileToBmpStream (2-bit, display-sized) instead of pngFileTo1BitBmpStreamWithSize,
// bit1 crop, bit2 originalThresholds; bits 3-7 pick the 1-bit thumbnail size. The rest is the
// PNG file, held in memory (mayhem/harnesses/stubs/memfile).
#include <HalStorage.h>
#include <PngToBmpConverter.h>
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
  HalFile file(data + 1, size - 1);
  NullPrint out;
  if (opts & 1) {
    PngToBmpConverter::pngFileToBmpStream(file, out, (opts >> 1) & 1, (opts >> 2) & 1);
  } else {
    // Home-screen thumbnail sizes: 32..280 wide, 1.5x as tall.
    const int w = 32 + 8 * (opts >> 3);
    PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(file, out, w, w * 3 / 2);
  }
  return 0;
}
