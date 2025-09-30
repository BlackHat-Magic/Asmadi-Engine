cmake_minimum_required(VERSION 3.28)  # Updated for better support
project(AsmadiEngine LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies (shared)
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

# Shader compilation option (shared, but executed in engine)
option(COMPILE_SHADERS "Compile shaders with glslangValidator" ON)
if(COMPILE_SHADERS)
    find_program(GLSLANG_VALIDATOR glslangValidator)
    if(NOT GLSLANG_VALIDATOR)
        message(WARNING "glslangValidator not found; shaders won't be compiled automatically.")
    endif()
endif()

# Enable optimizations for dead code elimination
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)  # Enables LTO for stripping unused code

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(examples)
# add_subdirectory(games)  # Uncomment when adding games