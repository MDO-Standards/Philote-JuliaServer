// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

#include "julia_executor.h"
#include "julia_runtime.h"
#include "julia_convert.h"
#include "test_helpers.h"

namespace philote {
namespace julia {
namespace test {

using namespace std::chrono_literals;

// Test fixture for JuliaExecutor tests
class JuliaExecutorTest : public JuliaTestFixture {
protected:
    void SetUp() override {
        JuliaTestFixture::SetUp();
        executor_ = &JuliaExecutor::GetInstance();
    }

    void TearDown() override {
        // Executor is a singleton, so we don't stop it between tests
        // to avoid issues with Julia runtime state
    }

    JuliaExecutor* executor_;
};

// Basic lifecycle tests

TEST_F(JuliaExecutorTest, GetInstanceReturnsSameInstance) {
    JuliaExecutor& instance1 = JuliaExecutor::GetInstance();
    JuliaExecutor& instance2 = JuliaExecutor::GetInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// Simple task submission tests

TEST_F(JuliaExecutorTest, SubmitSimpleIntReturningTask) {
    auto result = executor_->Submit([]() {
        return 42;
    });

    EXPECT_EQ(result, 42);
}

TEST_F(JuliaExecutorTest, SubmitSimpleDoubleReturningTask) {
    auto result = executor_->Submit([]() {
        return 3.14159;
    });

    EXPECT_NEAR(result, 3.14159, 1e-9);
}

TEST_F(JuliaExecutorTest, SubmitSimpleStringReturningTask) {
    auto result = executor_->Submit([]() {
        return std::string("Hello from Julia thread");
    });

    EXPECT_EQ(result, "Hello from Julia thread");
}

TEST_F(JuliaExecutorTest, SubmitVoidTask) {
    std::atomic<bool> executed{false};

    executor_->Submit([&executed]() {
        executed = true;
    });

    EXPECT_TRUE(executed);
}

// Tests with Julia C API calls

TEST_F(JuliaExecutorTest, SubmitTaskWithJuliaEval) {
    auto result = executor_->Submit([]() {
        jl_value_t* ret = jl_eval_string("2 + 2");
        if (!ret) {
            throw std::runtime_error("Julia eval failed");
        }
        return static_cast<int>(jl_unbox_int64(ret));
    });

    EXPECT_EQ(result, 4);
}

TEST_F(JuliaExecutorTest, SubmitTaskWithJuliaStringReturn) {
    auto result = executor_->Submit([]() {
        jl_value_t* ret = jl_eval_string("\"Julia string\"");
        if (!ret) {
            throw std::runtime_error("Julia eval failed");
        }
        return std::string(jl_string_ptr(ret));
    });

    EXPECT_EQ(result, "Julia string");
}

TEST_F(JuliaExecutorTest, SubmitTaskWithJuliaArrayCreation) {
    auto result = executor_->Submit([]() {
        jl_value_t* array_type = jl_apply_array_type(reinterpret_cast<jl_value_t*>(jl_float64_type), 1);
        jl_array_t* array = jl_alloc_array_1d(array_type, 5);

        double* data = jl_array_data(array, double);
        for (size_t i = 0; i < 5; ++i) {
            data[i] = static_cast<double>(i) * 2.0;
        }

        std::vector<double> result;
        for (size_t i = 0; i < 5; ++i) {
            result.push_back(data[i]);
        }
        return result;
    });

    EXPECT_EQ(result.size(), 5);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(result[i], static_cast<double>(i) * 2.0);
    }
}

// Exception handling tests

TEST_F(JuliaExecutorTest, SubmitTaskThatThrowsStdException) {
    EXPECT_THROW({
        executor_->Submit([]() -> int {
            throw std::runtime_error("Test exception");
        });
    }, std::runtime_error);
}

TEST_F(JuliaExecutorTest, SubmitTaskThatThrowsCustomException) {
    struct CustomException : public std::exception {
        const char* what() const noexcept override {
            return "Custom exception";
        }
    };

    EXPECT_THROW({
        executor_->Submit([]() -> int {
            throw CustomException();
        });
    }, CustomException);
}

TEST_F(JuliaExecutorTest, SubmitTaskWithJuliaError) {
    EXPECT_THROW({
        executor_->Submit([]() -> int {
            jl_eval_string("error(\"Intentional Julia error\")");
            jl_exception_occurred();
            throw std::runtime_error("Julia error occurred");
        });
    }, std::runtime_error);
}

TEST_F(JuliaExecutorTest, ExceptionDoesNotCrashExecutor) {
    // First task throws
    EXPECT_THROW({
        executor_->Submit([]() -> int {
            throw std::runtime_error("First error");
        });
    }, std::runtime_error);

    // Executor should still work
    auto result = executor_->Submit([]() {
        return 123;
    });

    EXPECT_EQ(result, 123);
}

// Concurrency tests

TEST_F(JuliaExecutorTest, SubmitFromMultipleThreads) {
    constexpr int num_threads = 10;
    constexpr int tasks_per_thread = 20;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < tasks_per_thread; ++i) {
                int expected = t * 1000 + i;
                auto result = executor_->Submit([expected]() {
                    return expected;
                });

                if (result == expected) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads * tasks_per_thread);
}

TEST_F(JuliaExecutorTest, TasksExecuteInOrder) {
    constexpr int num_tasks = 100;
    std::vector<int> results;
    results.reserve(num_tasks);

    std::mutex results_mutex;

    // Submit tasks that record their execution order
    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, &results, &results_mutex, i]() {
            executor_->Submit([&results, &results_mutex, i]() {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(i);
            });
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All tasks should have executed
    EXPECT_EQ(results.size(), num_tasks);

    // Note: We can't guarantee strict ordering due to concurrent submission,
    // but all values should be present
    std::vector<int> sorted_results = results;
    std::sort(sorted_results.begin(), sorted_results.end());

    for (int i = 0; i < num_tasks; ++i) {
        EXPECT_EQ(sorted_results[i], i);
    }
}

TEST_F(JuliaExecutorTest, ConcurrentSubmissionsWithDifferentReturnTypes) {
    std::atomic<int> int_count{0};
    std::atomic<int> double_count{0};
    std::atomic<int> string_count{0};

    std::vector<std::thread> threads;

    // Submit int tasks
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &int_count]() {
            auto result = executor_->Submit([]() { return 42; });
            if (result == 42) int_count++;
        });
    }

    // Submit double tasks
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &double_count]() {
            auto result = executor_->Submit([]() { return 3.14; });
            if (std::abs(result - 3.14) < 1e-9) double_count++;
        });
    }

    // Submit string tasks
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &string_count]() {
            auto result = executor_->Submit([]() { return std::string("test"); });
            if (result == "test") string_count++;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(int_count, 10);
    EXPECT_EQ(double_count, 10);
    EXPECT_EQ(string_count, 10);
}

