#pragma once

#include "imgui.h"
#include "skui/router.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace skui {

enum class Tone { Primary, Neutral, Danger };
enum class Icon {
    None,
    Components,
    Layout,
    Navigation,
    Settings,
    Back,
    Close,
    Check,
    Chevron,
    Search,
    Sun,
    Moon,
    Plus,
    Heart,
    Info,
    Sidebar
};

struct Theme {
    bool dark = false;
    bool animations = true; // false: immediate state/layout changes, no picker inertia
    float scale = 1.0f;
    ImVec4 accent{0.20f, 0.48f, 1.0f, 1.0f};
    bool haptics = true;
    float motion_response = .8f, motion_damping = .95f, motion_duration_scale = 1.f;
};
// Analytic spring step response. Parameters use Miuix's normalized duration.
float SpringProgress(float fraction, float response = .8f, float damping = .95f);
enum class Haptic { Press, Selection, Confirm };
using HapticHandler = void (*)(Haptic, void *);
void SetHapticHandler(HapticHandler handler, void *user_data = nullptr);
void PerformHaptic(Haptic event);

// Apply after the host has selected its ImGui context, before Begin(). The host
// owns the context, fonts, input and renderer; the library never creates them.
void ApplyTheme(const Theme &theme);
const Theme &GetTheme();
float Dp(float value);
ImVec4 Background();
ImVec4 Surface();
ImVec4 TextColor();
ImVec4 MutedColor();
void DrawIcon(Icon icon, ImVec2 center, float size, ImU32 color);
void Text(const char *text);
void Caption(const char *text);
void Heading(const char *text);
void Section(const char *title);
void Divider();
void Space(float height = 8.0f);
// Call every frame, including while collapsed. Content stays mounted through
// exit, is clipped to the animated height, and cannot receive input on exit.
void Collapsible(const char *id, bool expanded, const std::function<void()> &content);

// Always pair BeginCard/EndCard and BeginScroll/EndScroll, even when false.
bool BeginCard(const char *id);
void EndCard();
// A customizable container. Always pair with EndSurface, including false.
bool BeginSurface(const char *id, ImVec2 size = ImVec2(0,0), float rounding = 18,
                  ImVec4 color = ImVec4(0,0,0,-1));
void EndSurface();
bool BasicComponent(const char *id, const char *title, const char *description = nullptr,
                    Icon icon = Icon::None, const char *value = nullptr, bool arrow = false);
bool ArrowPreference(const char *id, const char *title, const char *description = nullptr,
                     const char *value = nullptr);
bool SwitchPreference(const char *title, bool *value, const char *description = nullptr);
bool CheckboxPreference(const char *title, bool *value, const char *description = nullptr);
bool RadioButtonPreference(const char *title, int *value, int option, const char *description = nullptr);
bool SliderPreference(const char *title, float *value, float minimum, float maximum,
                      const char *description = nullptr, const char *format = "%.0f");
bool RangeSliderPreference(const char *title, float *lower, float *upper, float minimum, float maximum,
                           const char *description = nullptr);
bool BeginScroll(const char *id, ImVec2 size = ImVec2(0, 0), bool horizontal = false);
void EndScroll();
bool Button(const char *label, Tone tone = Tone::Primary, ImVec2 size = ImVec2(0, 0));
bool IconButton(const char *id, Icon icon);
bool Toggle(const char *label, bool *value, const char *description = nullptr);
bool Checkbox(const char *label, bool *value);
bool Radio(const char *label, int *value, int option);
bool Slider(const char *label, float *value, float minimum, float maximum, const char *format = "%.0f",
            ImGuiSliderFlags flags = 0);
// Optional host IME adapter. Called on the rendering thread after each text
// field. Selection offsets are UTF-8 byte offsets. An edit must fit capacity
// including its terminating NUL. The host owns the callback and its lifetime.
struct TextInputEvent {
    ImGuiID id;
    char *buffer;
    std::size_t capacity;
    ImVec2 min, max;
    bool active, activated;
    int selection_start, selection_end;
    bool changed = false;
    bool finished = false;
};
using TextInputHandler = void (*)(TextInputEvent &, void *user_data);
void SetTextInputHandler(TextInputHandler handler, void *user_data = nullptr);
bool InputText(const char *label, char *buffer, std::size_t capacity, const char *hint = "");
bool SearchField(const char *id, char *buffer, std::size_t capacity);
bool Combo(const char *label, int *selected, const char *const *items, int count);
struct ChoiceItem {
    const char *label;
    const char *description = nullptr;
    Icon icon = Icon::None;
    bool enabled = true;
};
// Row-anchored floating selection. Invalid/empty selections display a prompt.
bool Dropdown(const char *label, int *selected, const ChoiceItem *items, int count,
              const char *description = nullptr);
