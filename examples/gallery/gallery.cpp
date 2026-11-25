#include "skui/devtools.h"
#include "gallery.h"
#include "preferences.h"
#include "data_model.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>

namespace gallery {
namespace {
struct Category { const char *title; const char *note; skui::Icon icon; };
const Category categories[] = {
    {"基础操作", "按钮、文本、图标与悬浮操作", skui::Icon::Components},
    {"表单与偏好", "输入、开关、单选、复选与滑块", skui::Icon::Settings},
    {"浮层与菜单", "Dropdown、Spinner、列表、级联菜单与面板", skui::Icon::Navigation},
    {"导航与布局", "侧栏、底栏、标签、顶栏与容器", skui::Icon::Layout},
    {"反馈与刷新", "提示、进度、角标与下拉刷新", skui::Icon::Info},
    {"颜色与数字", "数字滚轮、色板与连续调色", skui::Icon::Sun},
    {"平台窗口", "独立 Surface 的列表、菜单与面板", skui::Icon::Layout}
};
struct Entry { const char *name; int category; const char *note; bool available = true; };
const Entry entries[]={
    {"Scaffold",3,"应用壳：标题、内容、导航与返回"},
    {"Surface",3,"可定制背景与圆角的容器"},{"TopAppBar",3,"独立顶栏与返回操作"},
    {"NavigationBar",3,"独立底部导航"},{"NavigationRail",3,"Sidebar：可折叠侧栏与内容联动"},
    {"TabRow",3,"横向标签切换"},{"BreadcrumbBar",3,"可横向拖动的路径"},
    {"Card",3,"内容卡片"},{"BasicComponent",1,"图标、标题、摘要与尾部内容"},
    {"Button",0,"操作按钮与禁用状态"},{"IconButton",0,"图标按钮"},
    {"Text",0,"正文、标题、摘要"},{"SmallTitle",0,"分组标题"},
    {"TextField",1,"系统输入法文本输入"},{"Switch",1,"开关"},
    {"Checkbox",1,"复选框"},{"RadioButton",1,"单选框"},
    {"Slider",1,"数值滑块"},{"NumberPicker",5,"独立数字滚轮"},
    {"ProgressIndicator",4,"线形与圆形进度"},{"Snackbar",4,"操作结果提示"},
    {"Tooltip",4,"长按或悬停提示"},{"Badge",4,"锚点圆点与未读计数"},
    {"Icon",0,"图标展示"},{"FloatingActionButton",0,"开启底部悬浮操作"},
    {"FloatingToolbar",0,"开启底部悬浮操作"},{"Divider",0,"内容分隔线"},
    {"PullToRefresh",4,"顶部下拉、松开刷新"},{"SearchBar",1,"文本搜索"},
    {"Glass",3,"背景模糊、折射与圆角阴影"},
    {"CollapsingTopAppBar",3,"随滚动折叠的大标题"},
    {"HapticFeedback",4,"按压、选择与确认的触觉反馈"},
    {"Motion",4,"响应、阻尼与动画时长"},
    {"ColorPalette",5,"HSV 网格、灰度列、透明度"},{"ColorPicker",5,"HSV 连续调色"},
    {"ArrowPreference",1,"可导航的设置行"},{"SwitchPreference",1,"开关与说明"},
    {"CheckboxPreference",1,"复选与说明"},{"RadioButtonPreference",1,"单选与说明"},
    {"SliderPreference",1,"滑块与说明"},{"RangeSliderPreference",1,"两个范围端点与说明"},
    {"OverlayListPopup",2,"自定义触发器打开列表"},
    {"OverlayCascadingListPopup",2,"列表内二级切换"},
    {"OverlayDropdownPreference",2,"整行打开浮动单选"},
    {"OverlaySpinnerPreference",2,"图标、摘要、禁用项"},
    {"OverlayDropdownMenu",2,"带图标与分组的操作菜单"},
    {"OverlayIconDropdownMenu",2,"图标触发操作菜单"},
    {"OverlayIconCascadingDropdownMenu",2,"图标触发二级菜单"},
    {"OverlayBottomSheet",2,"应用窗口内底部面板"},{"OverlayDialog",2,"确认与取消对话框"},
    {"WindowListPopup",6,"独立窗口组件"},
    {"WindowCascadingListPopup",6,"独立窗口组件"},
    {"WindowDropdownPreference",6,"独立窗口组件"},
    {"WindowSpinnerPreference",6,"独立窗口组件"},
    {"WindowDropdownMenu",6,"独立窗口组件"},
    {"WindowIconDropdownMenu",6,"独立窗口组件"},
    {"WindowIconCascadingDropdownMenu",6,"独立窗口组件"},
    {"WindowBottomSheet",6,"独立窗口组件"},
    {"WindowDialog",6,"独立窗口组件"}
};

struct Gallery {
    skui::Router router;
    skui::RouterView router_view;
    skui::Theme theme;
    skui::Snackbar snackbar;
    int nav = 0, radio = 0, choice = 0, clicks = 0, accent = 0;
    bool notifications = true, checked = true, wide = false;
    bool progress_running = false, show_details = true;
    float volume = 42, progress = .35f;
    float range_low = 20, range_high = 80;
    int spinner_choice = 0, dialog_choice = 1, number = 5, breadcrumb = 2, refresh_count = 0;
    int long_breadcrumb = 6;
    bool breadcrumb_enabled = true;
    bool multiple[4] = {true, false, false, false};
    bool floating_tools = false, sidebar_expanded = true, sample_sidebar_expanded = true;
    int long_choice = 0;
    double refresh_until = 0;
    ImVec4 picked_color{.20f, .48f, 1, 1};
    char name[96] = "你好，skui";
    char search[96] = "";
    bool initialized = false;
    float scale = 2.2f;
    ImVec2 window_pos{}, window_size{};
    int sample_nav=0, color_space=0;
    skui::GlassBackdrop glass;
    skui::GlassStyle glass_style;
    skui::TopAppBarState appbar;
    char catalogue_search[96]="";
    skui::FrameProfiler profiler;
    bool developer_tools=false,show_metrics=false,show_ids=false;
    std::string preferences_path;
    Preferences saved;
    bool save_requested = false, auto_save = false;
    DataLoader data_loader;
    std::vector<DataRow> data_rows;
    DataView data_view;
    char data_query[96]="";
    bool data_descending=false, data_fail=false, data_scroll_pending=false;
    float data_scroll=0, data_next_scroll=0, data_row_height=58;
    int data_selected=0;
    std::string data_error;
    float preview_text_scale=1;
    bool preview_english=false,preview_enabled=true;
    bool preview_choices[3]{true,true,true};
    char preview_name[96]="";

