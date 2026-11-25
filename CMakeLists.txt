cmake_minimum_required(VERSION 3.21)
project(miuix-imgui VERSION 0.1.0 LANGUAGES C CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SKUI_BUILD_ANDROID_DEMO "Build the native Android floating gallery" ${ANDROID})
option(SKUI_BUILD_DESKTOP_DEMO "Build the SDL2/OpenGL3 gallery (POSIX host)" OFF)
option(SKUI_BUILD_TESTS "Build host-side component and navigation tests" OFF)
option(SKUI_BUILD_BENCHMARKS "Build headless CPU performance probes" OFF)
option(SKUI_INSTALL "Generate library installation and package exports" ON)
set(SKUI_IMGUI_TARGET "" CACHE STRING "Existing host ImGui target; empty builds the bundled ImGui")
set(SKUI_PACKAGE_FREETYPE OFF)

if(SKUI_IMGUI_TARGET)
    if(NOT TARGET ${SKUI_IMGUI_TARGET})
        message(FATAL_ERROR "SKUI_IMGUI_TARGET must name an existing CMake target")
    endif()
    set(SKUI_IMGUI ${SKUI_IMGUI_TARGET})
else()
    add_library(skui-imgui STATIC
        src/ImGui/imgui.cpp src/ImGui/imgui_draw.cpp
        src/ImGui/imgui_tables.cpp src/ImGui/imgui_widgets.cpp)
    target_include_directories(skui-imgui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/skui/imgui>)
    target_compile_features(skui-imgui PUBLIC cxx_std_17)
    set(SKUI_IMGUI skui-imgui)
endif()

add_library(skui-ui STATIC src/skui/skui.cpp src/skui/components.cpp src/skui/layout.cpp src/skui/overlay.cpp src/skui/navigation.cpp
    src/skui/window.cpp src/skui/color.cpp src/skui/glass.cpp src/skui/theme.cpp src/skui/diagnostics.cpp src/skui/runtime.cpp src/skui/devtools.cpp)
