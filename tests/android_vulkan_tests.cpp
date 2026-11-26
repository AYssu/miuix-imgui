// Device-only recovery regression. Intercepts Vulkan result codes around real
// GPU submissions; an unchanged-size OUT_OF_DATE must recreate the swapchain.
#include "VulkanGraphics.h"
#include "native_surface/ANativeWindowCreator.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <dlfcn.h>

#define CHECK(x) do {if (!(x)) {std::fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}} while (false)
namespace {
PFN_vkAcquireNextImageKHR real_acquire;
PFN_vkQueuePresentKHR real_present;
PFN_vkCreateSwapchainKHR real_create;
bool acquire_outdated=false,acquire_suboptimal=false,present_outdated=false,present_suboptimal=false;
int presents=0,swapchains=0;
VKAPI_ATTR VkResult VKAPI_CALL Acquire(VkDevice device,VkSwapchainKHR swapchain,uint64_t timeout,
                                     VkSemaphore semaphore,VkFence fence,uint32_t *index) {
    if (acquire_outdated) {acquire_outdated=false;return VK_ERROR_OUT_OF_DATE_KHR;}
    auto result=real_acquire(device,swapchain,timeout,semaphore,fence,index);
    if ((result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) && acquire_suboptimal) {acquire_suboptimal=false;return VK_SUBOPTIMAL_KHR;}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL Present(VkQueue queue,const VkPresentInfoKHR *info) {
    auto result=real_present(queue,info);++presents;
    if (result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) {
        if (present_outdated) {present_outdated=false;return VK_ERROR_OUT_OF_DATE_KHR;}
        if (present_suboptimal) {present_suboptimal=false;return VK_SUBOPTIMAL_KHR;}
    }
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice device,const VkSwapchainCreateInfoKHR *info,
                                    const VkAllocationCallbacks *allocator,VkSwapchainKHR *swapchain) {
    ++swapchains;return real_create(device,info,allocator,swapchain);
}
}
int main() {
    auto display=android::ANativeWindowCreator::GetDisplayInfo();
    int extent=std::max(display.width,display.height);CHECK(extent>0);
    auto *window=android::ANativeWindowCreator::Create("skui-vulkan-recovery-test",extent,extent,false);
    CHECK(window);
    VulkanGraphics graphics;CHECK(graphics.Init_Render(window,extent,extent));
    auto &io=ImGui::GetIO();io.Fonts->AddFontDefault();
    real_acquire=vkAcquireNextImageKHR;real_present=vkQueuePresentKHR;
    vkAcquireNextImageKHR=Acquire;vkQueuePresentKHR=Present;
    void *library=dlopen("libvulkan.so",RTLD_NOW);CHECK(library);
    real_create=reinterpret_cast<PFN_vkCreateSwapchainKHR>(dlsym(library,"vkCreateSwapchainKHR"));
    // ImGui 1.93 snapshots its loader into the renderer context at Init. Set
    // this backend hook before reinitializing, not only on the global loader.
    ImGui_ImplVulkan_Shutdown();
    CHECK(ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_1,[](const char *name,void *handle)->PFN_vkVoidFunction {
        if (std::strcmp(name,"vkCreateSwapchainKHR")==0) return reinterpret_cast<PFN_vkVoidFunction>(Create);
        return reinterpret_cast<PFN_vkVoidFunction>(dlsym(handle,name));
    },library));
    graphics.Setup();
    auto frame=[&] {
        io.DisplaySize=ImVec2(display.width,display.height);
        graphics.NewFrame();
        ImGui::SetNextWindowPos(ImVec2(30,40));ImGui::SetNextWindowSize(ImVec2(380,180));
        ImGui::Begin("Recovery regression",nullptr,ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Real Vulkan presents: %d",presents);ImGui::Button("Recovery works");
        ImGui::End();graphics.EndFrame();
    };
    for (int i=0;i<3;++i) frame();
    CHECK(presents>=3);
    auto recover=[&](bool &fault,const char *label,bool acquires_image) {
        int before=swapchains,before_present=presents;
        fault=true;frame();CHECK(!fault);
        if (acquires_image) CHECK(presents==before_present+1);
        else CHECK(presents==before_present);
        frame();CHECK(swapchains>before);
        CHECK(ANativeWindow_getWidth(window)==extent && ANativeWindow_getHeight(window)==extent);
        before_present=presents;
        for (int i=0;i<3;++i) frame();
        CHECK(presents==before_present+3);
        std::printf("PASS Vulkan recovery: %s, unchanged %dx%d surface, subsequent frames presented\n",label,extent,extent);
    };
    recover(acquire_outdated,"acquire OUT_OF_DATE",false);
    recover(present_outdated,"present OUT_OF_DATE",true);
    for (auto *fault : {&acquire_suboptimal,&present_suboptimal}) {
        int before=swapchains,before_present=presents;
        *fault=true;frame();CHECK(!*fault);
        frame();frame();
        CHECK(presents==before_present+3 && swapchains==before);
    }
    std::puts("PASS Vulkan SUBOPTIMAL: acquired images presented, no perpetual swapchain rebuilds");
    vkAcquireNextImageKHR=real_acquire;vkQueuePresentKHR=real_present;
    graphics.Shutdown();android::ANativeWindowCreator::Destroy(window);
    dlclose(library);
}