    void Adaptive() {
        skui::Section("字号与翻译预览");
        if(skui::BeginCard("adaptive-controls")) {
            skui::Slider("文字大小",&preview_text_scale,.85f,1.5f,"%.2f×");
            skui::Toggle("English",&preview_english,"切换较长的界面文案，检查换行与触摸区域");
            skui::Caption("拖动窗口调整宽度；卡片会自动换列。打开下方输入框可检查键盘避让。");
        }
        skui::EndCard();
        auto local=theme;local.tokens.text_scale=preview_text_scale;
        skui::ThemeScope scope(local);ImGui::PushFont(nullptr,skui::TextSize(16));
        int columns=skui::AdaptiveColumns(ImGui::GetContentRegionAvail().x,240,3);
        char summary[96];std::snprintf(summary,sizeof(summary),"%d 列 · 文字 %.2f× · 宽度 %.0f dp",columns,preview_text_scale,ImGui::GetContentRegionAvail().x/skui::Dp(1));
        skui::Caption(summary);
        if(skui::BeginAdaptiveGrid("adaptive-cards",240,3)) {
            const char *zh[]={"连接与自动同步","通知与专注模式","存储与备份"};
            const char *en[]={"Connection and automatic synchronization","Notifications and distraction-free focus","Storage management and scheduled backup"};
            for(int i=0;i<3;++i) {
                skui::NextAdaptiveCell();ImGui::PushID(i);
                if(skui::BeginCard("card")) {
                    skui::Checkbox(preview_english ? en[i] : zh[i],&preview_choices[i]);
                    skui::Caption(preview_english ? "A longer translated description wraps within the card, while the next action remains reachable." : "这段说明会随卡片宽度自动换行，下一项操作仍能正常点击。");
                    if(skui::Button(preview_english ? "Open settings" : "打开设置",skui::Tone::Neutral,ImVec2(-1,0))) Message(preview_english ? en[i] : zh[i]);
                }
                skui::EndCard();ImGui::PopID();
            }
            skui::EndAdaptiveGrid();
        }
        if(skui::BeginCard("adaptive-input")) {
            skui::InputText(preview_english ? "Display name" : "显示名称",preview_name,sizeof(preview_name));
            skui::SwitchPreference(preview_english ? "Synchronize automatically when a trusted connection becomes available" : "连接到可信网络时自动同步",&preview_enabled,
                preview_english ? "Background changes are saved after confirmation. This deliberately long translation must grow the row rather than overlap the next item." : "较长的标题与说明会让列表项自然增高，不覆盖后面的控件。");
        }
        skui::EndCard();ImGui::PopFont();
    }

    void PollData() {
        if(router.Current()->path!="/runtime") data_loader.Cancel();
        DataResult result;
        if(!data_loader.TakeResult(result)) return;
        data_error=std::move(result.error);
        if(data_error.empty()) {
            auto anchor=data_view.Anchor(data_rows,data_scroll,data_row_height);
            data_rows=std::move(result.rows);
            data_view.Rebuild(data_rows,data_query,data_descending);
            data_next_scroll=data_view.Restore(data_rows,anchor,data_row_height);data_scroll_pending=true;
            if(data_selected>static_cast<int>(data_rows.size())) data_selected=0;
        }
        skui::RequestAnimationFrame();
    }
    void Runtime() {
        skui::Section("后台加载");
        if(skui::BeginCard("data-task")) {
            skui::Caption("后台准备数据，界面继续响应；离开此页会取消未完成的请求。");
            skui::Toggle("模拟加载失败",&data_fail,"关闭后重试，可恢复正常加载");
            auto status=data_loader.Status();
            if(status.state==LoadState::Loading) {
                skui::Progress(status.progress,"正在准备记录");
                if(skui::Button("取消加载",skui::Tone::Neutral)) data_loader.Cancel();
            } else {
                if(skui::Button(status.state==LoadState::Failed ? "重试加载" : "加载 10000 条记录")) {
                    data_error.clear();data_loader.Start(10000,data_fail);
                }
                if(status.state==LoadState::Cancelled) skui::Caption("加载已取消，已有记录继续保留。");
            }
            if(!data_error.empty()) skui::Caption(data_error.c_str());
        }
        skui::EndCard();
        skui::Section("长列表与稳定选择");
        auto anchor=data_view.Anchor(data_rows,data_scroll,data_row_height);
        bool changed=skui::SearchField("data-search",data_query,sizeof(data_query));
        changed=skui::Toggle("按编号倒序",&data_descending,"过滤和排序时保留选中记录及可见锚点") || changed;
        data_row_height=skui::Dp(50)+ImGui::GetStyle().ItemSpacing.y;
        if(changed) {
            data_view.Rebuild(data_rows,data_query,data_descending);
            data_next_scroll=data_view.Restore(data_rows,anchor,data_row_height);data_scroll_pending=true;
        }
        const auto &indices=data_view.Indices();
        if(data_scroll_pending) {ImGui::SetNextWindowScroll(ImVec2(0,data_next_scroll));data_scroll_pending=false;}
        ImGui::SetNextWindowContentSize(ImVec2(0,std::max(0.f,indices.size()*data_row_height-ImGui::GetStyle().ItemSpacing.y)));
        int drawn=0;
        if(skui::BeginScroll("records",ImVec2(0,skui::Dp(300)))) {
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(indices.size()),data_row_height);
            while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                const auto &row=data_rows[indices[i]];
                ImGui::PushID(row.id);
                if(skui::ListItem("record",row.title.c_str(),nullptr,row.id==data_selected ? "已选择" : nullptr,false)) data_selected=row.id;
                ImGui::PopID();++drawn;
            }
        }
        data_scroll=ImGui::GetScrollY();skui::EndScroll();
        char summary[128];std::snprintf(summary,sizeof(summary),"匹配 %zu 条 · 本帧绘制 %d 条 · 选择 #%d",indices.size(),drawn,data_selected);
        skui::Caption(summary);
    }

    Preferences CapturePreferences() const {
        Preferences p=saved;
        p.theme=theme; p.wide=wide; p.sidebar_expanded=sidebar_expanded;
        return p;
    }
    void ConfigurePreferences(const char *path) {
        if (initialized) return;
        preferences_path=path ? path : "";
        if (preferences_path.empty()) return;
        auto result=LoadPreferences(preferences_path,saved);
        auto_save=result==LoadResult::Loaded || result==LoadResult::Missing;
        if (result==LoadResult::Loaded) {
            theme=saved.theme; wide=saved.wide; sidebar_expanded=saved.sidebar_expanded;
        } else if (!auto_save) Message("设置读取失败，已使用默认值；原文件会保留，手动保存后才会替换。");
    }
    void FlushPreferences(bool exiting) {
        if (preferences_path.empty() || (!save_requested && !(exiting && auto_save))) return;
        bool explicit_save=save_requested; save_requested=false;
        bool ok=SavePreferences(preferences_path,CapturePreferences());
        if (ok) auto_save=true;
        if (explicit_save) Message(ok ? "设置已保存" : "保存失败，原设置文件保持不变");
        else if (!ok) std::fprintf(stderr,"sk-imgui-ui: failed to save preferences\n");
    }

