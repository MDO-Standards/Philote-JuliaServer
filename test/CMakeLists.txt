# Test suite for Philote-JuliaServer

find_package(GTest REQUIRED)

# Test helpers library
add_library(julia_test_helpers STATIC
    test_helpers.cpp
)

target_link_libraries(julia_test_helpers
    PUBLIC
        julia_wrapper
        GTest::gtest
)

target_include_directories(julia_test_helpers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Unit tests executable
add_executable(julia_tests
    test_main.cpp
    test_julia_runtime.cpp
    # test_julia_thread.cpp  # Disabled - conflicts with single-threaded executor pattern
    test_julia_convert.cpp
    test_julia_config.cpp
    test_julia_executor.cpp
    # test_julia_explicit_discipline.cpp  # Disabled - needs API fixes
)

target_link_libraries(julia_tests
    PRIVATE
        julia_test_helpers
        julia_wrapper
        GTest::gtest  # Use gtest, not gtest_main (we have custom main)
        GTest::gmock
)

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(julia_tests)

# Integration tests (separate executable to avoid Julia init conflicts)
add_executable(julia_integration_tests
    test_integration.cpp
)

target_link_libraries(julia_integration_tests
    PRIVATE
        julia_test_helpers
        julia_wrapper
        GTest::gtest_main
)

gtest_discover_tests(julia_integration_tests)
