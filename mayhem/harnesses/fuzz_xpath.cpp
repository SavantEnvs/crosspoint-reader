// Fuzz the KOReader-sync chapter XPath resolver (lib/KOReaderSync/ChapterXPathResolver.cpp).
// Input = 8 header bytes (paragraph index u16, visible-text offset u32, progress u16 / 65535)
// followed by the chapter XHTML, which is streamed through the stubbed Epub exactly as the
// firmware streams a spine item. All three public resolvers are exercised on the chapter.
#include <ChapterXPathResolver.h>
#include <Epub.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 8) return 0;
  uint16_t paragraph, progress16;
  uint32_t offset;
  std::memcpy(&paragraph, data, 2);
  std::memcpy(&offset, data + 2, 4);
  std::memcpy(&progress16, data + 6, 2);
  std::vector<std::string> spine;
  spine.emplace_back(reinterpret_cast<const char*>(data + 8), size - 8);
  const auto epub = std::make_shared<Epub>(std::move(spine));

  (void)ChapterXPathResolver::findXPathForParagraph(epub, 0, paragraph);
  (void)ChapterXPathResolver::findXPathForParagraph(epub, 0, static_cast<uint16_t>(paragraph % 8));
  (void)ChapterXPathResolver::findXPathForVisibleTextOffset(epub, 0, offset);
  (void)ChapterXPathResolver::findXPathForVisibleTextOffset(epub, 0, offset % 512);
  (void)ChapterXPathResolver::findXPathForProgress(epub, 0, static_cast<float>(progress16) / 65535.0f);
  (void)ChapterXPathResolver::findXPathForProgress(epub, 0, 1.0f);
  (void)ChapterXPathResolver::findXPathForProgress(epub, 0, 0.5f);
  return 0;
}
