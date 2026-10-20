add_library(ultragravity_test_world
  src/test_world.cpp
)
add_library(ultragravity::test_world ALIAS ultragravity_test_world)

target_include_directories(ultragravity_test_world PUBLIC include)
target_link_libraries(ultragravity_test_world PUBLIC ultragravity::physics)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_test_world PRIVATE -Wall -Wextra -Wpedantic)
endif()
