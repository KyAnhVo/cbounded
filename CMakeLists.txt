cmake_minimum_required(VERSION 3.20)

project(cbounded VERSION 0.1.0 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Keep assert() enabled in RelWithDebInfo; only Release defines NDEBUG.
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO
       "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Clang REQUIRED CONFIG)

add_executable(cbounded src/main.c)

set_target_properties(
  cbounded
  PROPERTIES C_STANDARD 17
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

# c17 hides POSIX interfaces such as getline() and strdup() without this.
target_compile_definitions(cbounded PRIVATE _POSIX_C_SOURCE=200809L)

target_compile_options(
  cbounded
  PRIVATE -Wall
          -Wextra
          -Wpedantic
          -Wconversion
          -Wshadow
          -Wstrict-prototypes
          -Wformat=2
          -Wvla)

target_include_directories(cbounded PRIVATE include)

target_link_libraries(cbounded PRIVATE libclang)

install(TARGETS cbounded)
