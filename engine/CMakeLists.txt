add_subdirectory(third_party)

# Engine as static lib
add_library(engine STATIC
    src/ecs/ecs.c
    src/geometry/box.c
    src/geometry/capsule.c
    src/geometry/circle.c
    src/geometry/cone.c
    src/geometry/cylinder.c
    src/geometry/dodecahedron.c
    src/geometry/g_common.c
    src/geometry/icosahedron.c
    src/geometry/lathe.c
    src/geometry/octahedron.c
    src/geometry/plane.c
    src/geometry/ring.c
    src/geometry/sphere.c
    src/geometry/tetrahedron.c
    src/geometry/torus.c
    src/material/m_common.c
    src/material/basic_material.c
    src/material/phong_material.c
    src/math/matrix.c
    src/ui/ui.c
)

# Includes (public for linking targets)
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_image_INCLUDE_DIRS}
)

# Link deps (private, since engine is static)
target_link_libraries(engine PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    m
)

target_link_libraries(engine PUBLIC microui)

# Shader compilation (if enabled)
if(COMPILE_SHADERS AND GLSLANG_VALIDATOR)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    set(SHADERS 
        basic_material.vert 
        basic_material.frag
        phong_material.vert
        phong_material.frag
        ui.vert
        ui_solid.frag
    )

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

    add_custom_target(EngineShaders ALL DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(engine EngineShaders)
endif()