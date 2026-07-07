cmake_minimum_required(VERSION 3.21)

project(Taiga
	VERSION 2.0.0
	DESCRIPTION "The desktop tiger"
	HOMEPAGE_URL "https://taiga.moe/"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
	add_compile_options(/Zc:preprocessor)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

option(TAIGA_PORTABLE "Portable mode" ON)
option(TAIGA_BUILD_QT_APP "Build the existing Qt desktop application" ON)
option(TAIGA_BUILD_TESTS "Build Taiga migration tests" ON)

include(TaigaConfig)
include(QtSetup)

if (TAIGA_BUILD_TESTS)
	enable_testing()
	set(ANISTHESIA_BUILD_TESTS ON CACHE BOOL "Build Anisthesia tests" FORCE)
endif()

add_subdirectory(deps)
add_subdirectory(src)
