add_executable(ultragravity_tests
  main.cpp
  vehicle_file_test.cpp
  standstill_test.cpp
  vehicle_step_test.cpp
  straight_line_test.cpp
  drivetrain_test.cpp
  cornering_test.cpp
  traction_test.cpp
  animation_test.cpp
  aero_test.cpp
  input_smoothing_test.cpp
  telemetry_test.cpp
)
# doctest lives here, nlohmann json in the Unreal module.
target_include_directories(ultragravity_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party
  ${PROJECT_SOURCE_DIR}/../Source/UltraGravity/ThirdParty)
target_link_libraries(ultragravity_tests PRIVATE ultragravity::test_world)
target_compile_definitions(ultragravity_tests PRIVATE
  UG_VEHICLES_DIR="${PROJECT_SOURCE_DIR}/vehicles")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# One ctest entry per TEST_CASE, so `ctest -R <name>` starts single tests.
include(${PROJECT_SOURCE_DIR}/third_party/doctest/cmake/doctest.cmake)
doctest_discover_tests(ultragravity_tests)
