cmake_minimum_required(VERSION 3.28)
project(nonogram_solver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_CUDA "Enable CUDA line-solver (M4)" OFF)
option(BUILD_TESTS "Build doctest test suite" ON)
option(BUILD_CLI   "Build solver_cli (M1d)"     ON)
option(BUILD_APP   "Build GUI app (M2+)"        ON)

if(ENABLE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_ARCHITECTURES 120)  # RTX 5070 Blackwell (sm_120)
    if(NOT CMAKE_CUDA_STANDARD)
        set(CMAKE_CUDA_STANDARD 17)
    endif()
endif()

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlohmann_json)

if(BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

if(BUILD_APP)
    # GLFW
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    # glad2 — Python-based GL loader generator with a CMake integration that
    # generates sources at build time. We add its cmake/ dir as a subdirectory
    # and call glad_add_library() in app/CMakeLists.txt.
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG        v2.0.8
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(glad)
    if(NOT glad_POPULATED)
        FetchContent_Populate(glad)
    endif()

    # Dear ImGui (with GLFW + OpenGL3 backends)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
endif()

add_subdirectory(solver)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_CLI)
    add_subdirectory(solver_cli)
endif()

if(BUILD_APP)
    add_subdirectory(app)
endif()
