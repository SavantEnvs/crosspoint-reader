#pragma once
// Memory-backed HalFile/HalStorage for the fuzz harnesses: the API surface the chapter-HTML and CSS
// parsers use (a superset of upstream's test/{chapter_html_slim_parser,css_parser}/stubs/HalStorage.h),
// but files live in an in-process map instead of on the host filesystem. Harnesses call
// memStorageReset() at the start of every input so each result depends only on that input.
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr int O_WRITE = 1;
constexpr int O_CREAT = 2;
constexpr int O_TRUNC = 4;

namespace MemStorage {
using Buf = std::shared_ptr<std::vector<uint8_t>>;
inline std::map<std::string, Buf> files;
}  // namespace MemStorage

inline void memStorageReset() { MemStorage::files.clear(); }
inline void memStoragePut(const std::string& path, const uint8_t* data, size_t size) {
  auto b = std::make_shared<std::vector<uint8_t>>();
  if (size) b->assign(data, data + size);
  MemStorage::files[path] = b;
}

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(const char* path) { open(path, "wb"); }
  ~HalFile() { close(); }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  HalFile(HalFile&& o) noexcept : buf_(std::move(o.buf_)), pos_(o.pos_), writable_(o.writable_) { o.buf_.reset(); }
  HalFile& operator=(HalFile&& o) noexcept {
    buf_ = std::move(o.buf_);
    pos_ = o.pos_;
    writable_ = o.writable_;
    o.buf_.reset();
    return *this;
  }

  bool open(const char* path, const char* mode) {
    close();
    const bool w = mode[0] == 'w';
    auto it = MemStorage::files.find(path);
    if (w) {
      buf_ = std::make_shared<std::vector<uint8_t>>();
      MemStorage::files[path] = buf_;
    } else {
      if (it == MemStorage::files.end()) return false;
      buf_ = it->second;
    }
    writable_ = w;
    pos_ = 0;
    return true;
  }
  int available() const { return buf_ ? static_cast<int>(size() - pos_) : 0; }
  int read() { return buf_ && pos_ < buf_->size() ? (*buf_)[pos_++] : -1; }
  size_t read(void* buffer, size_t count) {
    if (!buf_ || pos_ >= buf_->size()) return 0;
    const size_t n = count < buf_->size() - pos_ ? count : buf_->size() - pos_;
    if (n) std::memcpy(buffer, buf_->data() + pos_, n);
    pos_ += n;
    return n;
  }
  size_t write(const void* buffer, size_t count) {
    if (!buf_ || !writable_) return 0;
    const size_t allowed = count < writeBytesLeft_ ? count : writeBytesLeft_;
    if (pos_ > buf_->size()) buf_->resize(pos_);
    const auto* p = static_cast<const uint8_t*>(buffer);
    if (allowed) buf_->insert(buf_->begin() + static_cast<long>(pos_), p, p + allowed);
    pos_ += allowed;
    writeBytesLeft_ -= allowed;
    return allowed;
  }
  size_t write(const uint8_t* buffer, size_t count) { return write(static_cast<const void*>(buffer), count); }
  size_t write(uint8_t byte) { return write(&byte, 1); }
  bool flush() { return buf_ != nullptr; }
  bool seek(size_t pos) {
    if (!buf_) return false;
    pos_ = pos;
    return true;
  }
  bool seekCur(size_t offset) { return seek(pos_ + offset); }
  bool close() {
    if (!buf_) return false;
    buf_.reset();
    return true;
  }
  bool isOpen() const { return buf_ != nullptr; }
  explicit operator bool() const { return isOpen(); }
  size_t position() const { return pos_; }
  size_t size() const { return buf_ ? buf_->size() : 0; }
  void limitWritesTo(size_t count) { writeBytesLeft_ = count; }

 private:
  MemStorage::Buf buf_;
  size_t pos_ = 0;
  bool writable_ = false;
  size_t writeBytesLeft_ = SIZE_MAX;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  int readOpens = 0;
  HalFile open(const char* path, int) { return HalFile(path); }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    readOpens++;
    return file.open(path, "rb");
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) { return openFileForRead("", path.c_str(), file); }
  bool openFileForWrite(const char*, const char* path, HalFile& file) { return file.open(path, "wb"); }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) { return file.open(path.c_str(), "wb"); }
  bool exists(const char* path) const { return MemStorage::files.count(path) != 0; }
  bool remove(const char* path) { return MemStorage::files.erase(path) != 0; }
  bool remove(const std::string& path) { return remove(path.c_str()); }
  bool rename(const char* from, const char* to) {
    auto it = MemStorage::files.find(from);
    if (it == MemStorage::files.end()) return false;
    MemStorage::files[to] = it->second;
    MemStorage::files.erase(from);
    return true;
  }
};

#define Storage HalStorage::getInstance()

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
