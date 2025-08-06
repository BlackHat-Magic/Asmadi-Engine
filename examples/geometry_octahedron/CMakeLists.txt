add_executable(octahedron main.c)

target_link_libraries(octahedron PRIVATE engine)

set_target_properties(octahedron PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

if(COMPILE_SHADERS AND GLSLANG_VALIDATOR)
    add_dependencies(octahedron EngineShaders)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()