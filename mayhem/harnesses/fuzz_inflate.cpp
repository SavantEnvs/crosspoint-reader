// Fuzz the deflate decoder the firmware uses (lib/InflateReader over uzlib tinflate) and the
// dictzip reader built on it (src/util/DictZip.cpp). The first input byte selects the mode:
//   0  one-shot   InflateReader::init(false) + setSource + a single read() into a fixed buffer
//   1  streaming  init(true) with a read callback feeding 2KB blocks, read() in 512-byte pieces
//   2  segmented  initWithSegments() over four separate 8KB ring blocks, same callback
//   3  dictzip    the rest is a .dict.dz file: DictZip::parse() and DictZip::extractEntry()
// Modes 0-2: bytes 1-2 are the requested output length, the rest is the raw deflate stream.
// Mode 3: bytes 1-4 / 5-8 are the extract offset / size, the rest is the file.
// Storage is a memory-backed stub whose state is replaced per input.
#include <DictZip.h>
#include <HalStorage.h>
#include <InflateReader.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The vendored uzlib ships without its checksum sources and the firmware never calls them.
extern "C" uint32_t uzlib_adler32(const void*, unsigned int, uint32_t) { return 0; }
extern "C" uint32_t uzlib_crc32(const void*, unsigned int, uint32_t) { return 0; }

namespace {
struct StreamCtx {
  InflateReader reader;  // must be first (uzlib callback cast, see InflateReader.h)
  const uint8_t* src = nullptr;
  size_t remaining = 0;
};

int feedCb(uzlib_uncomp* u) {
  auto* ctx = reinterpret_cast<StreamCtx*>(u);
  if (ctx->remaining == 0) return -1;
  const size_t n = std::min<size_t>(ctx->remaining, 2048);
  const uint8_t* chunk = ctx->src;
  ctx->src += n;
  ctx->remaining -= n;
  u->source = chunk + 1;
  u->source_limit = chunk + n;
  return chunk[0];
}

uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const uint8_t mode = data[0] & 3;

  if (mode == 3) {
    if (size < 9) return 0;
    const uint32_t offset = le32(data + 1);
    const uint32_t len = le32(data + 5) & 0xFFFFF;  // up to 1MB, like a dictionary definition
    std::vector<uint8_t> bytes(data + 9, data + size);
    bytes.reserve(bytes.size() + 1);
    Storage.setFile("/fuzz.dict.dz", std::move(bytes));
    {
      HalFile f;
      if (Storage.openFileForRead("DICTZIP", "/fuzz.dict.dz", f)) {
        DictZip::Info info;
        DictZip::ExtractError err;
        DictZip::parse(f, &info, &err);
      }
    }
    HalFile out;
    DictZip::ExtractError err = DictZip::ExtractError::None;
    DictZip::extractEntry("/fuzz.dict.dz", offset, len, out, &err);
    return 0;
  }

  if (size < 3) return 0;
  const size_t outLen = 1 + (static_cast<size_t>(data[1]) | (static_cast<size_t>(data[2]) << 8));
  const uint8_t* in = data + 3;
  const size_t inLen = size - 3;
  std::vector<uint8_t> out(outLen);

  if (mode == 0) {
    InflateReader r;
    if (!r.init(false)) return 0;
    r.setSource(in, inLen);
    r.read(out.data(), out.size());
    return 0;
  }

  StreamCtx ctx;
  std::unique_ptr<uint8_t[]> ring[InflateReader::RING_SEGMENTS];
  if (mode == 1) {
    if (!ctx.reader.init(true)) return 0;
  } else {
    uint8_t* segs[InflateReader::RING_SEGMENTS];
    for (size_t i = 0; i < InflateReader::RING_SEGMENTS; i++) {
      ring[i].reset(new uint8_t[InflateReader::RING_SEGMENT_BYTES]);
      segs[i] = ring[i].get();
    }
    if (!ctx.reader.initWithSegments(segs)) return 0;
  }
  ctx.src = in;
  ctx.remaining = inLen;
  ctx.reader.setReadCallback(&feedCb);
  for (size_t done = 0; done < outLen;) {
    const size_t n = std::min<size_t>(512, outLen - done);
    if (!ctx.reader.read(out.data() + done, n)) break;
    done += n;
  }
  return 0;
}
