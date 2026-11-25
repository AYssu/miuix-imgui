#pragma once
#define SKUI_VERSION_MAJOR 0
#define SKUI_VERSION_MINOR 1
#define SKUI_VERSION_PATCH 0
#define SKUI_VERSION_STRING "0.1.0"
#define SKUI_VERSION_NUMBER (SKUI_VERSION_MAJOR * 1000000 + SKUI_VERSION_MINOR * 1000 + SKUI_VERSION_PATCH)
namespace skui {
// Compiled library values; compare against the macros when diagnosing a
// mismatched header/library installation. Static C++ ABI is not guaranteed.
const char *GetVersion();
unsigned GetVersionNumber();
}
