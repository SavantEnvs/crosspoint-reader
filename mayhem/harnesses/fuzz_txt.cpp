// Two text converters, selected by the first input byte (the rest is the document):
//  * bit 0 clear: TxtToHtml::stream (lib/Txt) the way Txt::streamTxtToHtml drives it, i.e. through
//    the read-callback overload with the file delivered in reader-sized pieces, into a null Print.
//    Bits 1-2 pick the file name (.txt / .md / other), bits 3-6 the read size (1 B .. 32 KiB).
//  * bit 0 set: htmlToPlainText (src/util), used for dictionary definitions / book descriptions,
//    with a heading marker byte taken from bits 1-7.
#include <HtmlToPlainText.h>
#include <Print.h>
#include <TxtToHtml.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

class NullPrint : public Print {
 public:
  size_t total = 0;
  size_t write(uint8_t) override {
    total++;
    return 1;
  }
  size_t write(const uint8_t*, size_t size) override {
    total += size;
    return size;
  }
};

struct Reader {
  const uint8_t* data;
  size_t size;
  size_t pos;
  size_t chunk;
};

int readChunk(void* ctx, uint8_t* buf, size_t size) {
  auto* r = static_cast<Reader*>(ctx);
  if (r->pos >= r->size) return 0;
  const size_t n = std::min({size, r->chunk, r->size - r->pos});
  memcpy(buf, r->data + r->pos, n);
  r->pos += n;
  return static_cast<int>(n);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const uint8_t mode = data[0];
  data++;
  size--;

  if (mode & 1) {
    std::string html(reinterpret_cast<const char*>(data), size);
    const std::string out = htmlToPlainText(std::move(html), static_cast<char>(mode >> 1));
    (void)out.size();
    return 0;
  }

  static const char* const kNames[] = {"/books/story.txt", "/books/Notes.MD", "/books/readme.markdown", "plain"};
  Reader reader{data, size, 0, static_cast<size_t>(1) << ((mode >> 3) & 15)};
  NullPrint out;
  (void)TxtToHtml::stream(kNames[(mode >> 1) & 3], &reader, readChunk, out);
  return 0;
}
