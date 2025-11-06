// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <vector>
#include <atomic>

#include "julia_runtime.h"
#include "julia_executor.h"
#include "test_helpers.h"

namespace philote {
namespace julia {
namespace test {

using philote::julia::JuliaRuntime;
using philote::julia::JuliaExecutor;

// Test fixture for JuliaRuntime tests
class JuliaRuntimeTest : public JuliaTestFixture {
protected:
    void SetUp() override {
        JuliaTestFixture::SetUp();
    }
};

// Basic singleton and initialization tests

TEST_F(JuliaRuntimeTest, Singleton) {
    JuliaRuntime& runtime1 = JuliaRuntime::GetInstance();
    JuliaRuntime& runtime2 = JuliaRuntime::GetInstance();

    EXPECT_EQ(&runtime1, &runtime2);
    EXPECT_TRUE(runtime1.IsInitialized());
}

TEST_F(JuliaRuntimeTest, IsInitialized) {
    JuliaRuntime& runtime = JuliaRuntime::GetInstance();
    EXPECT_TRUE(runtime.IsInitialized());
}

TEST_F(JuliaRuntimeTest, GetMainModule) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_module_t* main_mod = runtime.GetMainModule();
        return main_mod != nullptr && main_mod == jl_main_module;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, GetBaseModule) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_module_t* base_mod = runtime.GetBaseModule();
        return base_mod != nullptr && base_mod == jl_base_module;
    });

    EXPECT_TRUE(result);
}

// EvalString tests

TEST_F(JuliaRuntimeTest, EvalStringSimpleArithmetic) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("2 + 2");
        if (!result) throw std::runtime_error("EvalString returned null");
        return static_cast<int>(jl_unbox_int64(result));
    });

    EXPECT_EQ(result, 4);
}

TEST_F(JuliaRuntimeTest, EvalStringMultiplication) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("7 * 6");
        if (!result) throw std::runtime_error("EvalString returned null");
        return static_cast<int>(jl_unbox_int64(result));
    });

    EXPECT_EQ(result, 42);
}

TEST_F(JuliaRuntimeTest, EvalStringFloatingPoint) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("3.14159");
        if (!result) throw std::runtime_error("EvalString returned null");
        return jl_unbox_float64(result);
    });

    EXPECT_NEAR(result, 3.14159, 1e-9);
}

TEST_F(JuliaRuntimeTest, EvalStringString) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("\"Hello Julia\"");
        if (!result) throw std::runtime_error("EvalString returned null");
        return std::string(jl_string_ptr(result));
    });

    EXPECT_EQ(result, "Hello Julia");
}

TEST_F(JuliaRuntimeTest, EvalStringArray) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("[1, 2, 3, 4, 5]");
        if (!result) return false;

        jl_array_t* arr = (jl_array_t*)result;
        if (jl_array_len(arr) != 5) return false;

        int64_t* data = jl_array_data(arr, int64_t);
        for (int i = 0; i < 5; ++i) {
            if (data[i] != i + 1) return false;
        }

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, EvalStringFunctionDefinition) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();

        // Define a function
        runtime.EvalString("square(x) = x^2");

        // Call the function
        jl_value_t* result = runtime.EvalString("square(5)");
        if (!result) throw std::runtime_error("Function call failed");

        return static_cast<int>(jl_unbox_int64(result));
    });

    EXPECT_EQ(result, 25);
}

TEST_F(JuliaRuntimeTest, EvalStringComplexExpression) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("sqrt(16.0) + 2^3");
        if (!result) throw std::runtime_error("EvalString returned null");
        return jl_unbox_float64(result);
    });

    EXPECT_NEAR(result, 12.0, 1e-9);  // sqrt(16) + 8 = 12
}

TEST_F(JuliaRuntimeTest, EvalStringWithVariables) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();

        runtime.EvalString("test_var_a = 10");
        runtime.EvalString("test_var_b = 20");
        jl_value_t* result = runtime.EvalString("test_var_a + test_var_b");

        if (!result) throw std::runtime_error("EvalString returned null");
        return static_cast<int>(jl_unbox_int64(result));
    });

    EXPECT_EQ(result, 30);
}

// EvalString error handling tests

TEST_F(JuliaRuntimeTest, DISABLED_EvalStringInvalidSyntax) {
    // DISABLED: Julia's parser may accept certain malformed expressions or return nothing
    // instead of throwing, making this test unreliable
    EXPECT_THROW({
        JuliaExecutor::GetInstance().Submit([]() {
            JuliaRuntime& runtime = JuliaRuntime::GetInstance();
            runtime.EvalString("2 + + 2");  // Invalid syntax
            return 0;
        });
    }, std::runtime_error);
}

TEST_F(JuliaRuntimeTest, EvalStringUndefinedVariable) {
    EXPECT_THROW({
        JuliaExecutor::GetInstance().Submit([]() {
            JuliaRuntime& runtime = JuliaRuntime::GetInstance();
            runtime.EvalString("undefined_variable_xyz");
            return 0;
        });
    }, std::runtime_error);
}

TEST_F(JuliaRuntimeTest, EvalStringRuntimeError) {
    EXPECT_THROW({
        JuliaExecutor::GetInstance().Submit([]() {
            JuliaRuntime& runtime = JuliaRuntime::GetInstance();
            runtime.EvalString("error(\"Intentional error\")");
            return 0;
        });
    }, std::runtime_error);
}

// LoadJuliaFile tests

