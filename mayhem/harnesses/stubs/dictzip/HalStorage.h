#pragma once
// Memory-backed HalStorage/HalFile for fuzzing src/util/DictZip.cpp: the subset of HalFile that
// DictZip uses. One file path is registered per input; HalFile::write() output is discarded
// (counted). Reset with HalStorage::setFile() at the start of every input.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class HalFile {
  friend class HalStorage;

 public:
  size_t size() { return data ? data->size() : 0; }
  size_t fileSize() { return size(); }
  size_t position() const { return pos; }
  bool seekSet(size_t offset) {
    if (!data || offset > data->size()) return false;
    pos = offset;
    return true;
  }
  int read(void* dst, size_t len) {
    if (!data) return -1;
    const size_t remaining = data->size() - pos;
    const size_t count = len < remaining ? len : remaining;
    if (count > 0) std::memcpy(dst, data->data() + pos, count);
    pos += count;
    return static_cast<int>(count);
  }
  int read() {
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
  }
  size_t write(const uint8_t*, size_t count) {
    written += count;
    return count;
  }
  bool close() {
    data = nullptr;
    pos = 0;
    return true;
  }
  bool isOpen() const { return data != nullptr; }

  size_t written = 0;

 private:
  const std::vector<uint8_t>* data = nullptr;
  size_t pos = 0;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    if (path != filePath) return false;
    file.data = &fileData;
    file.pos = 0;
    return true;
  }
  void setFile(const std::string& path, std::vector<uint8_t> bytes) {
    filePath = path;
    fileData = std::move(bytes);
  }

 private:
  std::string filePath;
  std::vector<uint8_t> fileData;
};

#define Storage HalStorage::getInstance()
