#pragma once
// Minimal Epub.h for the ContentOpfParser fuzz harness: the parser only needs HalFile/Storage from it
// (memory-backed, see ../mem/HalStorage.h), unlike upstream's gtest-based test stub.
#include <HalStorage.h>

#include <string>
