cmake_minimum_required(VERSION 3.20)

project(cbounded VERSION 0.1.0 LANGUAGES C)

# Builds go in ./build (cmake -B build); refuse in-source builds.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -B build")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Keep assert() enabled in RelWithDebInfo; only Release defines NDEBUG.
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO
       "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Clang REQUIRED CONFIG)

# All sources live flat in src/; re-glob automatically when files are added.
file(GLOB CBOUNDED_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)

add_executable(cbounded ${CBOUNDED_SOURCES})

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

target_include_directories(cbounded PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(cbounded PRIVATE libclang)

install(TARGETS cbounded)
