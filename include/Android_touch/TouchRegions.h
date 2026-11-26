#pragma once
#include "imgui.h"
#include <array>
#include <cstddef>

namespace Touch {
// A frame snapshot, copied under the input lock. Separate rectangles preserve
// Android touch-through in the gaps between independent ImGui windows.
struct CaptureRegions {
    static constexpr std::size_t Capacity = 128;
    std::array<ImVec4, Capacity> rectangles{};
    std::size_t count = 0;
    bool Contains(float x, float y) const;
    void Add(ImVec2 min, ImVec2 max);
};
// Call after building every window, before Render. The IME area stays with
// Android; an open popup captures outside taps so ImGui can dismiss it.
CaptureRegions CollectCaptureRegions(ImVec2 display, float keyboard_height = 0);
} // namespace Touch
