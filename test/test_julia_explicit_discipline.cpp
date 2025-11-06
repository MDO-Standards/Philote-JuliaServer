// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <vector>
#include <filesystem>

#include "julia_explicit_discipline.h"
#include "julia_config.h"
#include "julia_executor.h"
#include "test_helpers.h"

namespace philote {
namespace julia {
namespace test {

using namespace std::chrono_literals;

// Test fixture for JuliaExplicitDiscipline tests
class JuliaExplicitDisciplineTest : public JuliaTestFixture {
protected:
    void SetUp() override {
        JuliaTestFixture::SetUp();
    }

    void TearDown() override {
        discipline_.reset();
    }

    // Helper to create discipline from test discipline file
    std::unique_ptr<JuliaExplicitDiscipline> CreateDiscipline(
        const std::string& discipline_file,
        const std::string& julia_type) {

        DisciplineConfig config;
        config.kind = DisciplineKind::kExplicit;
        config.julia_file = GetTestDisciplinePath(discipline_file);
        config.julia_type = julia_type;

        return std::make_unique<JuliaExplicitDiscipline>(config);
    }

    std::unique_ptr<JuliaExplicitDiscipline> discipline_;
};

// Construction and initialization tests

TEST_F(JuliaExplicitDisciplineTest, ConstructWithValidConfig) {
    EXPECT_NO_THROW({
        discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");
    });
}

TEST_F(JuliaExplicitDisciplineTest, ConstructWithInvalidFile) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = "/nonexistent/file.jl";
    config.julia_type = "SomeType";

    EXPECT_THROW({
        discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);
    }, std::runtime_error);
}

TEST_F(JuliaExplicitDisciplineTest, ConstructWithInvalidType) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = GetTestDisciplinePath("paraboloid.jl");
    config.julia_type = "NonExistentType";

    EXPECT_THROW({
        discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);
    }, std::runtime_error);
}

// Metadata extraction tests (paraboloid discipline)

TEST_F(JuliaExplicitDisciplineTest, ParaboloidMetadataCorrect) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    const auto& var_meta = discipline_->var_meta();

    // Check we have exactly 3 variables: x, y, f
    ASSERT_EQ(var_meta.size(), 3);

    // Find each variable
    bool found_x = false, found_y = false, found_f = false;

    for (const auto& meta : var_meta) {
        if (meta.name() == "x") {
            found_x = true;
            EXPECT_EQ(meta.type(), philote::kInput);
            EXPECT_EQ(meta.shape_size(), 1);
            EXPECT_EQ(meta.shape(0), 1);
            EXPECT_EQ(meta.units(), "m");
        } else if (meta.name() == "y") {
            found_y = true;
            EXPECT_EQ(meta.type(), philote::kInput);
            EXPECT_EQ(meta.shape_size(), 1);
            EXPECT_EQ(meta.shape(0), 1);
            EXPECT_EQ(meta.units(), "m");
        } else if (meta.name() == "f") {
            found_f = true;
            EXPECT_EQ(meta.type(), philote::kOutput);
            EXPECT_EQ(meta.shape_size(), 1);
            EXPECT_EQ(meta.shape(0), 1);
            EXPECT_EQ(meta.units(), "m^2");
        }
    }

    EXPECT_TRUE(found_x);
    EXPECT_TRUE(found_y);
    EXPECT_TRUE(found_f);
}

TEST_F(JuliaExplicitDisciplineTest, ParaboloidPartialsMetadata) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    const auto& partial_meta = discipline_->partial_meta();

    // Should have 2 partials: df/dx and df/dy
    EXPECT_EQ(partial_meta.size(), 2);

    bool found_df_dx = false, found_df_dy = false;

    for (const auto& meta : partial_meta) {
        if (meta.of() == "f" && meta.wrt() == "x") {
            found_df_dx = true;
        } else if (meta.of() == "f" && meta.wrt() == "y") {
            found_df_dy = true;
        }
    }

    EXPECT_TRUE(found_df_dx);
    EXPECT_TRUE(found_df_dy);
}

