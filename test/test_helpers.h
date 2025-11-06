// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_TEST_HELPERS_H
#define PHILOTE_JULIA_TEST_HELPERS_H

#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <filesystem>

#include "julia_runtime.h"
#include "julia_executor.h"
#include "explicit.h"

namespace philote {
namespace julia {
namespace test {

/**
 * @brief Base test fixture that initializes Julia runtime
 *
 * This fixture ensures Julia runtime is initialized before tests run
 * and that the executor is properly started.
 */
class JuliaTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Julia runtime is singleton, so this is safe
        runtime_ = &JuliaRuntime::GetInstance();

        // Start executor
        JuliaExecutor::GetInstance().Start();
    }

    void TearDown() override {
        // Executor cleanup happens in its destructor
    }

    JuliaRuntime* runtime_;
};

/**
 * @brief Create a temporary Julia file with specified content
 * @param content Julia code to write to file
 * @return Path to created temporary file
 */
std::string CreateTempJuliaFile(const std::string& content);

/**
 * @brief Load a test discipline from examples/test_disciplines/
 * @param filename Name of Julia file (e.g., "paraboloid.jl")
 * @return Absolute path to the discipline file
 */
std::string GetTestDisciplinePath(const std::string& filename);

/**
 * @brief Create a temporary YAML config file for testing
 * @param julia_file Path to Julia discipline file
 * @param julia_type Name of Julia type to instantiate
 * @param port Port number for server (if 0, uses default)
 * @return Path to created temporary config file
 */
std::string CreateTempConfigFile(const std::string& julia_file,
                                  const std::string& julia_type,
                                  int port = 0);

/**
 * @brief Find an available port for testing
 * @return Available port number
 */
int FindAvailablePort();

/**
 * @brief Create a gRPC channel for testing
 * @param address Server address (e.g., "localhost:50051")
 * @return Shared pointer to gRPC channel
 */
std::shared_ptr<grpc::Channel> CreateTestChannel(const std::string& address);

/**
 * @brief Verify gradient correctness using numerical differentiation
 * @param discipline Pointer to discipline to test
 * @param inputs Input variables
 * @param analytical_partials Analytically computed partials
 * @param epsilon Finite difference step size (default 1e-6)
 * @param tolerance Relative tolerance for comparison (default 1e-5)
 * @return True if gradients match within tolerance
 */
bool VerifyGradientCorrectness(philote::Discipline* discipline,
                                const philote::Variables& inputs,
                                const philote::Partials& analytical_partials,
                                double epsilon = 1e-6,
                                double tolerance = 1e-5);

/**
 * @brief Expect Julia exception was thrown and contains message
 * @param callable Function or lambda to execute
 * @param expected_message Expected substring in exception message
 */
template<typename Callable>
void ExpectJuliaExceptionContains(Callable&& callable,
                                   const std::string& expected_message) {
    try {
        callable();
        FAIL() << "Expected exception containing: " << expected_message;
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find(expected_message), std::string::npos)
            << "Exception message '" << msg
            << "' does not contain expected '" << expected_message << "'";
    }
}

/**
 * @brief Expect two variables are equal within tolerance
 * @param expected Expected variable
 * @param actual Actual variable
 * @param tolerance Absolute tolerance (default 1e-9)
 */
void ExpectVariableEquals(const philote::Variable& expected,
                          const philote::Variable& actual,
                          double tolerance = 1e-9);

/**
 * @brief Expect two variable maps are equal within tolerance
 * @param expected Expected variables map
 * @param actual Actual variables map
 * @param tolerance Absolute tolerance (default 1e-9)
 */
void ExpectVariablesEqual(const philote::Variables& expected,
                          const philote::Variables& actual,
                          double tolerance = 1e-9);

/**
 * @brief Expect two partials maps are equal within tolerance
 * @param expected Expected partials map
 * @param actual Actual partials map
 * @param tolerance Absolute tolerance (default 1e-9)
 */
void ExpectPartialsEqual(const philote::Partials& expected,
                         const philote::Partials& actual,
                         double tolerance = 1e-9);

}  // namespace test
}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_TEST_HELPERS_H
