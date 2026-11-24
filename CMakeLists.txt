cmake_minimum_required(VERSION 3.21)
project(miuix-imgui VERSION 0.1.0 LANGUAGES C CXX)

option(SKUI_BUILD_ANDROID_DEMO "Build the native Android floating gallery" ${ANDROID})
option(SKUI_BUILD_TESTS "Build host-side component and navigation tests" OFF)
set(SKUI_IMGUI_TARGET "" CACHE STRING "Existing host ImGui target; empty builds the bundled ImGui")

if(SKUI_IMGUI_TARGET)
    if(NOT TARGET ${SKUI_IMGUI_TARGET})
        message(FATAL_ERROR "SKUI_IMGUI_TARGET must name an existing CMake target")
    endif()
    set(SKUI_IMGUI ${SKUI_IMGUI_TARGET})
else()
    add_library(skui-imgui STATIC
        src/ImGui/imgui.cpp src/ImGui/imgui_draw.cpp
        src/ImGui/imgui_tables.cpp src/ImGui/imgui_widgets.cpp)
    target_include_directories(skui-imgui PUBLIC include/ImGui)
    target_compile_features(skui-imgui PUBLIC cxx_std_17)
    set(SKUI_IMGUI skui-imgui)
endif()

add_library(skui-ui STATIC src/skui/skui.cpp src/skui/components.cpp src/skui/layout.cpp src/skui/overlay.cpp src/skui/navigation.cpp
    src/skui/window.cpp src/skui/color.cpp src/skui/glass.cpp)