// Metadata extraction tests (multi-output discipline)

TEST_F(JuliaExplicitDisciplineTest, MultiOutputMetadataCorrect) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    const auto& var_meta = discipline_->var_meta();

    // Check we have 5 variables: x, y, sum, product, difference
    ASSERT_EQ(var_meta.size(), 5);

    bool found_x = false, found_y = false;
    bool found_sum = false, found_product = false, found_difference = false;

    for (const auto& meta : var_meta) {
        if (meta.name() == "x") {
            found_x = true;
            EXPECT_EQ(meta.type(), philote::kInput);
        } else if (meta.name() == "y") {
            found_y = true;
            EXPECT_EQ(meta.type(), philote::kInput);
        } else if (meta.name() == "sum") {
            found_sum = true;
            EXPECT_EQ(meta.type(), philote::kOutput);
            EXPECT_EQ(meta.units(), "m");
        } else if (meta.name() == "product") {
            found_product = true;
            EXPECT_EQ(meta.type(), philote::kOutput);
            EXPECT_EQ(meta.units(), "m^2");
        } else if (meta.name() == "difference") {
            found_difference = true;
            EXPECT_EQ(meta.type(), philote::kOutput);
            EXPECT_EQ(meta.units(), "m");
        }
    }

    EXPECT_TRUE(found_x && found_y);
    EXPECT_TRUE(found_sum && found_product && found_difference);
}

TEST_F(JuliaExplicitDisciplineTest, MultiOutputPartialsMetadata) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    const auto& partial_meta = discipline_->partial_meta();

    // Should have 6 partials: 3 outputs × 2 inputs
    EXPECT_EQ(partial_meta.size(), 6);

    std::set<std::pair<std::string, std::string>> expected_partials = {
        {"sum", "x"}, {"sum", "y"},
        {"product", "x"}, {"product", "y"},
        {"difference", "x"}, {"difference", "y"}
    };

    for (const auto& meta : partial_meta) {
        auto key = std::make_pair(meta.of(), meta.wrt());
        EXPECT_TRUE(expected_partials.count(key) > 0)
            << "Unexpected partial: d" << meta.of() << "/d" << meta.wrt();
        expected_partials.erase(key);
    }

    EXPECT_TRUE(expected_partials.empty()) << "Missing expected partials";
}

// Compute tests

TEST_F(JuliaExplicitDisciplineTest, ParaboloidComputeBasic) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    // Create inputs
    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 3.0;
    inputs["y"](0) = 4.0;

    // Create outputs
    philote::Variables outputs;
    outputs["f"] = philote::Variable(philote::kOutput, {1});

    // Compute
    discipline_->Compute(inputs, outputs);

    // f = x² + y² = 9 + 16 = 25
    EXPECT_DOUBLE_EQ(outputs["f"](0), 25.0);
}

TEST_F(JuliaExplicitDisciplineTest, ParaboloidComputeZero) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 0.0;
    inputs["y"](0) = 0.0;

    philote::Variables outputs;
    outputs["f"] = philote::Variable(philote::kOutput, {1});

    discipline_->Compute(inputs, outputs);

    EXPECT_DOUBLE_EQ(outputs["f"](0), 0.0);
}

TEST_F(JuliaExplicitDisciplineTest, ParaboloidComputeNegative) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = -2.0;
    inputs["y"](0) = -3.0;

    philote::Variables outputs;
    outputs["f"] = philote::Variable(philote::kOutput, {1});

    discipline_->Compute(inputs, outputs);

    // f = (-2)² + (-3)² = 4 + 9 = 13
    EXPECT_DOUBLE_EQ(outputs["f"](0), 13.0);
}

