#include "skui/skui.h"
#include "detail.h"
#include "imgui_internal.h"
#include <map>
#include <set>
#include <vector>

namespace skui {
namespace {
thread_local WindowRenderHandler renderer=nullptr;
thread_local void *renderer_data=nullptr;
thread_local ImGuiContext *renderer_context=nullptr;
struct NativeScope {
    bool previous=detail::WindowScope();
    NativeScope() {detail::WindowScope()=true;}
    ~NativeScope() {detail::WindowScope()=previous;}
};
}
namespace detail {
bool &WindowScope() {static thread_local bool value=false;return value;}
}
void SetWindowRenderHandler(WindowRenderHandler handler,void *data) {
    renderer=handler;renderer_data=data;renderer_context=ImGui::GetCurrentContext();
}
bool HasWindowBackend() {return renderer && renderer_context==ImGui::GetCurrentContext();}
void RenderWindowLayers(ImDrawData *main) {
    if(!main || !HasWindowBackend()) return;
    std::map<ImGuiID,ImDrawData> layers;
    std::vector<ImGuiID> order;
    std::map<ImDrawList *,ImGuiID> owners;
    for(auto *window:GImGui->Windows) {
        auto *root=window->RootWindow;
        if(root->StateStorage.GetBool(ImHashStr("skui/native-window")))
            owners[window->DrawList]=root->ID;
    }
    for(auto *list:main->CmdLists) {
        auto owner=owners.find(list);if(owner==owners.end()) continue;
        if(!layers.count(owner->second)) order.push_back(owner->second);
        auto &layer=layers[owner->second];
        layer.Valid=true;layer.DisplayPos=main->DisplayPos;layer.DisplaySize=main->DisplaySize;
        layer.FramebufferScale=main->FramebufferScale;layer.OwnerViewport=main->OwnerViewport;layer.Textures=main->Textures;
        layer.AddDrawList(list);
    }
    std::set<ImGuiID> rendered;
    for(auto id:order) if(renderer(id,&layers[id],renderer_data)) rendered.insert(id);
    for(int i=main->CmdLists.Size-1;i>=0;--i) {
        auto *list=main->CmdLists[i];auto owner=owners.find(list);
        if(owner!=owners.end() && rendered.count(owner->second)) {
            main->TotalVtxCount-=list->VtxBuffer.Size;main->TotalIdxCount-=list->IdxBuffer.Size;
            main->CmdLists.erase(main->CmdLists.Data+i);
        }
    }
    main->CmdListsCount=main->CmdLists.Size;
    renderer(0,nullptr,renderer_data);
}
bool WindowDropdownPreference(const char *label,int *selected,const ChoiceItem *items,int count,const char *description) {
    NativeScope scope;return Dropdown(label,selected,items,count,description);
}
bool WindowSpinnerPreference(const char *label,int *selected,const ChoiceItem *items,int count,const char *description) {
    NativeScope scope;return Spinner(label,selected,items,count,description);
}
int WindowListPopup(const char *id,const PopupItem *items,int count) {NativeScope scope;return ListPopup(id,items,count);}
int WindowCascadingListPopup(const char *id,const PopupItem *items,int count) {return WindowListPopup(id,items,count);}
int WindowDropdownMenu(const char *label,const PopupItem *items,int count) {NativeScope scope;return DropdownMenu(label,items,count);}
int WindowIconDropdownMenu(const char *id,const PopupItem *items,int count) {NativeScope scope;return DropdownMenu(id,items,count,true);}
int WindowIconCascadingDropdownMenu(const char *id,const PopupItem *items,int count) {return WindowIconDropdownMenu(id,items,count);}
bool BeginWindowDialog(const char *id,const char *title,float width) {NativeScope scope;return BeginDialog(id,title,width);}
void EndWindowDialog() {EndDialog();}
bool BeginWindowBottomSheet(const char *id,const char *title,float height) {NativeScope scope;return BeginBottomSheet(id,title,height);}
void EndWindowBottomSheet() {EndBottomSheet();}
} // namespace skui
