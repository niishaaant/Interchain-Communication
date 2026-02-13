# CMakeLists.txt (top-level)
cmake_minimum_required(VERSION 3.16)

project(Blockchain-Comm-Sim LANGUAGES CXX)

# Require C++20 (needed for std::jthread, chrono, etc.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -g)
endif()

# Define include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Option A: Build everything as a single executable
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(ibc-sim ${SRC_FILES})

# Option B (preferred for large projects): Build modular libs
# add_subdirectory(src/core)
# add_subdirectory(src/consensus)
# add_subdirectory(src/ibc)
# add_subdirectory(src/net)
# add_subdirectory(src/util)
# add_subdirectory(src/sim)
# add_executable(ibc-sim src/main.cpp)
# target_link_libraries(ibc-sim PRIVATE core consensus ibc net util sim)
