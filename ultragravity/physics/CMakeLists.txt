# The sources live in the Unreal module Source/UltraGravity; CMake builds the same files.
# UltraGravityModule.cpp is Unreal boilerplate and is not built here.
set(UG_MODULE ${PROJECT_SOURCE_DIR}/../Source/UltraGravity)

add_library(ultragravity_physics
  ${UG_MODULE}/Private/animation_data.cpp
  ${UG_MODULE}/Private/checksum.cpp
  ${UG_MODULE}/Private/input_smoothing.cpp
  ${UG_MODULE}/Private/recording.cpp
  ${UG_MODULE}/Private/telemetry_csv.cpp
  ${UG_MODULE}/Private/vehicle_file.cpp
  ${UG_MODULE}/Private/vehicle_step.cpp
)
add_library(ultragravity::physics ALIAS ultragravity_physics)

target_include_directories(ultragravity_physics PUBLIC ${UG_MODULE}/Public)
# The JSON parser stays a detail of the loading code and is not visible outside.
target_include_directories(ultragravity_physics PRIVATE ${UG_MODULE}/ThirdParty)
target_compile_definitions(ultragravity_physics PRIVATE JSON_NOEXCEPTION)

# As in Unreal: without exceptions and without RTTI.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ultragravity_physics PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
elseif(MSVC)
  target_compile_options(ultragravity_physics PRIVATE /EHs-c- /GR- /W4)
endif()
