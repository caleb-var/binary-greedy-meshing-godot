cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED GODOT_CPP_PATH)
  message(FATAL_ERROR "Set GODOT_CPP_PATH to your godot-cpp checkout (e.g. -DGODOT_CPP_PATH=~/godot-cpp)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${GODOT_CPP_PATH} godot-cpp)

add_library(binary_menu_extension SHARED
  src/binary_mesher_gdextension.cpp
  src/register_types.cpp
)

target_include_directories(binary_menu_extension PRIVATE src)
target_link_libraries(binary_menu_extension PRIVATE godot::cpp)

set_target_properties(binary_menu_extension PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/godot/bin
)
