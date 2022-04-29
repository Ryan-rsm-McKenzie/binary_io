set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(INCLUDE_DIR "${ROOT_DIR}/include")
set(HEADER_FILES
	"${INCLUDE_DIR}/binary_io/any_stream.hpp"
	"${INCLUDE_DIR}/binary_io/binary_io.hpp"
	"${INCLUDE_DIR}/binary_io/common.hpp"
	"${INCLUDE_DIR}/binary_io/file_stream.hpp"
	"${INCLUDE_DIR}/binary_io/memory_stream.hpp"
	"${INCLUDE_DIR}/binary_io/span_stream.hpp"
)

set(SOURCE_DIR "${ROOT_DIR}/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/binary_io/binary_io.cpp"
)

source_group(
	TREE "${ROOT_DIR}"
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
)

add_library(
	"${PROJECT_NAME}"
	STATIC
	${HEADER_FILES}
	${SOURCE_FILES}
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig"
)
add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set_target_properties(
	"${PROJECT_NAME}"
	PROPERTIES
		CXX_STANDARD_REQUIRED ON
		DEBUG_POSTFIX "d"
)

target_compile_features(
	"${PROJECT_NAME}"
	PUBLIC
		cxx_std_20
)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
	set_target_properties(
		"${PROJECT_NAME}"
		PROPERTIES
			CXX_VISIBILITY_PRESET "hidden"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PUBLIC
		"$<BUILD_INTERFACE:${INCLUDE_DIR}>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	PRIVATE
		"${SOURCE_DIR}"
)

install(
	TARGETS "${PROJECT_NAME}"
	EXPORT "${PROJECT_NAME}-targets"
)

install(
	EXPORT "${PROJECT_NAME}-targets"
	NAMESPACE "${PROJECT_NAME}::"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

configure_file(
	"${ROOT_DIR}/cmake/config.cmake.in"
	"${PROJECT_NAME}Config.cmake"
	@ONLY
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	COMPATIBILITY "SameMajorVersion"
)

install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
	DIRECTORY "${INCLUDE_DIR}/binary_io"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