bool Spinner(const char *label, int *selected, const ChoiceItem *items, int count,
             const char *description = nullptr, bool dialog = false);
bool MultiSelect(const char *label, bool *selected, const ChoiceItem *items, int count);
// Action IDs belong to the caller. Children form a submenu; disabled entries
// cannot trigger actions. A negative return means no action this frame.
struct PopupItem {
    int action;
    const char *label;
    Icon icon = Icon::None;
    bool enabled = true;
    bool selected = false;
    bool separator_before = false;
    const PopupItem *children = nullptr;
    int child_count = 0;
};
int DropdownMenu(const char *label, const PopupItem *items, int count, bool icon_only = false);
// Open via OpenDialog(id), call in the same ID scope each frame. The preceding
// item anchors the popup. Supports children, grouping, icons and disabled rows.
int ListPopup(const char *id, const PopupItem *items, int count);
bool RangeSlider(const char *label, float *lower, float *upper, float minimum, float maximum);
bool NumberPicker(const char *label, int *value, int minimum, int maximum, int step = 1);
bool ColorPicker(const char *label, ImVec4 *color, bool alpha = true);
enum class ColorSpace { HSV, OkHSV, OkLab, OkLCH };
// Components: HSV/OkHSV h,s,v in [0,1]; OkLab L,a,b; OkLCH L,C,h in turns.
// The fourth component always preserves alpha. sRGB values use the sRGB transfer function.
ImVec4 ToColorSpace(ImVec4 srgb, ColorSpace space);
ImVec4 FromColorSpace(ImVec4 value, ColorSpace space);
bool ColorPicker(const char *label, ImVec4 *color, ColorSpace space, bool alpha = true);
bool ColorPalette(const char *id, ImVec4 *color, const ImVec4 *colors, int count);
// HSV grid with a gray column, alpha slider and checkerboard preview.
bool ColorPalette(const char *id, ImVec4 *color);
void CircularProgress(float fraction = -1, float size = 32);
// Attach to the preceding item. Also appears while holding a touch target.
void Tooltip(const char *text);
bool BreadcrumbBar(const char *id, const char *const *labels, int count, int *selected);
// Absolute screen positions in pixels, like ShellOptions::initial_position.
// Keep calling with visible=false to render the exit animation.
bool FloatingActionButton(const char *id, Icon icon, ImVec2 position, bool visible = true);
int FloatingToolbar(const char *id, const Icon *icons, const char *const *labels, int count,
                    ImVec2 position, bool visible = true);
// Always pair with EndScroll. Refresh work/state is owned by the caller.
bool BeginRefreshScroll(const char *id, bool refreshing, bool *requested, ImVec2 size = ImVec2(0, 0));
bool ListItem(const char *id, const char *title, const char *description = nullptr,
              const char *value = nullptr, bool arrow = true);
bool TabBar(const char *id, const char *const *labels, int count, int *selected);
void Progress(float fraction, const char *label = nullptr);
void Badge(const char *label);
// Overlay the preceding item without consuming layout or changing its ID.
// nullptr renders a dot; otherwise renders an unread-count badge.
void AnchoredBadge(const char *label = nullptr);
void Swatch(const char *id, ImVec4 color, bool selected, bool *pressed);

// Call OpenDialog once on an action. Begin/EndDialog inside a window each frame.
void OpenDialog(const char *id);
bool BeginDialog(const char *id, const char *title, float width = 340.0f);
void EndDialog();
void CloseDialog();
// Open with OpenDialog; pair EndBottomSheet only when Begin returns true.
// Close with CloseDialog; content stays mounted through the exit animation.
bool BeginBottomSheet(const char *id, const char *title, float height = 340);
void EndBottomSheet();

