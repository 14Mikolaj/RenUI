add_executable(renui_smoke install-consumer/main.cpp)
target_link_libraries(renui_smoke PRIVATE RenUI::RenUI)
target_compile_features(renui_smoke PRIVATE cxx_std_17)

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    add_custom_command(TARGET renui_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DRENUI_RUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:renui_smoke>,|>"
                "-DRENUI_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:renui_smoke>"
                -P "${CMAKE_CURRENT_LIST_DIR}/../cmake/CopyRuntimeDlls.cmake"
        COMMENT "Copying RenUI smoke-test runtime dependencies"
        VERBATIM
    )
endif()

add_test(NAME RenUI.Smoke COMMAND renui_smoke)
