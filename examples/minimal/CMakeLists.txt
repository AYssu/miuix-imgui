cmake_minimum_required(VERSION 3.21)
project(skui-consumer LANGUAGES C CXX)
# This is a separate consumer, exercising the same integration as another repo.
set(SKUI_BUILD_ANDROID_DEMO OFF CACHE BOOL "" FORCE)
add_subdirectory(../.. skui)
add_executable(skui-consumer main.cpp)
target_link_libraries(skui-consumer PRIVATE skui::ui skui::navigation)
