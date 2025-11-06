// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include "test_helpers.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace philote {
namespace julia {
namespace test {

// Helper function implementations

std::string CreateTempJuliaFile(const std::string& content) {
    // Create temp file in /tmp
    std::string temp_pattern = "/tmp/julia_test_XXXXXX.jl";
    char* temp_path = new char[temp_pattern.size() + 1];
    std::strcpy(temp_path, temp_pattern.c_str());

    // Create unique temporary file
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        delete[] temp_path;
        throw std::runtime_error("Failed to create temporary Julia file");
    }

    // Write content
    std::string path(temp_path);
    delete[] temp_path;
    close(fd);

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open temporary file: " + path);
    }

    file << content;
    file.close();

    return path;
}

std::string GetTestDisciplinePath(const std::string& filename) {
    // Construct path to test disciplines
    std::filesystem::path examples_dir = std::filesystem::current_path() / "examples" / "test_disciplines";
    std::filesystem::path file_path = examples_dir / filename;

    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("Test discipline not found: " + file_path.string());
    }

    return file_path.string();
}

void ExpectVariableEquals(const philote::Variable& expected,
                          const philote::Variable& actual,
                          double tolerance) {
    ASSERT_EQ(expected.Size(), actual.Size())
        << "Variable sizes differ: expected " << expected.Size()
        << ", got " << actual.Size();

    const auto& expected_shape = expected.Shape();
    const auto& actual_shape = actual.Shape();
    ASSERT_EQ(expected_shape.size(), actual_shape.size())
        << "Variable dimensions differ";

    for (size_t i = 0; i < expected_shape.size(); ++i) {
        ASSERT_EQ(expected_shape[i], actual_shape[i])
            << "Shape mismatch at dimension " << i;
    }

    for (size_t i = 0; i < expected.Size(); ++i) {
        EXPECT_NEAR(expected(i), actual(i), tolerance)
            << "Value mismatch at index " << i;
    }
}

void ExpectVariablesEqual(const philote::Variables& expected,
                          const philote::Variables& actual,
                          double tolerance) {
    ASSERT_EQ(expected.size(), actual.size())
        << "Number of variables differs: expected " << expected.size()
        << ", got " << actual.size();

    for (const auto& [name, expected_var] : expected) {
        ASSERT_TRUE(actual.count(name) > 0)
            << "Variable '" << name << "' not found in actual";

        const auto& actual_var = actual.at(name);
        ExpectVariableEquals(expected_var, actual_var, tolerance);
    }
}

void ExpectPartialsEqual(const philote::Partials& expected,
                         const philote::Partials& actual,
                         double tolerance) {
    ASSERT_EQ(expected.size(), actual.size())
        << "Number of partials differs: expected " << expected.size()
        << ", got " << actual.size();

    for (const auto& [key, expected_partial] : expected) {
        ASSERT_TRUE(actual.count(key) > 0)
            << "Partial d" << key.first << "/d" << key.second << " not found in actual";

        const auto& actual_partial = actual.at(key);
        ExpectVariableEquals(expected_partial, actual_partial, tolerance);
    }
}

}  // namespace test
}  // namespace julia
}  // namespace philote
