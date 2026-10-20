add_library(ultragravity_testwelt
  src/testwelt.cpp
)
add_library(ultragravity::testwelt ALIAS ultragravity_testwelt)

target_include_directories(ultragravity_testwelt PUBLIC include)
target_link_libraries(ultragravity_testwelt PUBLIC ultragravity::physik)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_testwelt PRIVATE -Wall -Wextra -Wpedantic)
endif()
