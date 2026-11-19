// Fuzz the chapter XHTML -> pages pipeline (lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp), driven the
// way Section::createSectionFile does it: the chapter is a file in storage, a ChapterHtmlSlimParser is built
// with a page-complete callback and run to completion (parseAndBuildPages), with embedded CSS enabled.
// Input format: if the input contains a NUL byte, the bytes before the first NUL are an external stylesheet
// (loaded through CssParser::loadFromStream) and the bytes after it are the XHTML; otherwise the whole input is
// the XHTML. Every emitted page is serialized to storage and read back (Page::serialize/deserialize).
// Storage is memory-backed (mayhem/harnesses/stubs/mem), reset per input; fixture config as upstream's
// test/chapter_html_slim_parser (stub GfxRenderer with fixed glyph metrics, epub == nullptr, so no images).
#include <Epub.h>
#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#define class struct
#define private public
#include "Epub/parsers/ChapterHtmlSlimParser.h"
#undef private
#undef class

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  memStorageReset();
  const uint8_t* nul = size ? static_cast<const uint8_t*>(std::memchr(data, 0, size)) : nullptr;
  const uint8_t* css = data;
  size_t cssSize = 0;
  const uint8_t* html = data;
  size_t htmlSize = size;
  if (nul) {
    cssSize = static_cast<size_t>(nul - data);
    html = nul + 1;
    htmlSize = size - cssSize - 1;
  }
  const std::string filepath = "/.crosspoint/epub_1/tmp_0.html";
  memStoragePut(filepath, html, htmlSize);

  CssParser cssParser("/.crosspoint/epub_1/");
  if (nul) {
    memStoragePut("/style.css", css, cssSize);
    HalFile src;
    if (Storage.openFileForRead("CSS", "/style.css", src)) (void)cssParser.loadFromStream(src);
  }

  GfxRenderer renderer;
  size_t pages = 0;
  ChapterHtmlSlimParser parser(nullptr, filepath, renderer, 0, 1.0f, false, 0,
                               static_cast<uint16_t>(renderer.getScreenWidth()),
                               static_cast<uint16_t>(renderer.getScreenHeight()), false, false, {}, true, "", "", 0,
                               {}, nullptr, &cssParser);
  parser.currentTextBlock = std::make_unique<ParsedText>();
  parser.completePageFn = [&](std::unique_ptr<Page> page, uint16_t, uint16_t, uint32_t) {
    if (++pages > 64) {
      // A real section build stops being useful long before this; keep each exec fast.
      parser.requestStop();
      return;
    }
    HalFile out;
    if (Storage.openFileForWrite("PG", "/page.bin", out) && page->serialize(out)) {
      out.close();
      HalFile in;
      if (Storage.openFileForRead("PG", "/page.bin", in)) (void)Page::deserialize(in);
    }
  };
  (void)parser.parseAndBuildPages();
  return 0;
}
