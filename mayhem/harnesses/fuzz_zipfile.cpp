// Fuzz the EPUB zip container reader (lib/ZipFile/ZipFile.cpp + miniz InflateStream): the whole
// input is the archive on the SD card. Opens it the way the reader does, preloads the slim stat
// table, enumerates central-directory entries, batch-resolves sizes, then inflates each entry to
// memory and to a streaming sink. Storage is upstream's memory-backed host stub, reset per input.
#include <HalStorage.h>
#include <ZipFile.h>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

namespace {
// Output sink that discards everything (the firmware streams entries to SD / a parser).
class NullSink : public Print {
 public:
  size_t total = 0;
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override {
    total += n;
    return n;
  }
};

// Entries claiming more than this are not extracted (keeps each exec fast and memory bounded).
constexpr size_t kMaxInflate = 1u << 20;
constexpr size_t kMaxEntries = 32;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string path = "/fuzz.epub";
  std::vector<uint8_t> bytes;
  bytes.reserve(size + 1);
  bytes.assign(data, data + size);
  Storage.setFile(path, std::move(bytes));

  ZipFile zip(path);
  if (!zip.open()) return 0;

  std::vector<std::string> names;
  zip.enumerateFileEntries([&](std::string_view p, uint32_t, uint32_t) {
    if (names.size() < kMaxEntries) names.emplace_back(p);
  });
  zip.close();

  ZipFile zip2(path);
  if (!zip2.loadAllFileStatSlims()) {
    // Unloaded: lookups scan the central directory instead.
  }
  std::deque<ZipFile::SizeTarget> targets;
  std::deque<uint32_t> sizes;
  uint16_t idx = 0;
  for (const auto& n : names) {
    targets.push_back({ZipFile::fnvHash64(n.data(), n.size()), static_cast<uint16_t>(n.size()), idx++});
  }
  std::sort(targets.begin(), targets.end(), [](const ZipFile::SizeTarget& a, const ZipFile::SizeTarget& b) {
    return a.hash != b.hash ? a.hash < b.hash : a.len < b.len;
  });
  sizes.assign(names.size(), 0);
  if (!targets.empty()) zip2.fillUncompressedSizes(targets, sizes);

  names.push_back("mimetype");
  names.push_back("META-INF/container.xml");
  for (const auto& n : names) {
    size_t inflated = 0;
    if (!zip2.getInflatedFileSize(n.c_str(), &inflated)) continue;
    if (inflated > kMaxInflate) continue;
    size_t outSize = 0;
    uint8_t* mem = zip2.readFileToMemory(n.c_str(), &outSize, true);
    if (mem) free(mem);
    NullSink sink;
    zip2.readFileToStream(n.c_str(), sink, 256);
    NullSink probe;
    zip2.readFileToStream(n.c_str(), probe, 64, true);
  }
  zip2.close();
  return 0;
}