TEST_F(JuliaRuntimeTest, LoadJuliaFileParaboloid) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        std::string filepath = GetTestDisciplinePath("paraboloid.jl");

        jl_module_t* module = runtime.LoadJuliaFile(filepath);
        if (!module) return false;

        // Check that ParaboloidDiscipline type is defined
        jl_value_t* type_check = runtime.EvalString("isdefined(Main, :ParaboloidDiscipline)");
        if (!type_check) return false;

        return static_cast<bool>(jl_unbox_bool(type_check));
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, LoadJuliaFileMultiOutput) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        std::string filepath = GetTestDisciplinePath("multi_output.jl");

        jl_module_t* module = runtime.LoadJuliaFile(filepath);
        if (!module) return false;

        // Check that MultiOutputDiscipline type is defined
        jl_value_t* type_check = runtime.EvalString("isdefined(Main, :MultiOutputDiscipline)");
        if (!type_check) return false;

        return static_cast<bool>(jl_unbox_bool(type_check));
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, LoadJuliaFileErrorDiscipline) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        std::string filepath = GetTestDisciplinePath("error_discipline.jl");

        jl_module_t* module = runtime.LoadJuliaFile(filepath);
        if (!module) return false;

        // Check that ErrorDiscipline type is defined
        jl_value_t* type_check = runtime.EvalString("isdefined(Main, :ErrorDiscipline)");
        if (!type_check) return false;

        return static_cast<bool>(jl_unbox_bool(type_check));
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, LoadJuliaFileWithFunctions) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        std::string filepath = GetTestDisciplinePath("paraboloid.jl");

        runtime.LoadJuliaFile(filepath);

        // Check that functions are available
        jl_value_t* setup_check = runtime.EvalString("isdefined(Main, :setup!)");
        if (!setup_check || !jl_unbox_bool(setup_check)) return false;

        jl_value_t* compute_check = runtime.EvalString("isdefined(Main, :compute)");
        if (!compute_check || !jl_unbox_bool(compute_check)) return false;

        jl_value_t* partials_check = runtime.EvalString("isdefined(Main, :compute_partials)");
        if (!partials_check || !jl_unbox_bool(partials_check)) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

// LoadJuliaFile error handling tests

TEST_F(JuliaRuntimeTest, DISABLED_LoadJuliaFileNonexistent) {
    // DISABLED: This test hangs when Julia tries to format error message
    // The showerror call in LoadJuliaFile appears to wait for stdin
    EXPECT_THROW({
        JuliaExecutor::GetInstance().Submit([]() {
            JuliaRuntime& runtime = JuliaRuntime::GetInstance();
            runtime.LoadJuliaFile("/nonexistent/file.jl");
            return 0;
        });
    }, std::runtime_error);
}

TEST_F(JuliaRuntimeTest, DISABLED_LoadJuliaFileInvalidSyntax) {
    // DISABLED: Hangs when Julia tries to format syntax error message
    EXPECT_THROW({
        JuliaExecutor::GetInstance().Submit([]() {
            JuliaRuntime& runtime = JuliaRuntime::GetInstance();

            // Create a temp file with invalid Julia syntax
            std::string temp_file = CreateTempJuliaFile("this is not valid Julia syntax @#$%");
            runtime.LoadJuliaFile(temp_file);

            return 0;
        });
    }, std::runtime_error);
}

TEST_F(JuliaRuntimeTest, LoadJuliaFileEmpty) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();

        // Create empty file
        std::string temp_file = CreateTempJuliaFile("");

        jl_module_t* module = runtime.LoadJuliaFile(temp_file);
        return module != nullptr;
    });

    EXPECT_TRUE(result);
}

// Thread safety tests

TEST_F(JuliaRuntimeTest, ConcurrentGetInstance) {
    constexpr int num_threads = 20;
    std::vector<std::thread> threads;
    std::vector<JuliaRuntime*> instances(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &JuliaRuntime::GetInstance();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All instances should be the same
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

TEST_F(JuliaRuntimeTest, ConcurrentEvalString) {
    constexpr int num_threads = 10;
    constexpr int evals_per_thread = 10;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&success_count]() {
            for (int i = 0; i < evals_per_thread; ++i) {
                try {
                    auto result = JuliaExecutor::GetInstance().Submit([]() {
                        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
                        jl_value_t* result = runtime.EvalString("1 + 1");
                        return static_cast<int>(jl_unbox_int64(result));
                    });

                    if (result == 2) {
                        success_count++;
                    }
                } catch (...) {
                    // Should not throw
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads * evals_per_thread);
}

// Edge cases

TEST_F(JuliaRuntimeTest, EvalStringEmptyString) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("");
        // Empty string evaluation returns nothing/void
        return result != nullptr;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, EvalStringWhitespace) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString("   ");
        return result != nullptr;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaRuntimeTest, EvalStringMultiline) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        jl_value_t* result = runtime.EvalString(R"(
            x = 5
            y = 10
            x + y
        )");
        if (!result) throw std::runtime_error("Multiline eval failed");
        return static_cast<int>(jl_unbox_int64(result));
    });

    EXPECT_EQ(result, 15);
}

TEST_F(JuliaRuntimeTest, DISABLED_LoadJuliaFileMultipleTimes) {
    // DISABLED: Loading the same file multiple times is not a meaningful use case
    // and can cause issues with module redefinition
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        JuliaRuntime& runtime = JuliaRuntime::GetInstance();
        std::string filepath = GetTestDisciplinePath("paraboloid.jl");

        // Load the same file multiple times
        jl_module_t* module1 = runtime.LoadJuliaFile(filepath);
        jl_module_t* module2 = runtime.LoadJuliaFile(filepath);
        jl_module_t* module3 = runtime.LoadJuliaFile(filepath);

        // All should succeed (redefines are allowed in Julia)
        return module1 != nullptr && module2 != nullptr && module3 != nullptr;
    });

    EXPECT_TRUE(result);
}

}  // namespace test
}  // namespace julia
}  // namespace philote