add_library(skui::ui ALIAS skui-ui)
target_include_directories(skui-ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(SKUI_IMGUI_TARGET)
    target_link_libraries(skui-ui PUBLIC $<BUILD_INTERFACE:${SKUI_IMGUI}> $<INSTALL_INTERFACE:skui::imgui-host>)
else()
    target_link_libraries(skui-ui PUBLIC skui-imgui)
endif()
target_compile_features(skui-ui PUBLIC cxx_std_17)

add_library(skui-navigation INTERFACE)
add_library(skui::navigation ALIAS skui-navigation)
target_include_directories(skui-navigation INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(skui-navigation INTERFACE cxx_std_17)

if(SKUI_BUILD_DESKTOP_DEMO)
    if(ANDROID OR NOT UNIX)
        message(FATAL_ERROR "The desktop gallery currently requires a POSIX desktop host; the UI library has no such dependency")
    endif()
    find_package(SDL2 2.0.18 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(skui-gallery
        examples/desktop/main.cpp examples/gallery/gallery.cpp
        examples/gallery/preferences.cpp examples/gallery/data_model.cpp
        src/ImGui/backends/imgui_impl_sdl2.cpp src/ImGui/backends/imgui_impl_opengl3.cpp)
    target_include_directories(skui-gallery PRIVATE examples/gallery include/ImGui/backends)
    target_link_libraries(skui-gallery PRIVATE skui::ui SDL2::SDL2 OpenGL::GL Threads::Threads)
endif()

if(SKUI_BUILD_ANDROID_DEMO)
    if(NOT ANDROID)
        message(FATAL_ERROR "Use the Android NDK CMake toolchain for the Android demo")
    endif()
    find_package(Threads REQUIRED)
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
        target_link_libraries(skui-imgui PUBLIC $<BUILD_INTERFACE:freetype> $<INSTALL_INTERFACE:Freetype::Freetype> z)
        set(SKUI_PACKAGE_FREETYPE ON)
    endif()
    add_executable(sk-imgui-ui
        src/main.cpp src/Android_draw/draw_Gui.cpp examples/gallery/gallery.cpp examples/gallery/preferences.cpp examples/gallery/data_model.cpp
        src/Android_touch/TouchHelperA.cpp
        src/Android_Graphics/GraphicsManager.cpp src/Android_Graphics/OpenGLGraphics.cpp
        src/Android_Graphics/VulkanGraphics.cpp src/Android_Graphics/vulkan_wrapper.cpp
        src/Android_my_imgui/AndroidImgui.cpp src/Android_my_imgui/my_imgui.cpp
        src/Android_my_imgui/my_imgui_impl_android.cpp
        src/Android_my_imgui/AndroidIme.cpp src/Android_my_imgui/AndroidImeApk.cpp
        src/Android_my_imgui/AndroidHaptics.cpp
        src/Android_my_imgui/FrameWake.cpp
        "${skui_ime_generated}/skui_ime_dex.h"
        src/ImGui/backends/imgui_impl_opengl3.cpp src/ImGui/backends/imgui_impl_vulkan.cpp
        src/My_Utils/stb_image/stb_image.cpp)
    target_include_directories(sk-imgui-ui PRIVATE
        "${skui_ime_generated}"
        examples/gallery include/Android_draw include/Android_Graphics
        include/Android_my_imgui include/Android_touch include/My_Utils
        include/My_Utils/stb_image include/ImGui/backends include/ImGui/misc/freetype)
    target_compile_definitions(sk-imgui-ui PRIVATE VK_USE_PLATFORM_ANDROID_KHR IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    target_link_libraries(sk-imgui-ui PRIVATE skui::ui skui::navigation Threads::Threads log android EGL GLESv3 z)
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

if(SKUI_INSTALL)
    set_target_properties(skui-ui PROPERTIES EXPORT_NAME ui)
    set_target_properties(skui-navigation PROPERTIES EXPORT_NAME navigation)
    install(TARGETS skui-ui EXPORT skuiTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS skui-navigation EXPORT skuiNavigationTargets)
    if(NOT SKUI_IMGUI_TARGET)
        set_target_properties(skui-imgui PROPERTIES EXPORT_NAME imgui)
        install(TARGETS skui-imgui EXPORT skuiTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        install(FILES include/ImGui/imgui.h include/ImGui/imgui_internal.h include/ImGui/imconfig.h
            include/ImGui/imstb_rectpack.h include/ImGui/imstb_textedit.h include/ImGui/imstb_truetype.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/skui/imgui)
    endif()
    install(DIRECTORY include/skui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
    install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/miuix-imgui)
    install(DIRECTORY LICENSES DESTINATION ${CMAKE_INSTALL_DATADIR}/miuix-imgui)
    set(skui_package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/skui)
    configure_package_config_file(cmake/skuiConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/skuiConfig.cmake
        INSTALL_DESTINATION ${skui_package_dir})
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/skuiConfigVersion.cmake
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
    install(EXPORT skuiTargets NAMESPACE skui:: DESTINATION ${skui_package_dir})
    install(EXPORT skuiNavigationTargets NAMESPACE skui:: DESTINATION ${skui_package_dir})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/skuiConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/skuiConfigVersion.cmake
        DESTINATION ${skui_package_dir})
endif()

if(SKUI_BUILD_BENCHMARKS)
    add_executable(skui-foundation-benchmark benchmarks/foundations.cpp)
    target_link_libraries(skui-foundation-benchmark PRIVATE skui::ui)
endif()

if(SKUI_BUILD_TESTS)
    find_package(Threads REQUIRED)
    enable_testing()
    add_executable(skui-tests tests/skui_tests.cpp)
    target_link_libraries(skui-tests PRIVATE skui::ui skui::navigation)
    add_test(NAME skui-tests COMMAND skui-tests)
    add_executable(skui-advanced-tests tests/advanced_tests.cpp)
    target_link_libraries(skui-advanced-tests PRIVATE skui::ui)
    add_test(NAME skui-advanced-tests COMMAND skui-advanced-tests)
    add_executable(skui-foundation-tests tests/foundation_tests.cpp)
    target_link_libraries(skui-foundation-tests PRIVATE skui::ui)
    if(UNIX)
        target_sources(skui-foundation-tests PRIVATE examples/gallery/preferences.cpp)
        target_include_directories(skui-foundation-tests PRIVATE examples/gallery)
    endif()
    add_test(NAME skui-foundation-tests COMMAND skui-foundation-tests)
    add_executable(skui-runtime-tests tests/runtime_tests.cpp examples/gallery/data_model.cpp)
    target_include_directories(skui-runtime-tests PRIVATE examples/gallery)
    target_link_libraries(skui-runtime-tests PRIVATE skui::ui Threads::Threads)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(skui-runtime-tests PRIVATE src/Android_my_imgui/FrameWake.cpp)
        target_include_directories(skui-runtime-tests PRIVATE include/Android_my_imgui)
        target_compile_definitions(skui-runtime-tests PRIVATE SKUI_TEST_NATIVE_WAKE)
    endif()
    add_test(NAME skui-runtime-tests COMMAND skui-runtime-tests)
    add_executable(skui-devtools-tests tests/devtools_tests.cpp)
    target_link_libraries(skui-devtools-tests PRIVATE skui::ui)
    add_test(NAME skui-devtools-tests COMMAND skui-devtools-tests)
    add_executable(skui-keyboard-tests tests/keyboard_tests.cpp)
    target_link_libraries(skui-keyboard-tests PRIVATE skui::ui)
    add_test(NAME skui-keyboard-tests COMMAND skui-keyboard-tests)
endif()