TEST_F(JuliaExplicitDisciplineTest, MultiOutputCompute) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 5.0;
    inputs["y"](0) = 3.0;

    philote::Variables outputs;
    outputs["sum"] = philote::Variable(philote::kOutput, {1});
    outputs["product"] = philote::Variable(philote::kOutput, {1});
    outputs["difference"] = philote::Variable(philote::kOutput, {1});

    discipline_->Compute(inputs, outputs);

    EXPECT_DOUBLE_EQ(outputs["sum"](0), 8.0);       // 5 + 3
    EXPECT_DOUBLE_EQ(outputs["product"](0), 15.0);  // 5 * 3
    EXPECT_DOUBLE_EQ(outputs["difference"](0), 2.0); // 5 - 3
}

// ComputePartials tests

TEST_F(JuliaExplicitDisciplineTest, ParaboloidComputePartials) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 3.0;
    inputs["y"](0) = 4.0;

    philote::Partials partials;
    partials[{"f", "x"}] = philote::Variable(philote::kOutput, {1});
    partials[{"f", "y"}] = philote::Variable(philote::kOutput, {1});

    discipline_->ComputePartials(inputs, partials);

    // df/dx = 2x = 6
    // df/dy = 2y = 8
    EXPECT_DOUBLE_EQ(partials[{"f", "x"}](0), 6.0);
    EXPECT_DOUBLE_EQ(partials[{"f", "y"}](0), 8.0);
}

TEST_F(JuliaExplicitDisciplineTest, ParaboloidPartialsAtZero) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 0.0;
    inputs["y"](0) = 0.0;

    philote::Partials partials;
    partials[{"f", "x"}] = philote::Variable(philote::kOutput, {1});
    partials[{"f", "y"}] = philote::Variable(philote::kOutput, {1});

    discipline_->ComputePartials(inputs, partials);

    // df/dx = 2x = 0
    // df/dy = 2y = 0
    EXPECT_DOUBLE_EQ(partials[{"f", "x"}](0), 0.0);
    EXPECT_DOUBLE_EQ(partials[{"f", "y"}](0), 0.0);
}

TEST_F(JuliaExplicitDisciplineTest, MultiOutputComputePartials) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 5.0;
    inputs["y"](0) = 3.0;

    philote::Partials partials;
    partials[{"sum", "x"}] = philote::Variable(philote::kOutput, {1});
    partials[{"sum", "y"}] = philote::Variable(philote::kOutput, {1});
    partials[{"product", "x"}] = philote::Variable(philote::kOutput, {1});
    partials[{"product", "y"}] = philote::Variable(philote::kOutput, {1});
    partials[{"difference", "x"}] = philote::Variable(philote::kOutput, {1});
    partials[{"difference", "y"}] = philote::Variable(philote::kOutput, {1});

    discipline_->ComputePartials(inputs, partials);

    // d(sum)/dx = 1, d(sum)/dy = 1
    EXPECT_DOUBLE_EQ(partials[{"sum", "x"}](0), 1.0);
    EXPECT_DOUBLE_EQ(partials[{"sum", "y"}](0), 1.0);

    // d(product)/dx = y = 3, d(product)/dy = x = 5
    EXPECT_DOUBLE_EQ(partials[{"product", "x"}](0), 3.0);
    EXPECT_DOUBLE_EQ(partials[{"product", "y"}](0), 5.0);

    // d(difference)/dx = 1, d(difference)/dy = -1
    EXPECT_DOUBLE_EQ(partials[{"difference", "x"}](0), 1.0);
    EXPECT_DOUBLE_EQ(partials[{"difference", "y"}](0), -1.0);
}

// Gradient verification tests

TEST_F(JuliaExplicitDisciplineTest, ParaboloidGradientsNumericallyCorrect) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 2.5;
    inputs["y"](0) = -1.5;

    philote::Partials analytical_partials;
    analytical_partials[{"f", "x"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"f", "y"}] = philote::Variable(philote::kOutput, {1});

    discipline_->ComputePartials(inputs, analytical_partials);

    EXPECT_TRUE(VerifyGradientCorrectness(
        discipline_.get(), inputs, analytical_partials, 1e-6, 1e-5));
}

