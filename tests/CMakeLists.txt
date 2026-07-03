add_executable(nonogram_tests
    test_main.cpp
    test_logical_rules.cpp
    test_backtracking.cpp
    test_random.cpp)

target_link_libraries(nonogram_tests
    PRIVATE
        nonogram_solver
        doctest::doctest)

# Only test_main.cpp carries DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN; the other
# test TUs just include the header.

# doctest's CMake test-discovery module.
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(nonogram_tests)
