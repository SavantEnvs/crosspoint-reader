// Fuzz the XTC/XTCH comic container parser (lib/Xtc/Xtc/XtcParser.cpp): the whole input is the
// file on the SD card. Opens it, reads page info for the first pages, loads each page into a
// buffer sized from the page info the way Xtc.cpp does (and into a too-small fixed one), streams
// pages in chunks, and lazily loads the chapter list. Storage is upstream's memory-backed stub,
// replaced on every input.
#include <HalStorage.h>
#include <XtcParser.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace xtc;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> bytes;
  bytes.reserve(size + 1);
  bytes.assign(data, data + size);
  Storage.setFile("/fuzz.xtc", std::move(bytes));

  XtcParser parser;
  if (parser.open("/fuzz.xtc") != XtcError::OK) return 0;

  (void)parser.getTitle();
  (void)parser.getAuthor();
  (void)parser.getWidth();
  (void)parser.getHeight();

  const uint32_t pages = std::min<uint32_t>(parser.getPageCount(), 6);
  // One past the end exercises the out-of-range path.
  for (uint32_t i = 0; i <= pages; i++) {
    PageInfo info{};
    if (!parser.getPageInfo(i, info)) continue;

    // Same sizing as Xtc::generateCoverBmp / generateThumbBmp.
    size_t bitmapSize;
    if (info.bitDepth == 2) {
      bitmapSize = static_cast<size_t>(info.width) * ((static_cast<size_t>(info.height) + 7) / 8) * 2;
    } else {
      bitmapSize = ((static_cast<size_t>(info.width) + 7) / 8) * info.height;
    }
    if (bitmapSize <= (1u << 21)) {
      std::vector<uint8_t> buf(bitmapSize);
      parser.loadPage(i, buf.data(), buf.size());
    }
    uint8_t small[256];
    parser.loadPage(i, small, sizeof(small));

    size_t total = 0;
    parser.loadPageStreaming(
        i, [&](const uint8_t* d, size_t n, size_t off) { total += n + (d ? 0 : off); }, 1024);
    parser.loadPageStreaming(
        i, [&](const uint8_t* d, size_t n, size_t off) { total += n + (d ? 0 : off); }, 7);
  }

  if (parser.hasChapters()) {
    for (const auto& c : parser.getChapters()) {
      (void)c.name.size();
      (void)c.startPage;
      (void)c.endPage;
    }
  }
  parser.close();
  return 0;
}
