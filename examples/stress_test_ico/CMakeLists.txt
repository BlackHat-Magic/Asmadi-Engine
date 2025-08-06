add_executable(stress_test_ico main.c)

target_link_libraries(stress_test_ico PRIVATE engine)

set_target_properties(stress_test_ico PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

if(COMPILE_SHADERS AND GLSLANG_VALIDATOR)
    add_dependencies(stress_test_ico EngineShaders)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()