cmake_minimum_required(VERSION 3.20)
project(SpacePirates LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED) # load textures

option(COMPILE_SHADERS "Compile shaders with glslangValidator" ON)
if(COMPILE_SHADERS)
    find_program(GLSLANG_VALIDATOR glslangValidator)
    if(NOT GLSLANG_VALIDATOR)
        message(WARNING "glslangValidator not found; shaders won't be compiled automatically.")
        set(GLSLANGVALIDATOR "") # defined but empty
    else()
        set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
        set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

        set(SHADERS triangle.vert triangle.frag)

        foreach(SHADER ${SHADERS})
            set(INPUT ${SHADER_DIR}/${SHADER})
            set(OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER}.spv)
            add_custom_command(
                OUTPUT ${OUTPUT}
                COMMAND ${GLSLANG_VALIDATOR} -V -o ${OUTPUT} ${INPUT} --target-env vulkan1.3
                DEPENDS ${INPUT}
                COMMENT "Compiling shader ${SHADER}"
            )
            list(APPEND SHADER_OUTPUTS ${OUTPUT})
        endforeach()

        add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})
    endif()
endif()

# executable
add_executable(space-pirates src/main.c src/matrix.c)

# link libraries
target_link_libraries(space-pirates
    PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        m
)

# include directories
target_include_directories(space-pirates
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_image_INCLUDE_DIRS}
)

# set output
set_target_properties(space-pirates PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Copy assets to build
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

if(COMPILE_SHADERS AND GLSLANG_VALIDATOR)
    add_dependencies(space-pirates Shaders)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()