include(FetchContent)

FetchContent_Declare(
    microui
    GIT_REPOSITORY  https://github.com/rxi/microui.git
    GIT_TAG         0850aba860959c3e75fb3e97120ca92957f9d057
    GIT_SHALLOW     TRUE
)

FetchContent_MakeAvailable(microui)

add_library(microui STATIC
    ${microui_SOURCE_DIR}/src/microui.c
)

target_include_directories(microui PUBLIC
    ${microui_SOURCE_DIR}/src
)

target_compile_definitions(microui PUBLIC
    MU_NO_GLFW=1
    MU_NO_FREETYPE=1
)

if(MSVC)
    target_compile_options(microui PRIVATE /W4 /wd4996)
else()
    target_compile_options(microui PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()