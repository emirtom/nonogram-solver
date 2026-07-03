# app/ — GLFW + OpenGL 3.3 + Dear ImGui front-end for the nonogram solver.

# Add glad2's CMake integration (defines glad_add_library).
add_subdirectory(${glad_SOURCE_DIR}/cmake glad_build)

# Generate a static GL 3.3 core loader.
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3 LANGUAGE C)

set(APP_SOURCES
    src/main.cpp
    src/app.cpp
    src/renderer.cpp
    src/puzzle_state.cpp)

add_executable(nonogram_app ${APP_SOURCES})

target_include_directories(nonogram_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(nonogram_app
    PRIVATE
        nonogram_solver
        glfw
        glad_gl_core_33
        nlohmann_json::nlohmann_json)

# Dear ImGui is not a CMake target by default; compile its sources directly
# with the right backends and include paths.
set(IMGUI_DIR ${imgui_SOURCE_DIR})
target_sources(nonogram_app PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(nonogram_app PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends)

target_compile_definitions(nonogram_app PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# OpenGL 3.3 core profile — request via GLFW window hints in code.
find_package(OpenGL REQUIRED)
target_link_libraries(nonogram_app PRIVATE OpenGL::GL)