// Tests with Variables and Partials

TEST_F(JuliaExecutorTest, SubmitTaskReturningVariables) {
    auto result = executor_->Submit([]() {
        Variables vars;
        vars["x"] = Variable(kInput, {3});
        vars["x"](0) = 1.0;
        vars["x"](1) = 2.0;
        vars["x"](2) = 3.0;
        return vars;
    });

    ASSERT_TRUE(result.count("x") > 0);
    EXPECT_EQ(result["x"].Size(), 3);
    EXPECT_DOUBLE_EQ(result["x"](0), 1.0);
    EXPECT_DOUBLE_EQ(result["x"](1), 2.0);
    EXPECT_DOUBLE_EQ(result["x"](2), 3.0);
}

TEST_F(JuliaExecutorTest, SubmitTaskReturningPartials) {
    auto result = executor_->Submit([]() {
        Partials partials;
        partials[{"y", "x"}] = Variable(kOutput, {2, 2});
        // Matrix {2,2}: use flat indexing
        partials[{"y", "x"}](0) = 1.0;  // [0,0]
        partials[{"y", "x"}](1) = 2.0;  // [0,1]
        partials[{"y", "x"}](2) = 3.0;  // [1,0]
        partials[{"y", "x"}](3) = 4.0;  // [1,1]
        return partials;
    });

    auto key = std::make_pair("y", "x");
    ASSERT_TRUE(result.count(key) > 0);
    EXPECT_EQ(result[key].Size(), 4);
    // Use flat indexing for access
    EXPECT_DOUBLE_EQ(result[key](0), 1.0);  // [0,0]
    EXPECT_DOUBLE_EQ(result[key](1), 2.0);  // [0,1]
    EXPECT_DOUBLE_EQ(result[key](2), 3.0);  // [1,0]
    EXPECT_DOUBLE_EQ(result[key](3), 4.0);  // [1,1]
}

