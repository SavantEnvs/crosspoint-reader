// Fuzz the CLX1 library index reader (lib/LibraryIndex/LibraryIndexFile.cpp): the whole input is
// the index file on the SD card (memory-backed upstream stub, test/library_index_file/stubs).
// Opens it both strictly and for reconciliation, then walks it the way the browser does: rows in
// every sort order, every record's name/author/title/source author/path/path hash, and a batched
// recent-row identity lookup.
#include <HalStorage.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "LibraryIndexFile.h"

namespace library {
// Defined in LibraryText.cpp upstream; the test links its own copy, as does this harness.
std::string joinLibraryPath(const std::string_view folder, const std::string_view name) {
  return std::string(folder) + "/" + std::string(name);
}
}  // namespace library

static void walk(library::LibraryIndexFile& index) {
  using library::SortOrder;
  const uint16_t count = index.bookCount();
  (void)index.ranksDegraded();
  (void)index.dedupDegraded();
  const SortOrder orders[] = {SortOrder::RecentAsc, SortOrder::RecentDesc, SortOrder::TitleAsc,
                              SortOrder::TitleDesc, SortOrder::AuthorAsc,  SortOrder::AuthorDesc};
  const uint16_t limit = std::min<uint16_t>(count, 64);
  for (SortOrder o : orders) {
    for (uint16_t row = 0; row <= limit; row++) (void)index.ordinalForRow(o, row);
  }
  std::vector<library::BookIdentity> ids;
  std::string s;
  for (uint16_t ord = 0; ord < count; ord++) {
    library::ClixRecord rec;
    if (!index.readRecord(ord, rec)) continue;
    (void)index.readName(rec, s);
    (void)index.readAuthor(rec, s);
    (void)index.readTitle(rec, s);
    (void)index.readSourceAuthor(rec, s);
    (void)index.readPath(rec, s);
    uint64_t h = 0;
    if (index.readPathHash(rec, h) && ids.size() < library::LibraryIndexFile::MAX_IDENTITY_LOOKUPS) {
      ids.push_back({h, (ord & 1) ? 0u : rec.fileSize});
    }
  }
  if (!ids.empty()) {
    uint16_t rows[library::LibraryIndexFile::MAX_IDENTITY_LOOKUPS];
    (void)index.recentRowsFor(ids.data(), ids.size(), rows);
  }
  (void)index.ioFailed();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Storage.setFile("/library.clx", std::vector<uint8_t>(data, data + size));
  {
    library::LibraryIndexFile index;
    if (index.open("/library.clx")) walk(index);
  }
  {
    library::LibraryIndexFile index;
    if (index.openForReconciliation("/library.clx")) walk(index);
  }
  Storage.clearFile();
  return 0;
}
