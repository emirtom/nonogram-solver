# solver/ — nonogram solving library (LR + CB)
add_library(nonogram_solver STATIC
    src/nonogram.cpp
    src/logical_rules.cpp
    src/backtracking.cpp
    src/solver.cpp)

target_include_directories(nonogram_solver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(nonogram_solver
    PUBLIC
        nlohmann_json::nlohmann_json)

if(ENABLE_CUDA)
    target_sources(nonogram_solver PRIVATE
        src/cuda/line_solver.cu
        src/cuda/kernels.cu)
    target_compile_definitions(nonogram_solver PUBLIC NONOGRAM_ENABLE_CUDA=1)
    target_link_libraries(nonogram_solver PUBLIC CUDA::cudart)
endif()
