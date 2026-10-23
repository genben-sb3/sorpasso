# Test driver and test maneuvers as a library, so tests can check them without the terminal.
add_library(ultragravity_tuning
  src/test_driver.cpp
  src/test_maneuver.cpp
)
add_library(ultragravity::tuning ALIAS ultragravity_tuning)

target_include_directories(ultragravity_tuning PUBLIC include)
target_link_libraries(ultragravity_tuning PUBLIC ultragravity::test_world)

# The terminal program: one command measures a car.
add_executable(tune src/tune.cpp)
target_link_libraries(tune PRIVATE ultragravity::tuning)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_tuning PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(tune PRIVATE -Wall -Wextra -Wpedantic)
endif()
