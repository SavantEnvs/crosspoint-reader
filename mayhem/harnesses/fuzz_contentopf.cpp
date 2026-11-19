// Fuzz the EPUB package document parser (ContentOpfParser) and the encryption.xml probe.
// First byte: bit0 = metadataOnly (Epub::readBookMetadata path vs full load with a spine cache),
// bit1 = also run the EncryptionManifestProbe on the rest; bits 2.. = chunk size selector. The
// remaining bytes are the XML, fed through setup()/write() in chunks like Epub::parseContentOpf
// (1024-byte zip chunks), then every getter is read. Storage is memory-backed and reset per input.
#include <ContentOpfParser.h>
#include <EncryptionManifestProbe.h>
#include <Epub/BookMetadataCache.h>
#include <HalStorage.h>

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  memStorageReset();
  const uint8_t mode = data[0];
  const uint8_t* xml = data + 1;
  const size_t xmlSize = size - 1;
  const bool metadataOnly = mode & 1;
  static const size_t kChunks[] = {1024, 1, 7, 64, 4096, 3, 256, 13};
  const size_t chunk = kChunks[(mode >> 2) & 7];

  {
    const std::string cachePath = "/.crosspoint/epub_1";
    const std::string basePath = "OEBPS/";
    BookMetadataCache cache;
    ContentOpfParser parser(cachePath, basePath, xmlSize, metadataOnly ? nullptr : &cache, metadataOnly);
    if (parser.setup()) {
      for (size_t off = 0; off < xmlSize;) {
        const size_t n = xmlSize - off < chunk ? xmlSize - off : chunk;
        if (parser.write(xml + off, n) != n) break;
        off += n;
      }
      (void)parser.title.size();
      (void)parser.author.size();
      (void)parser.language.size();
      (void)parser.isbn.size();
      (void)parser.asin.size();
      (void)parser.series.size();
      (void)parser.seriesIndex.has_value();
      (void)parser.tocNcxPath.size();
      (void)parser.tocNavPath.size();
      (void)parser.coverItemHref.size();
      (void)parser.guideCoverPageHref.size();
      (void)parser.textReferenceHref.size();
      (void)parser.cssFiles.size();
      (void)cache.spineHrefs.size();
    }
  }

  if (mode & 2) {
    EncryptionManifestProbe probe;
    if (probe.setup()) {
      for (size_t off = 0; off < xmlSize;) {
        const size_t n = xmlSize - off < chunk ? xmlSize - off : chunk;
        if (probe.write(xml + off, n) != n) break;
        off += n;
      }
      (void)probe.finish();
      (void)probe.needsProtectionCheck();
    }
  }
  return 0;
}
