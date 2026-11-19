#pragma once
// Host stand-in for the ESP-IDF FreeRTOS header. The PNG/JPEG cover converters only call
// vTaskDelay(1) to yield to the idle task while decoding; upstream's host Arduino.h stub
// (test/chapter_html_slim_parser/stubs) already defines it as a no-op.
#include <Arduino.h>
