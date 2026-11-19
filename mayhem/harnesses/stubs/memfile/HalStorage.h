#pragma once
// Memory-backed HalFile for the BMP and GIF converter harnesses (upstream's tests use a
// std::FILE-backed HalFile in test/chapter_html_slim_parser/stubs; this has the same interface
// subset the converters/Bitmap/GifCommon call, but reads from a caller-owned byte range so no
// file is touched). Write calls are accepted and discarded.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

class HalFile {
 public:
  HalFile() = default;
  HalFile(const uint8_t* bytes, size_t length) : data_(bytes), len_(length), open_(true) {}

  bool isOpen() const { return open_; }
  explicit operator bool() const { return open_; }
  bool close() {
    open_ = false;
    pos_ = 0;
    return true;
  }
  size_t size() const { return open_ ? len_ : 0; }
  size_t position() const { return pos_; }
  int available() const { return open_ ? static_cast<int>(len_ - pos_) : 0; }
  bool seek(size_t pos) {
    if (!open_ || pos > len_) return false;
    pos_ = pos;
    return true;
  }
  bool seekCur(int64_t offset) {
    const int64_t target = static_cast<int64_t>(pos_) + offset;
    if (target < 0) return false;
    return seek(static_cast<size_t>(target));
  }
  int read() {
    if (!open_ || pos_ >= len_) return -1;
    return data_[pos_++];
  }
  int read(void* dst, size_t count) {
    if (!open_) return -1;
    const size_t remaining = len_ - pos_;
    const size_t n = count < remaining ? count : remaining;
    if (n > 0) std::memcpy(dst, data_ + pos_, n);
    pos_ += n;
    return static_cast<int>(n);
  }
  size_t write(const void*, size_t count) { return count; }
  size_t write(uint8_t) { return 1; }
  bool flush() { return true; }

 private:
  const uint8_t* data_ = nullptr;
  size_t len_ = 0;
  size_t pos_ = 0;
  bool open_ = false;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
};

#define Storage HalStorage::getInstance()

// GifToBmpConverter yields with these (declared by the Arduino core on the device).
inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
