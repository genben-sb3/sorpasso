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
  test_maneuver_test.cpp
  abs_test.cpp
  stability_test.cpp
  recording_test.cpp
  pp_value_test.cpp
  drift_test.cpp
)
# doctest lives here, nlohmann json in the Unreal module.
target_include_directories(ultragravity_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party
  ${PROJECT_SOURCE_DIR}/../Source/UltraGravity/ThirdParty)
target_link_libraries(ultragravity_tests PRIVATE ultragravity::tuning)
# The vehicle files live in the game's Content folder, so packaged builds take them along (ADR 0010).
target_compile_definitions(ultragravity_tests PRIVATE
  UG_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  UG_VEHICLES_DIR="${PROJECT_SOURCE_DIR}/../Content/VehicleFiles")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# One ctest entry per TEST_CASE, so `ctest -R <name>` starts single tests.
include(${PROJECT_SOURCE_DIR}/third_party/doctest/cmake/doctest.cmake)
doctest_discover_tests(ultragravity_tests)

# The tuning tool as a command: measures the sports coupe and shows the table in the terminal.
add_test(NAME tune_measures_sports_coupe
  COMMAND tune ${PROJECT_SOURCE_DIR}/../Content/VehicleFiles/sports_coupe.json)
set_tests_properties(tune_measures_sports_coupe PROPERTIES
  PASS_REGULAR_EXPRESSION "Measurement: Sports Coupe.*Acceleration 0–100 km/h.*Braking 100–0 km/h without ABS.*Top speed")