    void Message(const char *text) { snackbar.Show(text); }
    void CatalogueRows(const char *filter, int family = -1) {
        std::string query=filter;
        std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return std::tolower(c);});
        int visible=0;
        for(int group=0;group<7;++group) {
            if(family>=0 && group!=family) continue;
            bool opened=false;
            ImGui::PushID(group);
            for(const auto &entry:entries) {
                if(entry.category!=group) continue;
                std::string name=std::string(entry.name)+" "+entry.note;
                std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
                if(!query.empty() && name.find(query)==std::string::npos) continue;
                if(!opened) {
                    skui::Section(categories[group].title);
                    skui::BeginCard("entries"); opened=true;
                } else skui::Divider();
                ++visible;
                if(skui::BasicComponent(entry.name,entry.name,entry.note,skui::Icon::None,
                    !entry.available ? "未实现" : nullptr,entry.available) && entry.available)
                    router.Push("/component-demo",entry.name);
            }
            if(opened) skui::EndCard();
            ImGui::PopID();
        }
        if(!visible) skui::Caption("没有匹配的组件，试试中文说明或英文名称。");
    }
    void Components() {
        skui::SearchField("component-search",search,sizeof(search));
        if(search[0]) { CatalogueRows(search); return; }
        skui::Section("应用基础");
        if(skui::BeginCard("application-foundations")) {
            if(skui::ArrowPreference("runtime","任务与长列表","取消、重试、进度、10000 条记录、排序与过滤")) router.Push("/runtime");
            skui::Divider();
            if(skui::ArrowPreference("adaptive","自适应与字体","宽度断点、长翻译、字号、键盘避让")) router.Push("/adaptive");
        }
        skui::EndCard();
        skui::Section("选择组件");
        if(skui::BeginCard("featured-selectors")) {
            if(skui::ArrowPreference("dropdown","DropdownPreference","整行打开浮动单选，展示短列表与长列表"))
                router.Push("/component-demo","OverlayDropdownPreference");
            skui::Divider();
            if(skui::ArrowPreference("spinner","SpinnerPreference","图标与摘要，比较浮层和对话框两种模式"))
                router.Push("/component-demo","OverlaySpinnerPreference");
        }
        skui::EndCard();
        skui::Section("材质与平台能力");
        if(skui::BeginCard("advanced-entry")) {
            if(skui::ArrowPreference("glass-entry","Glass","背景模糊、折射与实时材质"))
                router.Push("/component-demo","Glass");
            skui::Divider();
            if(skui::ArrowPreference("window-entry","Window 独立窗口","九种窗口组件，打开与关闭后自动回收"))
                router.Push("/catalogue","6");
        }
        skui::EndCard();
        skui::Section("按类别浏览");
        if(skui::BeginCard("component-categories")) {
            for(int i=0;i<7;++i) {
                if(i) skui::Divider();
                ImGui::PushID(i);
                if(skui::BasicComponent("category",categories[i].title,nullptr,categories[i].icon,
                                        nullptr,true)) {
                    catalogue_search[0]='\0';
                    router.Push("/catalogue",std::to_string(i));
                }
                ImGui::PopID();
            }
        }
        skui::EndCard();
        skui::Space(8);
        if(skui::Button("查看完整组件目录",skui::Tone::Neutral,ImVec2(-1,0))) {
            catalogue_search[0]='\0'; router.Push("/catalogue");
        }
    }
    void Catalogue(const skui::Route &route) {
        int family=route.parameter.size()==1 && route.parameter[0]>='0' && route.parameter[0]<='6'
            ? route.parameter[0]-'0' : -1;
        skui::SearchField("catalogue-search",catalogue_search,sizeof(catalogue_search));
        skui::Caption(family>=0 ? categories[family].note : "按类别排列；点名称进入独立示例。");
        CatalogueRows(catalogue_search,family);
    }
    void ComponentDemo(const skui::Route &route) {
        const auto &component=route.parameter;
        for(const auto &entry:entries) if(component==entry.name) { skui::Caption(entry.note); break; }
        skui::Space(8);
        const skui::ChoiceItem choices[]={{"跟随系统","使用系统设置",skui::Icon::Settings},
            {"浅色外观","明亮的界面",skui::Icon::Sun},{"深色外观","夜间使用",skui::Icon::Moon}};
        const skui::ChoiceItem advanced_choices[]={{"跟随系统","使用系统当前外观",skui::Icon::Settings},
            {"浅色外观","明亮、清晰的界面",skui::Icon::Sun},{"深色外观","适合夜间使用",skui::Icon::Moon},
            {"暂不可用","禁用选项不能选中",skui::Icon::Info,false}};
        const skui::PopupItem children[]={{10,"按名称排序"},{11,"按时间排序"},{12,"按大小排序"}};
        const skui::PopupItem actions[]={{0,"收藏",skui::Icon::Heart},
            {-1,"排序方式",skui::Icon::Navigation,true,false,false,children,3},
            {1,"分享",skui::Icon::Plus,true,false,true},{2,"暂不可用",skui::Icon::Info,false}};
        const skui::NavItem navigation[]={{"首页",skui::Icon::Components},{"发现",skui::Icon::Search},
            {"设置",skui::Icon::Settings}};
        if(component=="Surface" || component=="Card") {
            if(skui::BeginSurface("custom-surface",ImVec2(0,0),24)) {
                skui::Heading("内容容器");skui::Caption("可以设置容器颜色、圆角和尺寸。");
                skui::Button("容器内操作");
            } skui::EndSurface();
        } else if(skui::BeginCard("focused-demo")) {
            if(component=="Scaffold") skui::Caption("当前页面就是 Scaffold：顶栏、返回、内容区域与底部导航独立协作。");
            else if(component=="TopAppBar") {
                if(skui::TopAppBar("top","示例标题","页面说明与返回操作",true)) Message("返回按钮已触发");
            } else if(component=="CollapsingTopAppBar") {
                skui::CollapsingTopAppBar("collapse","我的内容",&appbar,"向上滚动，标题逐渐收起",true);
                skui::BeginScroll("appbar-content",ImVec2(0,skui::Dp(240)));
                appbar.scroll_offset=ImGui::GetScrollY();
                for(int i=0;i<20;++i) {char line[40];std::snprintf(line,sizeof(line),"内容 %02d",i+1);skui::ListItem(line,line,nullptr,nullptr,false);}
                skui::EndScroll();
            } else if(component=="Glass") {
                if(theme.animations) skui::RequestAnimationFrame();
                skui::Slider("模糊",&glass_style.blur,0,24);skui::Slider("折射",&glass_style.refraction,0,12);
                skui::Slider("色调强度",&glass_style.tint.w,0,.6f,"%.2f");
                skui::Caption("模糊与折射为 0 时背景清晰；色调强度控制白色蒙层。");
                auto p=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x,h=skui::Dp(220);
                auto *d=ImGui::GetWindowDrawList();
                d->AddRectFilled(p,ImVec2(p.x+w,p.y+h),ImGui::GetColorU32(ImVec4(.15f,.2f,.38f,1)),skui::Dp(24));
                for(int i=0;i<9;++i) {
                    float phase=static_cast<float>((theme.animations ? ImGui::GetTime()*.4 : 0)+i);
                    d->AddCircleFilled(ImVec2(p.x+w*(.12f+.095f*i),p.y+h*(.5f+.23f*std::sin(phase))),skui::Dp(30),
                        ImGui::GetColorU32(ImVec4(.3f+.06f*i,.65f,.95f-.08f*i,1)),32);
                }
                std::chrono::steady_clock::time_point effect_start;
                if(profiler.Enabled()) effect_start=std::chrono::steady_clock::now();
                glass.Capture(d,p,ImVec2(p.x+w,p.y+h));
                auto a=ImVec2(p.x+skui::Dp(22),p.y+skui::Dp(36)),b=ImVec2(p.x+w-skui::Dp(22),p.y+h-skui::Dp(36));
                glass.Draw(a,b,glass_style);
                if(profiler.Enabled()) profiler.AddEffectTime(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-effect_start).count());
                d->AddText(ImVec2(a.x+skui::Dp(16),a.y+skui::Dp(28)),IM_COL32_WHITE,"Glass");
                d->AddText(ImVec2(a.x+skui::Dp(16),a.y+skui::Dp(64)),IM_COL32_WHITE,"流动的背景，柔和的材质");
                ImGui::Dummy(ImVec2(w,h));
                if(profiler.Enabled()) {
                    char timing[80];std::snprintf(timing,sizeof(timing),"Glass CPU: %.3f ms",profiler.Latest().effects_ms);
                    skui::Caption(timing);
                }
            } else if(component=="HapticFeedback") {
                skui::Toggle("触觉反馈",&theme.haptics);
                if(skui::Button("轻触")) skui::PerformHaptic(skui::Haptic::Press);
                if(skui::Button("选择")) skui::PerformHaptic(skui::Haptic::Selection);
                if(skui::Button("确认")) skui::PerformHaptic(skui::Haptic::Confirm);
                skui::Caption("支持的设备会产生短促振动，系统勿扰设置仍然生效。");
            } else if(component=="Motion") {
                skui::Slider("响应",&theme.motion_response,.4f,1.5f,"%.2f");
                skui::Slider("阻尼",&theme.motion_damping,.65f,1.5f,"%.2f");
                skui::Slider("时长倍率",&theme.motion_duration_scale,.5f,2,"%.2f");
                skui::Caption("响应与阻尼调整页面转场，时长倍率也控制折叠与弹层。");
                if(skui::Button("查看路由转场")) router.Push("/detail","导航弹簧参数演示");
                skui::Toggle("展开内容",&show_details);
                skui::Collapsible("motion-sample",show_details,[]{skui::Space(20);skui::Text("可中途反向的展开过渡");skui::Space(80);});
            } else if(component=="NavigationBar") skui::NavigationBar("nav",navigation,3,&sample_nav);
            else if(component=="NavigationRail") {
                skui::NavigationRail("nav",navigation,3,&sample_nav,&sample_sidebar_expanded);
                ImGui::SameLine(0,skui::Dp(12));
                if(skui::BeginSurface("rail-page",ImVec2(0,skui::Dp(290)))) {
                    skui::Heading(navigation[sample_nav].label);
                    skui::Caption("点击侧栏图标切换内容；左上角按钮展开或收起侧栏。");
                    skui::Space(12);
                    if(sample_nav==0) { if(skui::Button("刷新首页")) Message("首页内容已刷新"); }
                    else if(sample_nav==1) skui::SwitchPreference("只看收藏",&checked);
                    else skui::SwitchPreference("消息通知",&notifications);
                }
                skui::EndSurface();
            }
            else if(component=="TabRow") {
                const char *labels[]={"全部","收藏","最近"};skui::TabBar("tabs",labels,3,&sample_nav);
            } else if(component=="BreadcrumbBar") {
                skui::Toggle("允许点击路径",&breadcrumb_enabled,"关闭后仍可左右滑动查看完整路径。");
                skui::Caption("短路径 · 高亮位置与完整路径独立");
                const char *labels[]={"设备","文档","项目","资料"};
                skui::BreadcrumbBar("path",labels,4,&breadcrumb,breadcrumb_enabled);
                std::string path;
                for(int i=0;i<=breadcrumb && i<4;++i) {if(i) path+=" / ";path+=labels[i];}
                skui::Caption(path.c_str());
                skui::Space(12);
                skui::Caption("长路径 · 左右滑动，长名称自动省略");
                const char *long_path[]={"内部存储","文档","设计项目","组件库","一个需要省略显示的很长很长的文件夹名称","导航示例","当前目录"};
                skui::BreadcrumbBar("long-path",long_path,7,&long_breadcrumb,breadcrumb_enabled);
                if(skui::Button("返回根目录",skui::Tone::Neutral)) long_breadcrumb=0;
                ImGui::SameLine();
                if(skui::Button("定位当前目录",skui::Tone::Neutral)) long_breadcrumb=6;
            } else if(component=="BasicComponent" || component=="ArrowPreference") {
                if(skui::BasicComponent("profile","个人资料","查看昵称与个人设置",skui::Icon::Heart,"查看",true))
                    router.Push("/detail","个人资料");
            } else if(component=="Button") {
                if(skui::Button("主要操作")) { ++clicks;Message("按钮已触发"); }
                ImGui::SameLine();if(skui::Button("普通操作",skui::Tone::Neutral)) Message("普通操作已触发");
                skui::Space(6);
                if(skui::Button("危险操作",skui::Tone::Danger)) Message("危险样式按钮已触发");
                ImGui::BeginDisabled();skui::Button("禁用按钮");ImGui::EndDisabled();
                char count[48];std::snprintf(count,sizeof(count),"主要操作触发 %d 次",clicks);skui::Caption(count);
            } else if(component=="Icon" || component=="IconButton") {
                if(skui::IconButton("heart",skui::Icon::Heart)) Message("已收藏");
                ImGui::SameLine();skui::IconButton("settings",skui::Icon::Settings);
            } else if(component=="Text" || component=="SmallTitle" || component=="Divider") {
                skui::Section("分组标题");skui::Text("正文内容");skui::Divider();skui::Caption("辅助说明");
            } else if(component=="TextField") {
                skui::InputText("显示名称",name,sizeof(name),"输入名称");
                skui::Space(8);skui::Caption("当前内容");skui::Text(name);
            }
            else if(component=="SearchBar") skui::SearchField("sample-search",search,sizeof(search));
            else if(component=="Switch" || component=="SwitchPreference")
                skui::SwitchPreference("消息通知",&notifications,"接收新消息与重要提醒");
            else if(component=="Checkbox" || component=="CheckboxPreference")
                skui::CheckboxPreference("自动同步",&checked,"连接网络时同步设置与收藏");
            else if(component=="RadioButton" || component=="RadioButtonPreference") {
                skui::RadioButtonPreference("标准模式",&radio,0,"适合日常使用");
                skui::RadioButtonPreference("专注模式",&radio,1,"减少打扰与通知");
            } else if(component=="Slider" || component=="SliderPreference")
                skui::SliderPreference("音量",&volume,0,100,"拖动调整媒体音量","%.0f %%");
            else if(component=="RangeSliderPreference")
                skui::RangeSliderPreference("允许范围",&range_low,&range_high,0,100,"分别拖动两个端点");
            else if(component=="NumberPicker") skui::NumberPicker("数量",&number,0,30);
            else if(component=="ProgressIndicator") {
                skui::Progress(progress,"当前进度");skui::CircularProgress(progress);
                ImGui::SameLine();skui::CircularProgress();
                if(skui::Button(progress_running ? "暂停" : "开始演示")) {
                    if(progress>=1) progress=0;
                    progress_running=!progress_running;
                }
            } else if(component=="Snackbar") {
                if(skui::Button("显示消息")) Message("操作已完成");
            } else if(component=="Tooltip") {
                skui::IconButton("hint",skui::Icon::Info);skui::Tooltip("这是长按显示的提示");
                skui::Caption("长按图标查看提示。");
            } else if(component=="Badge") {
                skui::IconButton("notification",skui::Icon::Heart);skui::AnchoredBadge("9");
                ImGui::SameLine();skui::IconButton("dot",skui::Icon::Settings);skui::AnchoredBadge();
                skui::Caption("数字与圆点叠加在图标角上，不占用额外布局。");
            } else if(component=="FloatingActionButton" || component=="FloatingToolbar")
                skui::Toggle("显示悬浮操作",&floating_tools,"按钮显示在页面底部");
            else if(component=="PullToRefresh") {
                bool requested=false;
                skui::BeginRefreshScroll("focused-refresh",ImGui::GetTime()<refresh_until,&requested,ImVec2(0,skui::Dp(280)));
                for(int i=0;i<6;++i) { char line[64];std::snprintf(line,sizeof(line),"内容 %d · 已刷新 %d 次",i+1,refresh_count);skui::Text(line);skui::Space(30); }
                skui::EndScroll();if(requested) { ++refresh_count;refresh_until=ImGui::GetTime()+.8; }
            } else if(component=="ColorPalette" || component=="ColorPicker") {
                if(component=="ColorPalette") {
                    const ImVec4 presets[]={{.2f,.48f,1,1},{.49f,.36f,.91f,1},{.06f,.64f,.48f,1},
                        {.96f,.43f,.24f,1},{.87f,.27f,.49f,1}};
                    skui::ColorPalette("presets",&picked_color,presets,5);
                    skui::Space(8);skui::ColorPalette("grid",&picked_color);
                } else {
                    const char *spaces[]={"HSV","OkHSV","OkLab","OkLCH"};
                    skui::Combo("色彩空间",&color_space,spaces,4);
                    skui::ColorPicker("自定义颜色",&picked_color,static_cast<skui::ColorSpace>(color_space));
                }
                if(skui::Button("设为强调色",skui::Tone::Primary,ImVec2(-1,0))) {
                    theme.accent=ImVec4(picked_color.x,picked_color.y,picked_color.z,1);Message("强调色已更新");
                }
            }
            else if(component.rfind("Window",0)==0) {
                if(!skui::HasWindowBackend()) skui::Caption("当前宿主没有安装独立窗口后端，使用同窗口浮层。");
                if(component=="WindowDropdownPreference") skui::WindowDropdownPreference("外观",&choice,choices,3);
                else if(component=="WindowSpinnerPreference") skui::WindowSpinnerPreference("外观",&spinner_choice,advanced_choices,4,"包含图标、摘要与禁用项");
                else if(component=="WindowDialog" || component=="WindowBottomSheet") {
                    bool sheet=component=="WindowBottomSheet";
                    if(skui::Button("打开独立窗口")) skui::OpenDialog("native-panel");
                    bool open=sheet ? skui::BeginWindowBottomSheet("native-panel","独立窗口面板",240) : skui::BeginWindowDialog("native-panel","确认操作");
                    if(open) {
                        skui::Caption("点击外部或完成按钮关闭。");
                        if(skui::Button("完成",skui::Tone::Primary,ImVec2(-1,0))) skui::CloseDialog();
                        if(sheet) skui::EndWindowBottomSheet();else skui::EndWindowDialog();
                    }
                } else {
                    bool cascade=component.find("Cascading")!=std::string::npos;int action=-1;
                    if(component.find("ListPopup")!=std::string::npos) {
                        if(skui::Button("打开独立窗口列表")) skui::OpenDialog("native-list");
                        action=cascade ? skui::WindowCascadingListPopup("native-list",actions,4) : skui::WindowListPopup("native-list",children,3);
                    } else if(component.find("Icon")!=std::string::npos)
                        action=cascade ? skui::WindowIconCascadingDropdownMenu("native-actions",actions,4) : skui::WindowIconDropdownMenu("native-actions",children,3);
                    else action=skui::WindowDropdownMenu("打开操作菜单",children,3);
                    if(action>=0) Message("独立窗口操作已完成");
                }
            } else if(component=="OverlayListPopup") {
                if(skui::Button("打开三项列表")) skui::OpenDialog("standalone-list");
                if(skui::ListPopup("standalone-list",children,3)>=0) Message("列表操作已触发");
            } else if(component=="OverlayCascadingListPopup") {
                if(skui::Button("打开级联列表")) skui::OpenDialog("cascading-list");
                if(skui::ListPopup("cascading-list",actions,4)>=0) Message("级联列表操作已触发");
                skui::Caption("选择“排序方式”进入下一级，再用返回按钮回到上级。");
            } else if(component=="OverlayDropdownPreference") {
                skui::Section("短列表：按内容展开");
                if(skui::Dropdown("外观",&choice,choices,3,"三个选项，无需滚动")) Message("外观选项已更新");
                skui::Section("长列表：超过高度后滚动");
                char labels[18][32]; skui::ChoiceItem long_items[18];
                for(int i=0;i<18;++i) {
                    std::snprintf(labels[i],sizeof(labels[i]),"选项 %02d",i+1);
                    long_items[i]={labels[i]};
                }
                if(skui::Dropdown("更多选项",&long_choice,long_items,18)) Message("长列表选项已更新");
                skui::Space(6);skui::Caption("当前外观");skui::Text(choices[choice].label);
            } else if(component=="OverlaySpinnerPreference") {
                skui::Section("浮动选择");
                if(skui::Spinner("外观",&spinner_choice,advanced_choices,4,"图标、摘要与禁用状态")) Message("浮动选择已更新");
                skui::Section("对话框选择");
                if(skui::Spinner("外观（对话框）",&dialog_choice,advanced_choices,4,"同一组选项，不同呈现方式",true)) Message("对话框选择已更新");
                skui::Section("多选");
                const skui::ChoiceItem features[]={{"消息通知"},{"自动同步"},{"声音提醒"},{"后台刷新"}};
                skui::MultiSelect("启用的功能",multiple,features,4);
            }
            else if(component=="OverlayBottomSheet") {
                if(skui::Button("打开底部面板")) skui::OpenDialog("sample-sheet");
                if(skui::BeginBottomSheet("sample-sheet","选择操作",240)) {
                    skui::ArrowPreference("share","分享内容");
                    if(skui::Button("完成",skui::Tone::Primary,ImVec2(-1,0))) skui::CloseDialog();
                    skui::EndBottomSheet();
                }
            } else if(component=="OverlayDialog") {
                if(skui::Button("打开对话框")) skui::OpenDialog("sample-dialog");
                if(skui::BeginDialog("sample-dialog","确认保存？")) {
                    skui::Caption("取消会回到当前页面。");
                    if(skui::Button("取消",skui::Tone::Neutral)) skui::CloseDialog();
                    ImGui::SameLine();if(skui::Button("确认")) { skui::CloseDialog();Message("已保存"); }
                    skui::EndDialog();
                }
            } else {
                bool icon=component.find("Icon")!=std::string::npos;
                bool cascading=component.find("Cascading")!=std::string::npos;
                const skui::PopupItem plain[]={{0,"收藏",skui::Icon::Heart},{1,"分享",skui::Icon::Plus},
                    {2,"暂不可用",skui::Icon::Info,false,false,true}};
                int action=skui::DropdownMenu("操作菜单",cascading ? actions : plain,cascading ? 4 : 3,icon);
                if(action>=0) Message(action==0 ? "已收藏" : action==1 ? "已选择分享" : "排序方式已更新");
                skui::Caption(cascading ? "点“排序方式”打开二级菜单；返回上级保留菜单。" : "菜单包含图标和禁用项；选择操作后自动收起。");
            }
        }
        if(component!="Surface" && component!="Card") skui::EndCard();
    }
    void AccentColors() {
        const ImVec4 colors[] = {{.20f, .48f, 1, 1},
                                 {.49f, .36f, .91f, 1},
                                 {.06f, .64f, .48f, 1},
                                 {.96f, .43f, .24f, 1},
                                 {.87f, .27f, .49f, 1}};
        for (int i = 0; i < 5; ++i) {
            if (i)
                ImGui::SameLine();
            ImGui::PushID(i);
            bool pressed = false;
            bool selected=std::abs(theme.accent.x-colors[i].x)<.001f && std::abs(theme.accent.y-colors[i].y)<.001f && std::abs(theme.accent.z-colors[i].z)<.001f;
            skui::Swatch("accent", colors[i], selected, &pressed);
            if (pressed) {
                accent = i;
                theme.accent = colors[i];
            }
            ImGui::PopID();
        }
    }
    void Layouts() {
        skui::Section("页面结构");
        if (skui::BeginCard("layout-intro")) {
            skui::Space(6);
            skui::Text("让内容自然地组织在一起");
            skui::Caption("统一留白、圆角和对齐，适应不同窗口宽度。");
            skui::Space(5);
            skui::Badge(wide ? "当前：宽屏侧栏" : "当前：紧凑导航");
        }
        skui::EndCard();
        skui::Section("分组设置");
        if (skui::BeginCard("layout-group")) {
            skui::Toggle("展开详细内容", &show_details);
            skui::Collapsible("layout-details", show_details, [&] {
                skui::Divider();
                skui::ListItem("connection", "网络连接", "同一张卡片中组合多个列表项", "已连接", false);
                skui::ListItem("storage", "存储空间", nullptr, "24.8 GB", false);
            });
        }
        skui::EndCard();
        skui::Section("并排操作");
        if (skui::BeginCard("layout-actions")) {
            skui::Caption("操作按钮均分可用宽度");
            skui::Space(5);
            float w = (ImGui::GetContentRegionAvail().x - skui::Dp(10)) * .5f;
            if (skui::Button("收藏", skui::Tone::Primary, ImVec2(w, 0)))
                Message("已收藏这个布局");
            ImGui::SameLine();
            if (skui::Button("分享", skui::Tone::Neutral, ImVec2(w, 0)))
                Message("分享按钮已触发");
        }
        skui::EndCard();
        skui::Section("自适应布局");
        if (skui::BeginCard("responsive")) {
            skui::Toggle("宽屏预览", &wide, "切换后，底部导航变为左侧栏");
            skui::Caption("也可拖动窗口右下角调整大小，布局按可用宽度自动切换。");
            if(skui::ArrowPreference("adaptive","检查自适应与字体","动态字号、长翻译与自动换列")) router.Push("/adaptive");
        }
        skui::EndCard();
    }
    void Navigation() {
        skui::Section("面包屑 Breadcrumb");
        if (skui::BeginCard("breadcrumbs")) {
            const char *segments[] = {"首页", "文档", "组件", "导航示例"};
            if (skui::BreadcrumbBar("path", segments, 4, &breadcrumb)) Message("已选择路径节点");
        }
        skui::EndCard();
        skui::Section("页面导航");
        if (skui::BeginCard("nav-intro")) {
            skui::Space(4);
            skui::Text("进入详情，再返回这里");
            skui::Caption("页面参数、返回栈与控件状态是独立的。返回后仍然保留你的操作。");
            skui::Space(10);
            if (skui::Button("进入详情页面", skui::Tone::Primary, ImVec2(-1, 0)))
                router.Push("/detail", "从导航示例进入");
        }
        skui::EndCard();
        skui::Section("返回后保留状态");
        if (skui::BeginCard("nav-state")) {
            char count[64];
            std::snprintf(count, sizeof(count), "计数 %d  /  点击增加", clicks);
            if (skui::Button(count, skui::Tone::Neutral, ImVec2(-1, 0)))
                ++clicks;
            skui::Toggle("消息通知", &notifications);
            skui::Slider("音量", &volume, 0, 100, "%.0f %%");
        }
        skui::EndCard();
        skui::Section("导航行为");
        if (skui::BeginCard("nav-links")) {
            if (skui::ListItem("open-settings", "打开设置页面", "进入后可通过左上角返回"))
                router.Push("/settings");
            skui::Divider();
            skui::ListItem("stack", "当前返回栈", nullptr, router.CanGoBack() ? "可返回" : "根页面", false);
        }
        skui::EndCard();
    }
    void Detail(const skui::Route &route) {
        skui::Section("路由参数");
        if (skui::BeginCard("route-param")) {
            skui::Space(8);
            skui::Text(route.parameter.c_str());
            skui::Caption("这个内容由进入页面时传入的参数生成。");
            skui::Space(8);
        }
        skui::EndCard();
        skui::Section("可操作的详情");
        if (skui::BeginCard("detail-controls")) {
            skui::Toggle("消息通知", &notifications);
            skui::Slider("音量", &volume, 0, 100, "%.0f %%");
            skui::Space(6);
            if (skui::Button("保存并返回", skui::Tone::Primary, ImVec2(-1, 0))) {
                Message("已保存，返回上一页");
                router.Back();
            }
        }
        skui::EndCard();
        skui::Section("更多页面");
        if (skui::BeginCard("detail-next")) {
            if (skui::ListItem("nested", "再进入一层", "测试多级返回栈"))
                router.Push("/detail", "第二层详情");
        }
        skui::EndCard();
    }
    void Settings() {
        skui::Section("外观");
        if (skui::BeginCard("appearance")) {
            skui::Toggle("深色模式", &theme.dark, "修改后，组件与页面一起更新");
            skui::InspectLastItem("settings/dark");
            skui::Toggle("交互动画", &theme.animations);
            skui::InspectLastItem("settings/animations");
            skui::Toggle("触觉反馈", &theme.haptics);
            skui::InspectLastItem("settings/haptics");
            skui::Divider();
            skui::Space(10);
            skui::Text("强调色");
            AccentColors();
            skui::Slider("控件圆角",&theme.tokens.control_radius,0,28,"%.0f");
            skui::Slider("卡片圆角",&theme.tokens.card_radius,0,32,"%.0f");
            skui::Slider("文字缩放",&theme.tokens.text_scale,.8f,1.4f,"%.2f");
            skui::Caption("设计参数统一作用于对应控件；字号变化仍需检查布局。");
            skui::Space(4);
        }
        skui::EndCard();
        skui::Section("布局");
        if (skui::BeginCard("settings-layout")) {
            skui::Toggle("宽屏预览", &wide, "在同一窗口中预览侧栏布局");
        }
        skui::EndCard();
        skui::Section("设置保存与诊断");
        if (skui::BeginCard("foundation")) {
            if (preferences_path.empty()) {
                skui::Caption("启动时传入 --settings <文件路径> 可启用加载与退出保存。");
            } else if (skui::Button("保存当前设置")) save_requested=true;
            bool diagnostics=profiler.Enabled();
            if (skui::Toggle("性能诊断",&diagnostics,"固定 240 帧采样；关闭后停止计时并清空统计")) profiler.SetEnabled(diagnostics);
            if (diagnostics) {
                skui::DrawDiagnostics(profiler);
                if (skui::Button("清空采样",skui::Tone::Neutral)) profiler.Clear();
            }
        }
        skui::EndCard();
        skui::Section("开发工具");
        if(skui::BeginCard("developer-tools")) {
            if(skui::Toggle("交互检查",&developer_tools,"仅记录显式标记的控件；关闭后释放记录"))
                skui::SetItemInspectionEnabled(developer_tools);
            if(developer_tools) {
                auto playback=skui::GetAnimationPlayback();
                bool changed=skui::Toggle("暂停展示动画",&playback.paused);
                changed|=skui::Slider("动画速度",&playback.speed,.1f,2.f,"%.2fx");
                if(changed) skui::SetAnimationPlayback(playback);
                if(skui::Button("推进一帧",skui::Tone::Neutral)) skui::StepAnimations();
                skui::Caption("真实输入、惯性滚动和任务不暂停；标记范围见开发工具文档。");
                skui::Toggle("ImGui Metrics",&show_metrics);skui::Toggle("ImGui ID Stack",&show_ids);
            } else skui::SetAnimationPlayback({});
        }
        skui::EndCard();
        skui::Section("关于");
        if (skui::BeginCard("about")) {
            skui::ListItem("lib", "sk-imgui-ui", nullptr, "0.1.0", false);
            skui::Divider();
            skui::ListItem("renderer", "渲染接口", nullptr, "Vulkan", false);
            char fps[32];
            std::snprintf(fps, sizeof(fps), "%.0f FPS", ImGui::GetIO().Framerate);
            skui::ListItem("fps", "实时帧率", nullptr, fps, false);
            skui::Caption("基于 Dear ImGui 的 C++ 组件库。当前以原生悬浮界面运行。");
            skui::Space(6);
        }
        skui::EndCard();
    }
    void Initialize() {
        router.Register("/components", [this](const skui::Route &) { Components(); });
        router.Register("/layouts", [this](const skui::Route &) { Layouts(); });
        router.Register("/navigation", [this](const skui::Route &) { Navigation(); });
        router.Register("/settings", [this](const skui::Route &) { Settings(); });
        router.Register("/runtime", [this](const skui::Route &) { Runtime(); });
        router.Register("/adaptive", [this](const skui::Route &) { Adaptive(); });
        router.Register("/detail", [this](const skui::Route &route) { Detail(route); });
        router.Register("/catalogue", [this](const skui::Route &route) { Catalogue(route); });
        router.Register("/component-demo", [this](const skui::Route &route) { ComponentDemo(route); });
        router.Reset("/components");
        initialized = true;
    }
    void Draw(bool *running, ImVec2 display, float keyboard_height,skui::SafeInsets safe_insets) {
        if (!initialized)
            Initialize();
        PollData();
        scale = std::clamp(std::min(display.x, display.y) / 490.f, 1.f, 2.6f) * (wide ? .57f : 1.f);
        theme.scale = scale;
        skui::ApplyTheme(theme);
        ImGui::PushFont(nullptr, skui::TextSize(16));
        const skui::NavItem items[] = {{"组件", skui::Icon::Components},
                                       {"布局", skui::Icon::Layout},
                                       {"导航", skui::Icon::Navigation},
                                       {"设置", skui::Icon::Settings}};
        const char *paths[] = {"/components", "/layouts", "/navigation", "/settings"};
        const char *titles[] = {"组件", "布局", "导航", "设置"};
        const char *subtitles[] = {"每一次点击，都有真实反馈", "用统一的设计组织内容", "让页面之间自然连接",
                                   "把界面调成你喜欢的样子"};
        bool back = false, close = false;
        int previous = nav;
        bool detail = router.Current()->path == "/detail";
        skui::ShellOptions shell;
        shell.visible_bottom = keyboard_height > 0 ? display.y - keyboard_height : 0;
        shell.safe_insets=safe_insets;
        shell.title = detail ? "详情" : router.Current()->path == "/settings" ? "设置" : titles[nav];
        if(router.Current()->path=="/catalogue") shell.title="组件目录";
        if(router.Current()->path=="/component-demo") shell.title="组件示例";
        if(router.Current()->path=="/runtime") shell.title="任务与长列表";
        if(router.Current()->path=="/adaptive") shell.title="自适应与字体";
        shell.subtitle = detail ? "页面参数与返回栈" : subtitles[nav];
        shell.items = items;
        shell.item_count = 4;
        shell.selected = &nav;
        shell.can_go_back = router.CanGoBack();
        shell.back_requested = &back;
        shell.close_requested = &close;
        shell.content_includes_header = true;
        shell.sidebar_expanded = &sidebar_expanded;
        shell.initial_position = ImVec2(display.x * saved.position.x, display.y * saved.position.y);
        shell.initial_size = ImVec2(display.x * saved.size.x, std::min(display.y * saved.size.y, 1850.f));
        bool visible = skui::BeginShell(shell);
        window_pos = skui::ShellPosition();
        window_size = skui::ShellSize();
        // Never persist the temporary geometry used to avoid the keyboard.
        if (keyboard_height<=0 && display.x>0 && display.y>0) {
            saved.size=ImVec2(std::clamp(window_size.x/display.x,.1f,1.f),std::clamp(window_size.y/display.y,.1f,1.f));
            saved.position=ImVec2(std::clamp(window_pos.x/display.x,0.f,1-saved.size.x),std::clamp(window_pos.y/display.y,0.f,1-saved.size.y));
        }
        if (previous != nav)
            router.Reset(paths[nav]);
        if (close)
            *running = false;
        if (visible) {
            router_view.Draw(router,ImVec2(0,0),[&](const skui::Route &route) {
                const char *title=titles[nav], *subtitle=subtitles[nav];
                if(route.path=="/detail") {title="详情";subtitle="页面参数与返回栈";}
                else if(route.path=="/catalogue") {
                    title="组件目录";
                    if(route.parameter.size()==1 && route.parameter[0]>='0' && route.parameter[0]<='6')
                        title=categories[route.parameter[0]-'0'].title;
                } else if(route.path=="/component-demo") { title="组件示例";subtitle=route.parameter.c_str(); }
                else if(route.path=="/runtime") {title="任务与长列表";subtitle="后台准备、可取消、按需绘制";}
                else if(route.path=="/adaptive") {title="自适应与字体";subtitle="字号、长翻译、宽度与键盘";}
                back=skui::TopAppBar("page-header",title,subtitle,router.CanGoBack());
            });
        }
        if(back) router.Back();
        if (progress_running) {
            if(router.Current()->path=="/component-demo") skui::RequestAnimationFrame();
            progress = std::min(1.f, progress + ImGui::GetIO().DeltaTime * .12f);
            if (progress >= 1) {
                progress_running = false;
                Message("演示任务已完成");
            }
        }
        skui::EndShell();
        {
            const skui::Icon icons[] = {skui::Icon::Heart, skui::Icon::Search, skui::Icon::Settings};
            const char *labels[] = {"收藏", "搜索", "设置"};
            auto position = ImVec2(window_pos.x + skui::Dp(20), window_pos.y + window_size.y - skui::Dp(140));
            int action = skui::FloatingToolbar("gallery-toolbar", icons, labels, 3, position, floating_tools);
            if (action >= 0) Message(labels[action]);
            if (skui::FloatingActionButton("gallery-fab", skui::Icon::Plus,
                ImVec2(window_pos.x + window_size.x - skui::Dp(76), position.y), floating_tools)) {
                ++clicks; Message("悬浮按钮已触发");
            }
        }
        snackbar.Draw(ImVec2(window_pos.x + skui::Dp(24), window_pos.y + window_size.y - skui::Dp(132)),
                      window_size.x - skui::Dp(48));
        ImGui::PopFont();
        if(developer_tools) {
            ImGui::SetNextWindowSize(ImVec2(460,420),ImGuiCond_FirstUseEver);
            bool open=true;
            if(ImGui::Begin("Interaction Inspector",&open)) skui::DrawInteractionInspector();
            ImGui::End();
            if(!open) {developer_tools=false;skui::SetItemInspectionEnabled(false);skui::SetAnimationPlayback({});}
            if(show_metrics) ImGui::ShowMetricsWindow(&show_metrics);
            if(show_ids) ImGui::ShowIDStackToolWindow(&show_ids);
        }
    }
};
Gallery app;
} // namespace
void Draw(bool *running, ImVec2 display_size, float keyboard_height,skui::SafeInsets safe_insets) { app.Draw(running, display_size, keyboard_height,safe_insets); }
void Notify(const char *message) { app.Message(message); }
float FontScale() { return 35.2f; }
skui::FrameProfiler &Profiler() { return app.profiler; }
void ConfigurePreferences(const char *path) { app.ConfigurePreferences(path); }
void FlushPreferences(bool exiting) { app.FlushPreferences(exiting); }
void SetWakeHandler(void (*handler)(void *),void *user) {app.data_loader.SetWakeHandler(handler,user);}
void Shutdown() {app.data_loader.Shutdown();app.data_loader.SetWakeHandler(nullptr);}
} // namespace gallery