TEST_F(JuliaExplicitDisciplineTest, MultiOutputGradientsNumericallyCorrect) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["y"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 7.0;
    inputs["y"](0) = 2.0;

    philote::Partials analytical_partials;
    analytical_partials[{"sum", "x"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"sum", "y"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"product", "x"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"product", "y"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"difference", "x"}] = philote::Variable(philote::kOutput, {1});
    analytical_partials[{"difference", "y"}] = philote::Variable(philote::kOutput, {1});

    discipline_->ComputePartials(inputs, analytical_partials);

    EXPECT_TRUE(VerifyGradientCorrectness(
        discipline_.get(), inputs, analytical_partials, 1e-6, 1e-5));
}

// Thread safety tests

TEST_F(JuliaExplicitDisciplineTest, ConcurrentComputeCalls) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    constexpr int num_threads = 10;
    constexpr int calls_per_thread = 20;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < calls_per_thread; ++i) {
                double x_val = static_cast<double>(t);
                double y_val = static_cast<double>(i);

                philote::Variables inputs;
                inputs["x"] = philote::Variable(philote::kInput, {1});
                inputs["y"] = philote::Variable(philote::kInput, {1});
                inputs["x"](0) = x_val;
                inputs["y"](0) = y_val;

                philote::Variables outputs;
                outputs["f"] = philote::Variable(philote::kOutput, {1});

                try {
                    discipline_->Compute(inputs, outputs);

                    double expected = x_val * x_val + y_val * y_val;
                    if (std::abs(outputs["f"](0) - expected) < 1e-9) {
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

    EXPECT_EQ(success_count, num_threads * calls_per_thread);
}

TEST_F(JuliaExplicitDisciplineTest, ConcurrentComputePartialsCalls) {
    discipline_ = CreateDiscipline("paraboloid.jl", "ParaboloidDiscipline");

    constexpr int num_threads = 10;
    constexpr int calls_per_thread = 20;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < calls_per_thread; ++i) {
                double x_val = static_cast<double>(t);
                double y_val = static_cast<double>(i);

                philote::Variables inputs;
                inputs["x"] = philote::Variable(philote::kInput, {1});
                inputs["y"] = philote::Variable(philote::kInput, {1});
                inputs["x"](0) = x_val;
                inputs["y"](0) = y_val;

                philote::Partials partials;
                partials[{"f", "x"}] = philote::Variable(philote::kOutput, {1});
                partials[{"f", "y"}] = philote::Variable(philote::kOutput, {1});

                try {
                    discipline_->ComputePartials(inputs, partials);

                    double expected_df_dx = 2.0 * x_val;
                    double expected_df_dy = 2.0 * y_val;

                    if (std::abs(partials[{"f", "x"}](0) - expected_df_dx) < 1e-9 &&
                        std::abs(partials[{"f", "y"}](0) - expected_df_dy) < 1e-9) {
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

    EXPECT_EQ(success_count, num_threads * calls_per_thread);
}

TEST_F(JuliaExplicitDisciplineTest, ConcurrentMixedCalls) {
    discipline_ = CreateDiscipline("multi_output.jl", "MultiOutputDiscipline");

    constexpr int num_compute_threads = 5;
    constexpr int num_partials_threads = 5;
    constexpr int calls_per_thread = 20;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Compute threads
    for (int t = 0; t < num_compute_threads; ++t) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < calls_per_thread; ++i) {
                philote::Variables inputs;
                inputs["x"] = philote::Variable(philote::kInput, {1});
                inputs["y"] = philote::Variable(philote::kInput, {1});
                inputs["x"](0) = static_cast<double>(t);
                inputs["y"](0) = static_cast<double>(i);

                philote::Variables outputs;
                outputs["sum"] = philote::Variable(philote::kOutput, {1});
                outputs["product"] = philote::Variable(philote::kOutput, {1});
                outputs["difference"] = philote::Variable(philote::kOutput, {1});

                try {
                    discipline_->Compute(inputs, outputs);
                    success_count++;
                } catch (...) {}
            }
        });
    }

    // ComputePartials threads
    for (int t = 0; t < num_partials_threads; ++t) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < calls_per_thread; ++i) {
                philote::Variables inputs;
                inputs["x"] = philote::Variable(philote::kInput, {1});
                inputs["y"] = philote::Variable(philote::kInput, {1});
                inputs["x"](0) = static_cast<double>(t);
                inputs["y"](0) = static_cast<double>(i);

                philote::Partials partials;
                partials[{"sum", "x"}] = philote::Variable(philote::kOutput, {1});
                partials[{"sum", "y"}] = philote::Variable(philote::kOutput, {1});
                partials[{"product", "x"}] = philote::Variable(philote::kOutput, {1});
                partials[{"product", "y"}] = philote::Variable(philote::kOutput, {1});
                partials[{"difference", "x"}] = philote::Variable(philote::kOutput, {1});
                partials[{"difference", "y"}] = philote::Variable(philote::kOutput, {1});

                try {
                    discipline_->ComputePartials(inputs, partials);
                    success_count++;
                } catch (...) {}
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count,
              (num_compute_threads + num_partials_threads) * calls_per_thread);
}

