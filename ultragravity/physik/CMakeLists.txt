# Die Quellen liegen im Unreal-Modul Source/UltraGravity; CMake baut dieselben Dateien.
# UltraGravityModule.cpp ist Unreal-Boilerplate und wird hier nicht gebaut.
set(UG_MODUL ${PROJECT_SOURCE_DIR}/../Source/UltraGravity)

add_library(ultragravity_physik
  ${UG_MODUL}/Private/animationsdaten.cpp
  ${UG_MODUL}/Private/eingabe_glaettung.cpp
  ${UG_MODUL}/Private/fahrzeugdatei.cpp
  ${UG_MODUL}/Private/fahrzeugschritt.cpp
  ${UG_MODUL}/Private/pruefsumme.cpp
  ${UG_MODUL}/Private/telemetrie_csv.cpp
)
add_library(ultragravity::physik ALIAS ultragravity_physik)

target_include_directories(ultragravity_physik PUBLIC ${UG_MODUL}/Public)
# Der JSON-Parser bleibt ein Detail des Ladecodes und ist nach außen nicht sichtbar.
target_include_directories(ultragravity_physik PRIVATE ${UG_MODUL}/ThirdParty)
target_compile_definitions(ultragravity_physik PRIVATE JSON_NOEXCEPTION)

# Wie in Unreal: ohne Exceptions und ohne RTTI.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_physik PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
elseif(MSVC)
  target_compile_options(ultragravity_physik PRIVATE /EHs-c- /GR- /W4)
endif()
