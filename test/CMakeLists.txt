foreach(name list_test descriptor_test text_test home_test current_test call_test)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE apps spisor_library)
    spisor_strict_warnings(${name})
    target_compile_definitions(${name} PRIVATE APPS_SCRATCH_PATH="${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME ${name} COMMAND ${name})
endforeach()

add_test(NAME sources_are_ascii
    COMMAND ${CMAKE_COMMAND} -DDIRECTORY=${PROJECT_SOURCE_DIR}/src -P ${spisor_library_SOURCE_DIR}/test/ascii_check.cmake)