// Error handling tests

TEST_F(JuliaExplicitDisciplineTest, ErrorDisciplineSetupError) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = GetTestDisciplinePath("error_discipline.jl");
    config.julia_type = "ErrorDiscipline";
    config.options["throw_on_setup"] = true;

    EXPECT_THROW({
        discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);
    }, std::runtime_error);
}

TEST_F(JuliaExplicitDisciplineTest, ErrorDisciplineComputeError) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = GetTestDisciplinePath("error_discipline.jl");
    config.julia_type = "ErrorDiscipline";
    config.options["throw_on_compute"] = true;

    discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 1.0;

    philote::Variables outputs;
    outputs["y"] = philote::Variable(philote::kOutput, {1});

    EXPECT_THROW({
        discipline_->Compute(inputs, outputs);
    }, std::runtime_error);
}

TEST_F(JuliaExplicitDisciplineTest, ErrorDisciplineComputePartialsError) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = GetTestDisciplinePath("error_discipline.jl");
    config.julia_type = "ErrorDiscipline";
    config.options["throw_on_partials"] = true;

    discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);

    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 1.0;

    philote::Partials partials;
    partials[{"y", "x"}] = philote::Variable(philote::kOutput, {1});

    EXPECT_THROW({
        discipline_->ComputePartials(inputs, partials);
    }, std::runtime_error);
}

// SetOptions tests

TEST_F(JuliaExplicitDisciplineTest, SetOptionsChangesErrorBehavior) {
    DisciplineConfig config;
    config.kind = DisciplineKind::kExplicit;
    config.julia_file = GetTestDisciplinePath("error_discipline.jl");
    config.julia_type = "ErrorDiscipline";
    // Don't throw initially
    config.options["throw_on_compute"] = false;

    discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);

    // First compute should succeed
    philote::Variables inputs;
    inputs["x"] = philote::Variable(philote::kInput, {1});
    inputs["x"](0) = 1.0;

    philote::Variables outputs;
    outputs["y"] = philote::Variable(philote::kOutput, {1});

    EXPECT_NO_THROW({
        discipline_->Compute(inputs, outputs);
    });

    // Now set options to throw
    google::protobuf::Struct options;
    (*options.mutable_fields())["throw_on_compute"].set_bool_value(true);
    discipline_->SetOptions(options);

    // Second compute should throw
    EXPECT_THROW({
        discipline_->Compute(inputs, outputs);
    }, std::runtime_error);
}

}  // namespace test
}  // namespace julia
}  // namespace philote
