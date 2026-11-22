cmake_minimum_required(VERSION 3.20)

project(cbounded VERSION 0.1.0 LANGUAGES C)

#Builds go in./ build(cmake - B build); refuse in - source builds.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed. Use: cmake -B build")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

#Keep assert() enabled in RelWithDebInfo; only Release defines NDEBUG.
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO
       "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Clang REQUIRED CONFIG)

option(CBOUNDED_SANITIZE "Build everything with ASan + UBSan" OFF)
if(CBOUNDED_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# All sources live flat in src/; re-glob automatically when files are added.
# Everything except main.c goes into a library so tests can link against it.
file(GLOB CBOUNDED_CORE_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM CBOUNDED_CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/main.c)

add_library(cbounded_core STATIC ${CBOUNDED_CORE_SOURCES})
add_executable(cbounded ${PROJECT_SOURCE_DIR}/src/main.c)

foreach(target cbounded_core cbounded)
  set_target_properties(
    ${target}
    PROPERTIES C_STANDARD 17
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)

  target_compile_options(
    ${target}
    PRIVATE -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wstrict-prototypes
            -Wformat=2
            -Wvla)
endforeach()

# c17 hides POSIX interfaces such as getline() and strdup() without this.
# PUBLIC so anything linking cbounded_core (main, tests) gets it too.
target_compile_definitions(cbounded_core PUBLIC _POSIX_C_SOURCE=200809L)
target_include_directories(cbounded_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(cbounded_core PUBLIC libclang)

target_link_libraries(cbounded PRIVATE cbounded_core)

install(TARGETS cbounded)

enable_testing()
add_subdirectory(tests)
