cmake_minimum_required(VERSION 3.10)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Coverage")

set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "List of supported configurations.")
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
if(NOT CMAKE_BUILD_TYPE)
  message("Defaulting to Release build.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}." FORCE)
endif()

# set the project name and version
project(rattle VERSION 0.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -fsanitize=undefined")
set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS} -O0 -g -coverage")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
add_executable(rattle src/rattle.c)
target_include_directories(rattle PUBLIC
  "${PROJECT_BINARY_DIR}"
  )
configure_file(config.h.in config.h)

# Sort out runtime
add_subdirectory(./runtime)
