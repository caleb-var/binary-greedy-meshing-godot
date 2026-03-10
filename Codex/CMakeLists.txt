cmake_minimum_required(VERSION 3.21)
project(TinyVoxelDDGI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(voxcore
    src/VoxelChunk.cpp
    src/Generator.cpp
    src/Light.cpp
    src/DDGICascade.cpp
    src/Mesh.cpp
    src/BinaryGreedyMesher.cpp
    src/ChunkCodec.cpp
    src/SoftwareRenderer.cpp
    src/GodotInterop.cpp
)

target_include_directories(voxcore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(voxcore
    PUBLIC
        VOX_CHUNK_SIZE=32
)

target_compile_options(voxcore
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/O2 /W4 /permissive- /EHsc>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3 -Wall -Wextra -Wpedantic>
)

add_executable(vox_demo app/main.cpp)
target_link_libraries(vox_demo PRIVATE voxcore)

include(CTest)
if(BUILD_TESTING)
    add_executable(vox_tests tests/roundtrip_tests.cpp)
    target_link_libraries(vox_tests PRIVATE voxcore)
    add_test(NAME vox_tests COMMAND vox_tests)
endif()
