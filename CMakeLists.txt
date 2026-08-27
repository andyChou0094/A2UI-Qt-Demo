cmake_minimum_required(VERSION 3.10)

project(A2UIQtDemo VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The demo deliberately targets the archived Qt 5.12.8 API surface.  EXACT is
# important: accepting a newer Qt installation would hide accidental use of
# APIs that are unavailable on the deployment toolchains.
find_package(Qt5 5.12.8 EXACT REQUIRED COMPONENTS Core Widgets Network Test)

if(NOT Qt5Core_VERSION_STRING STREQUAL "5.12.8")
    message(FATAL_ERROR
        "A2UIQtDemo requires Qt 5.12.8 exactly; found ${Qt5Core_VERSION_STRING}")
endif()

include(A2uiBuildPolicy)
a2ui_enforce_build_policy()

add_library(a2ui_build_contract INTERFACE)
target_compile_features(a2ui_build_contract INTERFACE cxx_std_14)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Keep the language mode visible in verbose build logs in addition to the
    # CXX_STANDARD target property.
    target_compile_options(a2ui_build_contract INTERFACE -std=c++14)
endif()

enable_testing()
add_subdirectory(frontend)
add_subdirectory(tests)
