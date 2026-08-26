cmake_minimum_required(VERSION 3.16)
project(RenUIInstallConsumer LANGUAGES CXX)

find_package(RenUI 0.1.3 CONFIG REQUIRED)

add_executable(renui_install_consumer main.cpp)
target_link_libraries(renui_install_consumer PRIVATE RenUI::RenUI)
target_compile_features(renui_install_consumer PRIVATE cxx_std_17)

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    add_custom_command(TARGET renui_install_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DRENUI_RUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:renui_install_consumer>,|>"
                "-DRENUI_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:renui_install_consumer>"
                -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/CopyRuntimeDlls.cmake"
        COMMENT "Copying installed-package runtime dependencies"
        VERBATIM
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME RenUI.InstallConsumer COMMAND renui_install_consumer)
endif()
