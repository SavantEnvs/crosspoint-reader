#pragma once
// The ContentOpfParser only calls createSpineEntry(); the fuzz stub counts calls and keeps hrefs.
#include <string>
#include <vector>

class BookMetadataCache {
 public:
  std::vector<std::string> spineHrefs;
  void createSpineEntry(const std::string& href) { spineHrefs.push_back(href); }
};
