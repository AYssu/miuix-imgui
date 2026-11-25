#pragma once
#include "skui/skui.h"
#include <string>
#include <string_view>

namespace gallery {
// Gallery-owned application state; the component library never chooses a path
// or reads/writes files. Geometry is normalized against the visible display.
struct Preferences {
    skui::Theme theme;
    bool wide = false, sidebar_expanded = true;
    ImVec2 position{.035f,.065f}, size{.93f,.85f};
};
bool DecodePreferences(std::string_view text, Preferences &output);
std::string EncodePreferences(const Preferences &value);
enum class LoadResult { Loaded, Missing, Invalid, IoError };
LoadResult LoadPreferences(const std::string &path, Preferences &output);
// Same-directory exclusive temporary file, flush and atomic rename; failed
// writes never truncate the previous configuration. No I/O from Draw().
bool SavePreferences(const std::string &path, const Preferences &value);
} // namespace gallery
