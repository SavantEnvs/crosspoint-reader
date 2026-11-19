#pragma once
// Host stand-in for the bit of Arduino.h DictZip.cpp uses (ESP.getMaxAllocHeap).
#include <cstdint>

struct EspHostStub {
  uint32_t getMaxAllocHeap() const { return UINT32_MAX; }
};
inline EspHostStub ESP;
