cmake_minimum_required(VERSION 3.21)

project(
	binary_io
	VERSION 2.0.6
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
	set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(BINARY_IO_BUILD_SRC "whether we should build the library itself" ON)
if(BINARY_IO_BUILD_SRC)
	add_subdirectory(src)
endif()

include(CTest)
if(BUILD_TESTING)
	find_package(Catch2 REQUIRED CONFIG)
	include(Catch)
	add_subdirectory(tests)
endif()

option(BINARY_IO_BUILD_DOCS "whether we should build documentation" OFF)
if(BINARY_IO_BUILD_DOCS)
	add_subdirectory(docs)
endif()
