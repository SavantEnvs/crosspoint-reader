#pragma once
// TocNcxParser / TocNavParser only call createTocEntry(); the fuzz stub records the entries.
#include <cstdint>
#include <string>
#include <vector>

class BookMetadataCache {
 public:
  struct Entry {
    std::string title, href, anchor;
    uint8_t level;
  };
  std::vector<Entry> toc;
  void createTocEntry(const std::string& title, const std::string& href, const std::string& anchor, uint8_t level) {
    toc.push_back({title, href, anchor, level});
  }
};
