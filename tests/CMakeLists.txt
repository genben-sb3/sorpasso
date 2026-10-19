add_executable(ultragravity_tests
  main.cpp
  fahrzeugdatei_test.cpp
  stand_test.cpp
  fahrzeugschritt_test.cpp
  geradeaus_test.cpp
)
target_include_directories(ultragravity_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_link_libraries(ultragravity_tests PRIVATE ultragravity::testwelt)
target_compile_definitions(ultragravity_tests PRIVATE
  UG_FAHRZEUGE_DIR="${PROJECT_SOURCE_DIR}/fahrzeuge")

add_test(NAME ultragravity_tests COMMAND ultragravity_tests)
