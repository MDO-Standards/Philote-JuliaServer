# Test suite for Philote-JuliaServer

find_package(GTest REQUIRED)

# Unit tests executable
add_executable(julia_tests
    test_julia_runtime.cpp
    test_julia_thread.cpp
    test_julia_convert.cpp
    test_julia_config.cpp
)

target_link_libraries(julia_tests
    PRIVATE
        julia_wrapper
        GTest::gtest_main
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
        julia_wrapper
        GTest::gtest_main
)

gtest_discover_tests(julia_integration_tests)
