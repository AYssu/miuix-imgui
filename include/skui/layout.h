#pragma once
#include "imgui.h"

namespace skui {
// Physical pixels supplied by the host, relative to the main viewport.
struct SafeInsets {float left=0,top=0,right=0,bottom=0;};
struct ViewportBounds {ImVec2 position{},size{};};
struct WindowPlacement {ImVec2 position{},size{};};
ViewportBounds ResolveViewport(ImVec2 display,SafeInsets safe={},float keyboard_height=0);
WindowPlacement FitWindow(WindowPlacement window,ViewportBounds viewport,ImVec2 minimum={});
enum class WidthClass {Compact,Medium,Expanded};
// Width is physical; breakpoints 600 / 840 use the current Theme scale.
WidthClass ClassifyWidth(float width);
// Cell minimum uses logical units and increases with text_scale above 1.
int AdaptiveColumns(float available_width,float minimum_cell_width=260,int maximum_columns=3);
// Only call NextAdaptiveCell / EndAdaptiveGrid when Begin returns true.
bool BeginAdaptiveGrid(const char *id,float minimum_cell_width=260,int maximum_columns=3);
void NextAdaptiveCell();
void EndAdaptiveGrid();
} // namespace skui
