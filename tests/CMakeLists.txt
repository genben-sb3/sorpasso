add_executable(ultragravity_tests
  main.cpp
  fahrzeugdatei_test.cpp
  stand_test.cpp
  fahrzeugschritt_test.cpp
  geradeaus_test.cpp
  antrieb_test.cpp
  kurven_test.cpp
)
target_include_directories(ultragravity_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_link_libraries(ultragravity_tests PRIVATE ultragravity::testwelt)
target_compile_definitions(ultragravity_tests PRIVATE
  UG_FAHRZEUGE_DIR="${PROJECT_SOURCE_DIR}/fahrzeuge")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Ein ctest-Eintrag pro TEST_CASE, damit `ctest -R <Name>` einzelne Tests startet.
include(${PROJECT_SOURCE_DIR}/third_party/doctest/cmake/doctest.cmake)
doctest_discover_tests(ultragravity_tests)
