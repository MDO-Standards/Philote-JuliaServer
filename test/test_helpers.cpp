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
    // Create temp file in /tmp (mkstemp doesn't support extensions, so we add .jl after)
    char temp_pattern[] = "/tmp/julia_test_XXXXXX";

    // Create unique temporary file
    int fd = mkstemp(temp_pattern);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary Julia file");
    }

    // Rename to add .jl extension
    std::string path_without_ext(temp_pattern);
    std::string path = path_without_ext + ".jl";

    close(fd);

    // Rename the file to have .jl extension
    if (rename(path_without_ext.c_str(), path.c_str()) != 0) {
        unlink(path_without_ext.c_str());  // Clean up
        throw std::runtime_error("Failed to rename temporary file to .jl");
    }

    // Write content
    std::ofstream file(path);
    if (!file.is_open()) {
        unlink(path.c_str());  // Clean up
        throw std::runtime_error("Failed to open temporary file: " + path);
    }

    file << content;
    file.close();

    return path;
}

std::string GetTestDisciplinePath(const std::string& filename) {
    // Try to find test disciplines relative to source directory
    // First try relative to current path (for running from build dir)
    std::filesystem::path cwd = std::filesystem::current_path();

    // Common patterns: build/, build/test/, or source root
    std::vector<std::filesystem::path> search_paths = {
        cwd / "examples" / "test_disciplines",  // Running from build
        cwd / ".." / "examples" / "test_disciplines",  // Running from build/test
        cwd / ".." / ".." / "examples" / "test_disciplines",  // Running from build/test/subdir
    };

    for (const auto& base_path : search_paths) {
        std::filesystem::path file_path = base_path / filename;
        if (std::filesystem::exists(file_path)) {
            return std::filesystem::canonical(file_path).string();
        }
    }

    throw std::runtime_error("Test discipline not found: " + filename +
                           " (searched from " + cwd.string() + ")");
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
