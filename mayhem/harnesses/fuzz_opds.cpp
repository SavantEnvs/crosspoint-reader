// Fuzz the OPDS feed parser (freeink-sdk/libs/network/Opds/OpdsFeedParser), driven the way
// OpdsBookBrowserActivity::fetchFeed / OpdsClient do: the HTTP body is written to the Print-style
// parser in pieces, the parser is flushed, then entries, facets, titles, pagination and search
// URLs are read. The body's first byte picks OPDS 1.x Atom (expat) or OPDS 2.0 JSON, as in
// firmware. The first input byte is not part of the feed: bits 0-4 give the write size
// ((n)^2+1 bytes) and bits 5-7 the preferred UI language.
#include <OpdsFeedParser.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const unsigned sel = data[0];
  data++;
  size--;
  static const char* const kLangs[] = {"", "en", "fr", "de", "pt", "zh", "es", "ja"};
  const size_t chunk = static_cast<size_t>(sel & 31) * (sel & 31) + 1;

  OpdsFeedParser parser;
  parser.setPreferredLanguage(kLangs[sel >> 5]);
  for (size_t pos = 0; pos < size; pos += chunk) {
    const size_t n = (size - pos < chunk) ? size - pos : chunk;
    parser.write(data + pos, n);
  }
  parser.flush();

  size_t sink = 0;
  sink += parser.error();
  sink += parser.truncated();
  sink += parser.isOpds2();
  sink += parser.currentPage() + parser.pageCount();
  sink += parser.getFeedTitle().size() + parser.getSearchTemplate().size() + parser.getSearchDescriptionUrl().size();
  sink += parser.getNextPageUrl().size() + parser.getPrevPageUrl().size() + parser.getFirstPageUrl().size() +
          parser.getLastPageUrl().size() + parser.getShelfUrl().size() + parser.getWishlistUrl().size() +
          parser.getHistoryUrl().size();
  for (const OpdsEntry& e : parser.takeEntries()) {
    sink += e.title.size() + e.author.size() + e.href.size() + e.id.size() + e.heading.size() + e.detail.size() +
            e.selfHref.size() + e.description.size() + e.coverHref.size() + e.purchase + e.indirect + e.lcpLicense +
            static_cast<size_t>(e.type);
  }
  for (const OpdsEntry& e : parser.takeFacetEntries()) sink += e.title.size() + e.href.size() + e.heading.size();

  // The authenticated retry after a 401 reuses the instance: reset must give a clean parser.
  parser.reset();
  sink += parser.getFeedTitle().size();
  return sink == static_cast<size_t>(-1) ? 1 : 0;
}
