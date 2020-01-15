cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(rattle VERSION 0.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -fsanitize=undefined")

add_executable(rattle src/rattle.c)
target_include_directories(rattle PUBLIC
  "${PROJECT_BINARY_DIR}"
  )
configure_file(config.h.in config.h)

add_library(runtime STATIC runtime/runtime.c)
