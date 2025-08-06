# Demo executable
add_executable(box main.c)

# Link to engine lib
target_link_libraries(box PRIVATE engine)

# Set output dir
set_target_properties(box PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Copy assets to build dir (only what's needed for this example)
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# If shaders, depend on them (already built via engine)
if(COMPILE_SHADERS AND GLSLANG_VALIDATOR)
    add_dependencies(box EngineShaders)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()