cmake_minimum_required(VERSION 3.16)

set(RENUI_VERSION "0.1.12")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(RENUI_IS_TOP_LEVEL ON)
    project(RenUI VERSION ${RENUI_VERSION} LANGUAGES CXX)
else()
    set(RENUI_IS_TOP_LEVEL OFF)
endif()

option(RENUI_FETCH_SFML
    "Fetch the tested SFML release when no SFML package is available" OFF)
option(RENUI_BUILD_TESTS "Build RenUI's standalone smoke tests"
    ${RENUI_IS_TOP_LEVEL})
set(RENUI_SFML_GIT_TAG "3.0.2" CACHE STRING
    "SFML Git tag used when RENUI_FETCH_SFML is enabled")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)

if(NOT TARGET SFML::Graphics AND NOT TARGET sfml-graphics)
    find_package(SFML 3 COMPONENTS Graphics Window System QUIET)
endif()

if(NOT TARGET SFML::Graphics AND NOT TARGET sfml-graphics AND RENUI_FETCH_SFML)
    if((WIN32 AND CMAKE_VERSION VERSION_LESS 3.24) OR
       (NOT WIN32 AND CMAKE_VERSION VERSION_LESS 3.22))
        message(FATAL_ERROR
            "SFML ${RENUI_SFML_GIT_TAG} requires CMake 3.24 or newer on Windows "
            "and CMake 3.22 or newer on other platforms when fetched from source.")
    endif()

    include(FetchContent)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_CONFIGURE_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG ${RENUI_SFML_GIT_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(SFML)
endif()

if(DEFINED SFML_VERSION AND SFML_VERSION VERSION_LESS 3)
    message(FATAL_ERROR "RenUI requires SFML 3 or newer; found SFML ${SFML_VERSION}.")
endif()

if(NOT TARGET SFML::Graphics)
    if(TARGET sfml-graphics AND TARGET sfml-window AND TARGET sfml-system)
        add_library(SFML::Graphics INTERFACE IMPORTED GLOBAL)
        add_library(SFML::Window INTERFACE IMPORTED GLOBAL)
        add_library(SFML::System INTERFACE IMPORTED GLOBAL)
        set_property(TARGET SFML::Graphics PROPERTY INTERFACE_LINK_LIBRARIES sfml-graphics)
        set_property(TARGET SFML::Window PROPERTY INTERFACE_LINK_LIBRARIES sfml-window)
        set_property(TARGET SFML::System PROPERTY INTERFACE_LINK_LIBRARIES sfml-system)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(RenUISFML QUIET IMPORTED_TARGET
                sfml-graphics>=3 sfml-window>=3 sfml-system>=3)
        endif()
        if(TARGET PkgConfig::RenUISFML)
            add_library(SFML::Graphics INTERFACE IMPORTED GLOBAL)
            add_library(SFML::Window INTERFACE IMPORTED GLOBAL)
            add_library(SFML::System INTERFACE IMPORTED GLOBAL)
            set_property(TARGET SFML::Graphics PROPERTY
                INTERFACE_LINK_LIBRARIES PkgConfig::RenUISFML)
            set_property(TARGET SFML::Window PROPERTY
                INTERFACE_LINK_LIBRARIES PkgConfig::RenUISFML)
            set_property(TARGET SFML::System PROPERTY
                INTERFACE_LINK_LIBRARIES PkgConfig::RenUISFML)
        endif()
    endif()
endif()

if(NOT TARGET SFML::Graphics OR NOT TARGET SFML::Window OR NOT TARGET SFML::System)
    message(FATAL_ERROR
        "RenUI requires SFML 3 (Graphics, Window, System). Install SFML 3 or provide "
        "SFML_DIR/CMAKE_PREFIX_PATH; RenUI has no non-SFML rendering backend.")
endif()

add_library(RenUI
    src/Core.cpp
    src/Layout.cpp
    src/Widgets.cpp
    src/TextInput.cpp
    src/TextArea.cpp
    src/Window.cpp
)
add_library(RenUI::RenUI ALIAS RenUI)

target_compile_features(RenUI PUBLIC cxx_std_17)
target_link_libraries(RenUI PUBLIC SFML::Graphics SFML::Window SFML::System)
target_include_directories(RenUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(DEFINED SFML_INCLUDE_DIRS AND SFML_INCLUDE_DIRS)
    target_include_directories(RenUI SYSTEM PUBLIC $<BUILD_INTERFACE:${SFML_INCLUDE_DIRS}>)
endif()

set_target_properties(RenUI PROPERTIES
    VERSION ${RENUI_VERSION}
    SOVERSION 0
    EXPORT_NAME RenUI
    POSITION_INDEPENDENT_CODE ON
)

get_target_property(RENUI_LIBRARY_TYPE RenUI TYPE)
if(RENUI_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(RenUI PUBLIC RENUI_STATIC_DEFINE)
endif()

string(REPLACE "." ";" RENUI_VERSION_PARTS "${RENUI_VERSION}")
list(GET RENUI_VERSION_PARTS 0 RENUI_VERSION_MAJOR)
list(GET RENUI_VERSION_PARTS 1 RENUI_VERSION_MINOR)
list(GET RENUI_VERSION_PARTS 2 RENUI_VERSION_PATCH)

configure_file(
    include/RenUI/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/RenUI/Version.hpp
    @ONLY
)

generate_export_header(RenUI
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/generated/RenUI/Export.hpp
    EXPORT_MACRO_NAME RENUI_API
    NO_EXPORT_MACRO_NAME RENUI_NO_EXPORT
    STATIC_DEFINE RENUI_STATIC_DEFINE
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/RenUIConfigVersion.cmake
    VERSION ${RENUI_VERSION}
    COMPATIBILITY SameMinorVersion
)
configure_package_config_file(
    cmake/RenUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RenUIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RenUI
)

install(TARGETS RenUI
    EXPORT RenUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/RenUI
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/RenUI/Version.hpp
              ${CMAKE_CURRENT_BINARY_DIR}/generated/RenUI/Export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RenUI
)
install(EXPORT RenUITargets
    FILE RenUITargets.cmake
    NAMESPACE RenUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RenUI
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/RenUIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/RenUIConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RenUI
)
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    RENAME LICENSE-RenUI.txt
)
install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    RENAME README-RenUI.md
)

if(RENUI_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
