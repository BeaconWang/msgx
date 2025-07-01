cmake_minimum_required (VERSION 3.8)
project ("CMake-msgx")

include_directories("./include")

add_executable (Msgx "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Msgx PROPERTY CXX_STANDARD 26)
endif()

# TODO: Add tests and install targets if needed.
