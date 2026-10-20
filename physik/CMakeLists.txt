add_library(ultragravity_physik
  src/animationsdaten.cpp
  src/eingabe_glaettung.cpp
  src/fahrzeugdatei.cpp
  src/fahrzeugschritt.cpp
  src/pruefsumme.cpp
  src/telemetrie_csv.cpp
)
add_library(ultragravity::physik ALIAS ultragravity_physik)

target_include_directories(ultragravity_physik PUBLIC include)
# Der JSON-Parser bleibt ein Detail des Ladecodes und ist nach außen nicht sichtbar.
target_include_directories(ultragravity_physik PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_compile_definitions(ultragravity_physik PRIVATE JSON_NOEXCEPTION)

# Wie in Unreal: ohne Exceptions und ohne RTTI.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_physik PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
elseif(MSVC)
  target_compile_options(ultragravity_physik PRIVATE /EHs-c- /GR- /W4)
endif()
