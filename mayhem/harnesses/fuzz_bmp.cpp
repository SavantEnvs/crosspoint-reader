// Fuzz the BMP readers: lib/BmpToBmpConverter (cover/sleep-image conversion) and
// lib/GfxRenderer/Bitmap.cpp (header + row parsing as GfxRenderer::drawBitmap drives it).
// Input byte 0 selects options (bit0 crop, bit1 originalThresholds, bit2 dithering for the direct
// Bitmap path); the rest is the BMP file, held in memory (mayhem/harnesses/stubs/memfile).
#include <Bitmap.h>
#include <BmpToBmpConverter.h>
#include <HalStorage.h>
#include <Print.h>

#include <cstddef>
#include <cstdint>
#include <vector>

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
  const uint8_t* bmp = data + 1;
  const size_t bmpLen = size - 1;

  {
    HalFile file(bmp, bmpLen);
    NullPrint out;
    BmpToBmpConverter::bmpFileToBmpStream(file, out, opts & 1, (opts >> 1) & 1);
  }

  // The renderer's path: parse headers, then walk the rows with outputRow/rowBytes scratch buffers.
  {
    HalFile file(bmp, bmpLen);
    Bitmap bitmap(file, (opts >> 2) & 1, (opts >> 1) & 1);
    if (bitmap.parseHeaders() != BmpReaderError::Ok) return 0;
    const int width = bitmap.getWidth();
    const int height = bitmap.getHeight();
    const size_t outputRowSize = (static_cast<size_t>(width) + 3) / 4;
    const size_t rowBytesSize = static_cast<size_t>(bitmap.getRowBytes());
    std::vector<uint8_t> scratch(outputRowSize + rowBytesSize);
    uint8_t* outputRow = scratch.data();
    uint8_t* rowBytes = scratch.data() + outputRowSize;
    (void)bitmap.hasGreyscale();
    (void)bitmap.isTopDown();
    (void)bitmap.is1Bit();
    (void)bitmap.getBpp();

    const int rows = height < 64 ? height : 64;
    for (int y = 0; y < rows; y++) {
      if (bitmap.readNextRow(outputRow, rowBytes) != BmpReaderError::Ok) break;
    }
    if (bitmap.rewindToData() == BmpReaderError::Ok) {
      for (int y = 0; y < rows; y++) {
        if (bitmap.readNextRow(outputRow, rowBytes) != BmpReaderError::Ok) break;
      }
    }
    std::vector<uint8_t> gray(static_cast<size_t>(width));
    for (int y = 0; y < rows; y++) {
      const int yy = (opts & 8) ? height - 1 - y : y;
      if (bitmap.readRowGray(yy, gray.data(), rowBytes) != BmpReaderError::Ok) break;
    }
  }
  return 0;
}