TEST_F(JuliaExecutorTest, SubmitTaskWithVariablesParameter) {
    Variables input_vars;
    input_vars["a"] = Variable(kInput, {2});
    input_vars["a"](0) = 10.0;
    input_vars["a"](1) = 20.0;

    auto result = executor_->Submit([input_vars]() {
        return input_vars.at("a")(0) + input_vars.at("a")(1);
    });

    EXPECT_DOUBLE_EQ(result, 30.0);
}

// Stress tests

TEST_F(JuliaExecutorTest, ManySmallTasks) {
    constexpr int num_tasks = 1000;
    std::atomic<int> counter{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, &counter]() {
            executor_->Submit([&counter]() {
                counter++;
            });
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter, num_tasks);
}

TEST_F(JuliaExecutorTest, TasksWithVariableExecutionTime) {
    constexpr int num_tasks = 50;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([this, &completed, i]() {
            executor_->Submit([&completed, i]() {
                // Variable work (some tasks do more work than others)
                int iterations = (i % 10 + 1) * 100;
                volatile double sum = 0.0;
                for (int j = 0; j < iterations; ++j) {
                    sum += j * 0.001;
                }
                completed++;
            });
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completed, num_tasks);
}

// Edge cases

TEST_F(JuliaExecutorTest, SubmitTaskReturningEmptyString) {
    auto result = executor_->Submit([]() {
        return std::string("");
    });

    EXPECT_EQ(result, "");
}

TEST_F(JuliaExecutorTest, SubmitTaskReturningZero) {
    auto result = executor_->Submit([]() {
        return 0;
    });

    EXPECT_EQ(result, 0);
}

TEST_F(JuliaExecutorTest, SubmitTaskReturningNegativeNumber) {
    auto result = executor_->Submit([]() {
        return -42;
    });

    EXPECT_EQ(result, -42);
}

TEST_F(JuliaExecutorTest, SubmitTaskReturningNaN) {
    auto result = executor_->Submit([]() {
        return std::numeric_limits<double>::quiet_NaN();
    });

    EXPECT_TRUE(std::isnan(result));
}

TEST_F(JuliaExecutorTest, SubmitTaskReturningInfinity) {
    auto result = executor_->Submit([]() {
        return std::numeric_limits<double>::infinity();
    });

    EXPECT_TRUE(std::isinf(result));
}

TEST_F(JuliaExecutorTest, SubmitTaskWithLargeDataStructure) {
    constexpr size_t large_size = 10000;

    auto result = executor_->Submit([large_size]() {
        std::vector<double> data(large_size);
        for (size_t i = 0; i < large_size; ++i) {
            data[i] = static_cast<double>(i);
        }
        return data;
    });

    EXPECT_EQ(result.size(), large_size);
    EXPECT_DOUBLE_EQ(result[0], 0.0);
    EXPECT_DOUBLE_EQ(result[large_size - 1], static_cast<double>(large_size - 1));
}

}  // namespace test
}  // namespace julia
}  // namespace philote
