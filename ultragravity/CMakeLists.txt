cmake_minimum_required(VERSION 3.25)
project(ultragravity CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Physikpaket: zieht nach APEXLINE um. Nur Standardbibliothek plus der eingebundene JSON-Header.
add_subdirectory(physik)

# Bleibt im Repo.
add_subdirectory(testwelt)

enable_testing()
add_subdirectory(tests)
