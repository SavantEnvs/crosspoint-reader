// Fuzz the EPUB container.xml / NCX / EPUB3 nav parsers. First byte selects the parser
// (0 = ContainerParser, 1 = TocNcxParser, 2 = TocNavParser, else = all three), the rest is the
// XML, streamed in chunks via setup()/write() the way Epub.cpp streams the zip entry.
#include <ContainerParser.h>
#include <Epub/BookMetadataCache.h>
#include <TocNavParser.h>
#include <TocNcxParser.h>

#include <cstddef>
#include <cstdint>
#include <string>

template <typename P>
static void feed(P& p, const uint8_t* xml, size_t size, size_t chunk) {
  if (!p.setup()) return;
  for (size_t off = 0; off < size;) {
    const size_t n = size - off < chunk ? size - off : chunk;
    if (p.write(xml + off, n) != n) break;
    off += n;
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const uint8_t sel = data[0] % 4;
  const uint8_t* xml = data + 1;
  const size_t n = size - 1;
  const size_t chunk = (data[0] >> 2) % 3 == 0 ? 1024 : ((data[0] >> 2) % 3 == 1 ? 5 : 512);
  const std::string base = "OEBPS/";
  if (sel == 0 || sel == 3) {
    ContainerParser p(n);
    feed(p, xml, n, chunk);
    (void)p.fullPath.size();
  }
  if (sel == 1 || sel == 3) {
    BookMetadataCache cache;
    TocNcxParser p(base, n, &cache);
    feed(p, xml, n, chunk);
    (void)cache.toc.size();
  }
  if (sel == 2 || sel == 3) {
    BookMetadataCache cache;
    TocNavParser p(base, n, &cache);
    feed(p, xml, n, chunk);
    (void)cache.toc.size();
  }
  return 0;
}
