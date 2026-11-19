// Fuzz the .cpfont SD-card font reader (lib/EpdFont/SdCardFont.cpp): the whole input is the font
// file on the SD card. Loads it the way the reader does, prewarms a page of text (glyph metrics,
// bitmaps, kerning classes, ligature pairs), builds the layout advance table, then measures and
// looks up glyphs/kerning/ligatures per style through the EpdFont interface the renderer uses.
// Storage is upstream's memory-backed host stub (test/sd_card_font/stubs/HalStorage.h), whose
// globals are reset per input.
#include <HalStorage.h>
#include <SdCardFont.h>

#include <cstddef>
#include <cstdint>

static const char kText[] =
    "AV fi ffl To WA Ta \xEA\xB0\x80\xEA\xB0\x81\xEB\x82\x98 ABCDEF EF \xC3\xA9\xC3\xB1 "
    "\xEF\xBF\xBD \xE4\xB8\xAD\xE6\x96\x87 \xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 0123456789";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // reserve(): the stub memcpy()s from data(), which must not be null even for an empty file.
  sdFontTestFile.reserve(size + 1);
  sdFontTestFile.assign(data, data + size);
  sdFontTestReads = 0;
  SdCardFont font;
  if (!font.load("/fuzz.cpfont")) return 0;
  font.prewarm(kText, 0x0F);
  font.buildAdvanceTable(kText, 0x0F, "\xEF\xAC\x81");
  for (uint8_t style = 0; style < 4; ++style) {
    (void)font.getAdvance('A', style);
    EpdFont* f = font.getEpdFont(style);
    if (!f) continue;
    int w = 0, h = 0;
    f->getTextDimensions(kText, &w, &h);
    const uint32_t cps[] = {'A', 'V', 'f', 'i', 'l', 'E', 'F', 0xAC00, 0xAC01, 0xFFFD, 0x4E2D, 0x0633};
    for (uint32_t a : cps) {
      const EpdGlyph* g = f->getGlyph(a);
      if (g && font.isOverflowGlyph(g)) (void)font.getOverflowBitmap(g);
      (void)f->hasCodepoint(a);
      for (uint32_t b : cps) {
        (void)f->getKerning(a, b);
        (void)f->getLigature(a, b);
      }
    }
  }
  // Second page with metadata only, then a cache drop and a re-prewarm (reader page turn).
  font.clearCache();
  font.prewarm("Hello, World! \xEA\xB0\x80", 0x0F, true);
  font.releaseResidentCaches();
  font.prewarm(kText, 0x01, false, true, false);
  if (EpdFont* f = font.getEpdFont(0)) {
    int w = 0, h = 0;
    f->getTextDimensions("AVAV fi", &w, &h);
  }
  (void)SdCardFont::takeCorruptGlyphNotice();
  return 0;
}
