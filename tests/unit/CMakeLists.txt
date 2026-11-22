# Every test_*.c in this folder becomes its own executable + CTest test.
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)

foreach(src ${UNIT_TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  set_target_properties(
    ${name}
    PROPERTIES C_STANDARD 17
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  target_link_libraries(${name} PRIVATE cbounded_core)
  add_test(NAME ${name} COMMAND ${name})
  # Safety net on top of the per-case alarm() inside the runner.
  set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
