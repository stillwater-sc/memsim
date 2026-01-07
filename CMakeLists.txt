cmake_minimum_required(VERSION 3.20)

project(stillwater-memsim
    VERSION 0.1.0
    DESCRIPTION "Multi-fidelity memory simulation library for embodied AI accelerators"
    HOMEPAGE_URL "https://github.com/stillwater-sc/stillwater-memsim"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MEMSIM_BUILD_TESTS "Build unit tests" ON)
option(MEMSIM_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MEMSIM_BUILD_EXAMPLES "Build examples" ON)
option(MEMSIM_HEADER_ONLY "Header-only mode (no compiled components)" OFF)
option(MEMSIM_ENABLE_JSON "Enable JSON configuration support" ON)
option(MEMSIM_ENABLE_TRACING "Enable trace output support" ON)

# Main library
add_library(memsim)
add_library(sw::memsim ALIAS memsim)

target_include_directories(memsim
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(memsim PUBLIC cxx_std_20)

# Source files (when not header-only)
if(NOT MEMSIM_HEADER_ONLY)
    target_sources(memsim PRIVATE
        src/technology/lpddr5_controller.cpp
        src/technology/hbm3_controller.cpp
        src/technology/gddr7_controller.cpp
        src/util/json_config.cpp
        src/util/trace.cpp
    )
else()
    # Header-only: create interface library
    set_target_properties(memsim PROPERTIES LINKER_LANGUAGE CXX)
    target_compile_definitions(memsim PUBLIC MEMSIM_HEADER_ONLY)
endif()

# JSON support (nlohmann_json)
if(MEMSIM_ENABLE_JSON)
    find_package(nlohmann_json 3.11 QUIET)
    if(nlohmann_json_FOUND)
        target_link_libraries(memsim PUBLIC nlohmann_json::nlohmann_json)
        target_compile_definitions(memsim PUBLIC MEMSIM_HAS_JSON)
    else()
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
        target_link_libraries(memsim PUBLIC nlohmann_json::nlohmann_json)
        target_compile_definitions(memsim PUBLIC MEMSIM_HAS_JSON)
    endif()
endif()

# Tracing support
if(MEMSIM_ENABLE_TRACING)
    target_compile_definitions(memsim PUBLIC MEMSIM_HAS_TRACING)
endif()

# Tests
if(MEMSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(MEMSIM_BUILD_BENCHMARKS)
    add_subdirectory(tests/benchmarks)
endif()

# Examples
if(MEMSIM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS memsim
    EXPORT memsim-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/sw
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY specs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/memsim/specs
)

install(EXPORT memsim-targets
    FILE memsim-targets.cmake
    NAMESPACE sw::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memsim
)

# Package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/memsim-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/memsim-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/memsim-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memsim
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/memsim-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/memsim-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memsim
)