// Host hook: receive a frame's separate native layers, then render the remaining
// main draw data. id=0/data=null marks end-of-frame and retires unused surfaces.
// Return false to retain that layer in the main renderer after a platform failure.
using WindowRenderHandler = bool (*)(ImGuiID id, ImDrawData *data, void *user_data);
void SetWindowRenderHandler(WindowRenderHandler handler, void *user_data = nullptr);
bool HasWindowBackend();
void RenderWindowLayers(ImDrawData *main_data);
bool WindowDropdownPreference(const char *label,int *selected,const ChoiceItem *items,int count,const char *description=nullptr);
bool WindowSpinnerPreference(const char *label,int *selected,const ChoiceItem *items,int count,const char *description=nullptr);
int WindowListPopup(const char *id,const PopupItem *items,int count);
int WindowCascadingListPopup(const char *id,const PopupItem *items,int count);
int WindowDropdownMenu(const char *label,const PopupItem *items,int count);
int WindowIconDropdownMenu(const char *id,const PopupItem *items,int count);
int WindowIconCascadingDropdownMenu(const char *id,const PopupItem *items,int count);
bool BeginWindowDialog(const char *id,const char *title,float width=340);
void EndWindowDialog();
bool BeginWindowBottomSheet(const char *id,const char *title,float height=340);
void EndWindowBottomSheet();

struct GlassStyle {
    float blur = 12, refraction = 4, saturation = 1.2f, rounding = 24;
    ImVec4 tint{1,1,1,.22f};
};
// Capture the already drawn background before painting glass, excluding the
// glass itself. Capture and Draw must run in the same frame/rendering thread.
// Sampling adapts to blur, up to native density (bounded to 1M pixels / 2048px).
class GlassBackdrop {
  public:
    GlassBackdrop();
    ~GlassBackdrop();
    GlassBackdrop(const GlassBackdrop &)=delete;
    GlassBackdrop &operator=(const GlassBackdrop &)=delete;
    bool Capture(ImDrawList *source,ImVec2 min,ImVec2 max);
    bool Draw(ImVec2 min,ImVec2 max,const GlassStyle &style = {});
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct NavItem {
    const char *label;
    Icon icon;
};
// Independent layout pieces; dimensions are logical units scaled by Theme.
bool TopAppBar(const char *id, const char *title, const char *subtitle = nullptr, bool can_go_back = false);
struct TopAppBarState {
    float scroll_offset = 0;
    float collapse_range = 64;
    float collapsed_fraction = 0; // Updated by CollapsingTopAppBar.
};
// Set scroll_offset from the associated scroll child every frame. The state
// owns no gesture; the content remains the sole owner of scrolling.
bool CollapsingTopAppBar(const char *id,const char *title,TopAppBarState *state,
                         const char *subtitle=nullptr,bool can_go_back=false);
bool NavigationBar(const char *id, const NavItem *items, int count, int *selected);
// An expanded pointer enables the built-in toggle; the caller owns its state.
// nullptr keeps the original expanded layout without a toggle.
bool NavigationRail(const char *id, const NavItem *items, int count, int *selected,
                    bool *expanded = nullptr);
struct ShellOptions {
    const char *id = "skui-app";
    const char *title = "组件";
    const char *subtitle = nullptr;
    const NavItem *items = nullptr;
    int item_count = 0;
    int *selected = nullptr;
    bool can_go_back = false;
    bool *back_requested = nullptr;
    bool *close_requested = nullptr;
    ImVec2 initial_position{32, 100};
    ImVec2 initial_size{900, 1600};
    // Physical screen Y of an occluding OS keyboard; zero disables the limit.
    // The previous window position and size are restored when the limit ends.
    float visible_bottom = 0;
    // Let RouterView own the page header so title/back also participate in motion.
    bool content_includes_header = false;
    // Optional externally controlled wide-screen rail state. Otherwise stored per shell.
    bool *sidebar_expanded = nullptr;
};
// A movable/resizable floating application frame. Initial positions/sizes are
// physical pixels; component dimensions use Theme::scale. EndShell always.
bool BeginShell(const ShellOptions &options);
void EndShell();
ImVec2 ShellPosition();
ImVec2 ShellSize();

class Snackbar {
  public:
    void Show(std::string message, float seconds = 2.5f);
    void Draw(ImVec2 position, float width);

  private:
    std::string message_;
    double until_ = 0;
    double last_draw_ = 0;
    float opacity_ = 0, from_ = 0, elapsed_ = 0;
    bool showing_ = false;
};

// Own one view per router. Draw replaces BeginScroll/Router::Draw/EndScroll.
// Pages run once per frame; outgoing pages are frozen drawing snapshots.
class RouterView {
  public:
    RouterView();
    ~RouterView();
    RouterView(const RouterView &) = delete;
    RouterView &operator=(const RouterView &) = delete;
    void Draw(Router &router, ImVec2 size = ImVec2(0, 0), const Router::Page &header = {});
    bool IsTransitioning() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace skui
