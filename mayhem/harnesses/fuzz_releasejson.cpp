// Fuzz the OTA release-JSON parser (lib/JsonParser/ReleaseJsonParser.cpp) the way
// OtaUpdater::checkForUpdate drives it: the HTTP body arrives in chunks, the first bytes are fed
// one at a time until tag_name is found, then the firmware asset name is derived from the tag and
// the rest is fed. First input byte selects the chunk size; the rest is the response body.
#include <ReleaseJsonParser.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;
  const size_t chunk = static_cast<size_t>(data[0]) + 1;
  data++;
  size--;

  auto parser = std::make_unique<ReleaseJsonParser>();
  parser->setFirmwareAssetName("");
  char assetName[48] = {};
  bool assetNameSet = false;
  size_t pos = 0;
  while (pos < size) {
    const size_t len = (size - pos < chunk) ? size - pos : chunk;
    size_t offset = 0;
    while (!assetNameSet && offset < len) {
      parser->feed(reinterpret_cast<const char*>(data + pos + offset), 1);
      offset++;
      if (parser->foundTag()) {
        snprintf(assetName, sizeof(assetName), "crosspoint-%s%s.bin", parser->getTagName(), "-x3-x4");
        parser->setFirmwareAssetName(assetName);
        assetNameSet = true;
      }
    }
    if (offset < len) parser->feed(reinterpret_cast<const char*>(data + pos + offset), len - offset);
    pos += len;
  }
  if (parser->foundTag()) (void)parser->getTagName();
  if (parser->foundFirmware()) {
    (void)parser->getFirmwareUrl();
    (void)parser->getFirmwareSize();
  }
  return 0;
}