add_library(skui::ui ALIAS skui-ui)
target_include_directories(skui-ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(skui-ui PUBLIC ${SKUI_IMGUI})
target_compile_features(skui-ui PUBLIC cxx_std_17)

add_library(skui-navigation INTERFACE)
add_library(skui::navigation ALIAS skui-navigation)
target_include_directories(skui-navigation INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(skui-navigation INTERFACE cxx_std_17)

if(SKUI_BUILD_ANDROID_DEMO)
    if(NOT ANDROID)
        message(FATAL_ERROR "Use the Android NDK CMake toolchain for the Android demo")
    endif()
    find_package(Java REQUIRED COMPONENTS Development)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(SKUI_ANDROID_SDK "$ENV{ANDROID_SDK_ROOT}" CACHE PATH "Android SDK for the embedded system IME bridge")
    if(NOT SKUI_ANDROID_SDK)
        set(SKUI_ANDROID_SDK "$ENV{HOME}/Android/Sdk")
    endif()
    set(SKUI_ANDROID_PLATFORM "36" CACHE STRING "SDK platform used to compile the IME helper")
    set(SKUI_ANDROID_BUILD_TOOLS "36.0.0" CACHE STRING "SDK build-tools used for DEX/APK generation")
    set(skui_android_jar "${SKUI_ANDROID_SDK}/platforms/android-${SKUI_ANDROID_PLATFORM}/android.jar")
    set(skui_d8 "${SKUI_ANDROID_SDK}/build-tools/${SKUI_ANDROID_BUILD_TOOLS}/d8")
    if(NOT EXISTS "${skui_android_jar}" OR NOT EXISTS "${skui_d8}")
        message(FATAL_ERROR "Install platforms;android-${SKUI_ANDROID_PLATFORM} and build-tools;${SKUI_ANDROID_BUILD_TOOLS}; set SKUI_ANDROID_SDK")
    endif()
    set(skui_ime_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/ime")
    add_custom_command(OUTPUT "${skui_ime_generated}/skui_ime_dex.h"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-ime.py"
            --javac "${Java_JAVAC_EXECUTABLE}" --d8 "${skui_d8}"
            --android-jar "${skui_android_jar}"
            --source "${CMAKE_CURRENT_SOURCE_DIR}/android/ime/skui/android/NativeIme.java"
            --activity "${CMAKE_CURRENT_SOURCE_DIR}/android/ime/skui/android/ImeActivity.java"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/android/ime/AndroidManifest.xml"
            --output "${skui_ime_generated}"
        DEPENDS scripts/build-ime.py android/ime/skui/android/NativeIme.java
            android/ime/skui/android/ImeActivity.java android/ime/AndroidManifest.xml
        VERBATIM)
    if(NOT SKUI_IMGUI_TARGET)
        # Build the same reviewed dependency for every ABI, rather than shipping
        # opaque archives from the original template. Host-only builds stay offline.
        include(FetchContent)
        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()
        foreach(dependency ZLIB BZIP2 PNG HARFBUZZ BROTLI)
            set(FT_DISABLE_${dependency} ON CACHE BOOL "Disable optional FreeType dependency" FORCE)
        endforeach()
        FetchContent_Declare(skui_freetype
            URL https://codeload.github.com/freetype/freetype/tar.gz/refs/tags/VER-2-13-3
            URL_HASH SHA256=bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103)
        FetchContent_MakeAvailable(skui_freetype)
        target_compile_definitions(skui-imgui PUBLIC IMGUI_ENABLE_FREETYPE)
        target_include_directories(skui-imgui PRIVATE include/ImGui/misc/freetype)
        target_sources(skui-imgui PRIVATE src/ImGui/misc/freetype/imgui_freetype.cpp)
        target_link_libraries(skui-imgui PUBLIC freetype z)
    endif()
    add_executable(sk-imgui-ui
        src/main.cpp src/Android_draw/draw_Gui.cpp examples/gallery/gallery.cpp
        src/Android_touch/TouchHelperA.cpp
        src/Android_Graphics/GraphicsManager.cpp src/Android_Graphics/OpenGLGraphics.cpp
        src/Android_Graphics/VulkanGraphics.cpp src/Android_Graphics/vulkan_wrapper.cpp
        src/Android_my_imgui/AndroidImgui.cpp src/Android_my_imgui/my_imgui.cpp
        src/Android_my_imgui/my_imgui_impl_android.cpp
        src/Android_my_imgui/AndroidIme.cpp src/Android_my_imgui/AndroidImeApk.cpp
        src/Android_my_imgui/AndroidHaptics.cpp
        "${skui_ime_generated}/skui_ime_dex.h"
        src/ImGui/backends/imgui_impl_opengl3.cpp src/ImGui/backends/imgui_impl_vulkan.cpp
        src/My_Utils/stb_image/stb_image.cpp)
    target_include_directories(sk-imgui-ui PRIVATE
        "${skui_ime_generated}"
        examples/gallery include/Android_draw include/Android_Graphics
        include/Android_my_imgui include/Android_touch include/My_Utils
        include/My_Utils/stb_image include/ImGui/backends include/ImGui/misc/freetype)
    target_compile_definitions(sk-imgui-ui PRIVATE VK_USE_PLATFORM_ANDROID_KHR IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    target_link_libraries(sk-imgui-ui PRIVATE skui::ui skui::navigation log android EGL GLESv3 z)
    target_link_options(sk-imgui-ui PRIVATE "-Wl,-z,max-page-size=16384")
    # Explicit device regression target; excluded from the shipping build.
    add_executable(skui-ime-tests EXCLUDE_FROM_ALL tests/android_ime_tests.cpp
        src/Android_my_imgui/AndroidIme.cpp src/Android_my_imgui/AndroidImeApk.cpp
        "${skui_ime_generated}/skui_ime_dex.h")
    target_include_directories(skui-ime-tests PRIVATE include include/ImGui
        include/Android_my_imgui "${skui_ime_generated}")
    target_compile_features(skui-ime-tests PRIVATE cxx_std_17)
    target_link_options(skui-ime-tests PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(SKUI_BUILD_TESTS)
    enable_testing()
    add_executable(skui-tests tests/skui_tests.cpp)
    target_link_libraries(skui-tests PRIVATE skui::ui skui::navigation)
    add_test(NAME skui-tests COMMAND skui-tests)
    add_executable(skui-advanced-tests tests/advanced_tests.cpp)
    target_link_libraries(skui-advanced-tests PRIVATE skui::ui)
    add_test(NAME skui-advanced-tests COMMAND skui-advanced-tests)
endif()
