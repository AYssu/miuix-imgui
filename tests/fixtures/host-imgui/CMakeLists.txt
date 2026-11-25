cmake_minimum_required(VERSION 3.21)
project(skui-host-package-fixture LANGUAGES C CXX)
include(GNUInstallDirs)
if(NOT SKUI_SOURCE_DIR)
    message(FATAL_ERROR "Pass SKUI_SOURCE_DIR for the matching ImGui source fixture")
endif()
add_library(host-imgui STATIC
    ${SKUI_SOURCE_DIR}/src/ImGui/imgui.cpp ${SKUI_SOURCE_DIR}/src/ImGui/imgui_draw.cpp
    ${SKUI_SOURCE_DIR}/src/ImGui/imgui_tables.cpp ${SKUI_SOURCE_DIR}/src/ImGui/imgui_widgets.cpp)
target_compile_features(host-imgui PUBLIC cxx_std_17)
target_include_directories(host-imgui PUBLIC
    $<BUILD_INTERFACE:${SKUI_SOURCE_DIR}/include/ImGui>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/host-imgui>)
set_target_properties(host-imgui PROPERTIES EXPORT_NAME imgui)
install(TARGETS host-imgui EXPORT hostImGuiTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT hostImGuiTargets NAMESPACE host:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/host-imgui)
install(FILES ${SKUI_SOURCE_DIR}/include/ImGui/imgui.h ${SKUI_SOURCE_DIR}/include/ImGui/imgui_internal.h
    ${SKUI_SOURCE_DIR}/include/ImGui/imconfig.h ${SKUI_SOURCE_DIR}/include/ImGui/imstb_rectpack.h
    ${SKUI_SOURCE_DIR}/include/ImGui/imstb_textedit.h ${SKUI_SOURCE_DIR}/include/ImGui/imstb_truetype.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/host-imgui)
set(SKUI_IMGUI_TARGET host-imgui CACHE STRING "" FORCE)
set(SKUI_BUILD_ANDROID_DEMO OFF CACHE BOOL "" FORCE)
set(SKUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${SKUI_SOURCE_DIR} skui)
