#pragma once
// Host stand-in for the display HAL. The PNG/JPEG cover converters only read the panel size in
// their pngFileToBmpStream/jpegFileToBmpStream wrappers (portrait cover = height x width).
// Values are the Xteink X4 panel (800x480).
#include <Print.h>

struct HostDisplay {
  int getDisplayWidth() const { return 800; }
  int getDisplayHeight() const { return 480; }
};

inline HostDisplay display;
