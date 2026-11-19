// Fuzz the EPUB CSS parser (lib/Epub/Epub/css/CssParser.cpp): the whole input is a stylesheet.
// Loads it through loadFromStream (as Epub/Section do for each stylesheet), resolves styles for a
// set of tag/class/id/ancestor combinations the HTML parser asks for, parses it as an inline
// style attribute, then round-trips the binary rule cache (save + load) and resolves again.
// Storage is a memory-backed stub (mayhem/harnesses/stubs/mem), reset per input.
#include <CssParser.h>
#include <HalStorage.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

static void resolveSome(const CssParser& p) {
  static const char* const kTags[] = {"p", "div", "span", "h1", "body", "a", "li", "blockquote", "table", "td", "img", "em"};
  static const char* const kClasses[] = {"", "a", "note", "a b", "calibre1 calibre2 c3 c4 c5", "poem", "x y z"};
  const CssAncestor anc[] = {CssParser::makeAncestor("body", "", ""), CssParser::makeAncestor("div", "poem a", "main"),
                             CssParser::makeAncestor("blockquote", "note", "")};
  for (const char* t : kTags) {
    for (const char* c : kClasses) {
      (void)p.resolveStyle(t, c);
      (void)p.resolveStyle(t, c, "main", anc, 3);
      (void)p.resolveStyle(t, c, "", anc + 1, 2, p.hasFirstLetterRules());
    }
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  memStorageReset();
  memStoragePut("/fuzz.css", data, size);
  CssParser parser("/cache/");
  HalFile src;
  if (!Storage.openFileForRead("CSS", "/fuzz.css", src)) return 0;
  const auto res = parser.loadFromStream(src);
  src.close();
  (void)res;
  resolveSome(parser);
  (void)CssParser::parseInlineStyle(std::string_view(reinterpret_cast<const char*>(data), size));

  if (parser.saveToCache(res == CssParser::ParseResult::Complete)) {
    (void)parser.inspectCache();
    CssParser again("/cache/");
    if (again.loadFromCache() == CssParser::CacheLoadResult::Complete) {
      resolveSome(again);
      (void)again.ruleCount();
    }
  }
  parser.deleteCache();
  return 0;
}
