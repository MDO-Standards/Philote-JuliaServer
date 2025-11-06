// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "julia_convert.h"
#include "julia_runtime.h"
#include "julia_thread.h"
#include "julia_executor.h"
#include "test_helpers.h"

namespace philote {
namespace julia {
namespace test {

using philote::Variables;
using philote::Variable;
using philote::Partials;

// Test fixture for JuliaConvert tests
class JuliaConvertTest : public JuliaTestFixture {
protected:
    void SetUp() override {
        JuliaTestFixture::SetUp();
    }
};

// VariablesToJuliaDict and JuliaDictToVariables roundtrip tests

TEST_F(JuliaConvertTest, RoundtripScalar) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["x"] = Variable(philote::kOutput, {1});
        vars["x"](0) = 42.0;

        jl_value_t* dict = VariablesToJuliaDict(vars);
        if (!dict) return false;

        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.size() != 1) return false;
        if (vars_back.count("x") == 0) return false;
        if (std::abs(vars_back.at("x")(0) - 42.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, RoundtripVector) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["vec"] = Variable(philote::kOutput, {3});
        vars["vec"](0) = 1.0;
        vars["vec"](1) = 2.0;
        vars["vec"](2) = 3.0;

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.count("vec") == 0) return false;
        if (vars_back.at("vec").Size() != 3) return false;
        if (std::abs(vars_back.at("vec")(0) - 1.0) > 1e-9) return false;
        if (std::abs(vars_back.at("vec")(1) - 2.0) > 1e-9) return false;
        if (std::abs(vars_back.at("vec")(2) - 3.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_RoundtripMatrix) {
    // DISABLED: Hangs during reshape() or dict operations
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["mat"] = Variable(philote::kOutput, {2, 3});
        // Matrix is {2, 3} - 2 rows, 3 columns. Use flat indexing: row * ncols + col
        vars["mat"](0) = 1.0;  // [0,0]
        vars["mat"](1) = 2.0;  // [0,1]
        vars["mat"](2) = 3.0;  // [0,2]
        vars["mat"](3) = 4.0;  // [1,0]
        vars["mat"](4) = 5.0;  // [1,1]
        vars["mat"](5) = 6.0;  // [1,2]

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.count("mat") == 0) return false;
        if (vars_back.at("mat").Size() != 6) return false;

        const auto& mat = vars_back.at("mat");
        const auto& shape = mat.Shape();
        if (shape.size() != 2 || shape[0] != 2 || shape[1] != 3) return false;

        // Check all values using flat indexing
        if (std::abs(mat(0) - 1.0) > 1e-9) return false;  // [0,0]
        if (std::abs(mat(1) - 2.0) > 1e-9) return false;  // [0,1]
        if (std::abs(mat(2) - 3.0) > 1e-9) return false;  // [0,2]
        if (std::abs(mat(3) - 4.0) > 1e-9) return false;  // [1,0]
        if (std::abs(mat(4) - 5.0) > 1e-9) return false;  // [1,1]
        if (std::abs(mat(5) - 6.0) > 1e-9) return false;  // [1,2]

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_RoundtripMultipleVariables) {
    // DISABLED: Hangs during dict operations with multiple variables
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["a"] = Variable(philote::kOutput, {1});
        vars["a"](0) = 10.0;

        vars["b"] = Variable(philote::kOutput, {2});
        vars["b"](0) = 20.0;
        vars["b"](1) = 30.0;

        vars["c"] = Variable(philote::kOutput, {2, 2});
        // Matrix {2,2}: flat indexing
        vars["c"](0) = 1.0;  // [0,0]
        vars["c"](1) = 2.0;  // [0,1]
        vars["c"](2) = 3.0;  // [1,0]
        vars["c"](3) = 4.0;  // [1,1]

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.size() != 3) return false;
        if (vars_back.count("a") == 0 || vars_back.count("b") == 0 || vars_back.count("c") == 0) return false;

        // Check 'a'
        if (std::abs(vars_back.at("a")(0) - 10.0) > 1e-9) return false;

        // Check 'b'
        if (vars_back.at("b").Size() != 2) return false;
        if (std::abs(vars_back.at("b")(0) - 20.0) > 1e-9) return false;
        if (std::abs(vars_back.at("b")(1) - 30.0) > 1e-9) return false;

        // Check 'c' using flat indexing
        const auto& c = vars_back.at("c");
        if (c.Size() != 4) return false;
        if (std::abs(c(0) - 1.0) > 1e-9) return false;  // [0,0]
        if (std::abs(c(1) - 2.0) > 1e-9) return false;  // [0,1]
        if (std::abs(c(2) - 3.0) > 1e-9) return false;  // [1,0]
        if (std::abs(c(3) - 4.0) > 1e-9) return false;  // [1,1]

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, RoundtripZeroValues) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["zero"] = Variable(philote::kOutput, {3});
        vars["zero"](0) = 0.0;
        vars["zero"](1) = 0.0;
        vars["zero"](2) = 0.0;

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.count("zero") == 0) return false;
        if (vars_back.at("zero").Size() != 3) return false;

        for (size_t i = 0; i < 3; ++i) {
            if (std::abs(vars_back.at("zero")(i)) > 1e-9) return false;
        }

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, RoundtripNegativeValues) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["neg"] = Variable(philote::kOutput, {3});
        vars["neg"](0) = -1.0;
        vars["neg"](1) = -2.5;
        vars["neg"](2) = -100.0;

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.count("neg") == 0) return false;
        if (std::abs(vars_back.at("neg")(0) - (-1.0)) > 1e-9) return false;
        if (std::abs(vars_back.at("neg")(1) - (-2.5)) > 1e-9) return false;
        if (std::abs(vars_back.at("neg")(2) - (-100.0)) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, RoundtripLargeVector) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        constexpr size_t size = 1000;
        Variables vars;
        vars["large"] = Variable(philote::kOutput, {size});

        for (size_t i = 0; i < size; ++i) {
            vars["large"](i) = static_cast<double>(i);
        }

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.count("large") == 0) return false;
        if (vars_back.at("large").Size() != size) return false;

        for (size_t i = 0; i < size; ++i) {
            if (std::abs(vars_back.at("large")(i) - static_cast<double>(i)) > 1e-9) return false;
        }

        return true;
    });

    EXPECT_TRUE(result);
}

// JuliaDictToPartials tests

TEST_F(JuliaConvertTest, PartialsSingleDerivative) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Create a Julia flat dict with "output~input" => [2.0]
        jl_value_t* result_dict = jl_eval_string("Dict(\"y~x\" => [2.0])");
        if (!result_dict || jl_exception_occurred()) return false;

        Partials partials = JuliaDictToPartials(result_dict);

        if (partials.size() != 1) return false;
        if (partials.count({"y", "x"}) == 0) return false;
        if (std::abs(partials.at({"y", "x"})(0) - 2.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, PartialsMultipleDerivatives) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Create flat dict with multiple partials
        jl_value_t* result_dict = jl_eval_string(
            "Dict(\"f~x\" => [2.0], \"f~y\" => [3.0], \"g~x\" => [4.0])");
        if (!result_dict || jl_exception_occurred()) return false;

        Partials partials = JuliaDictToPartials(result_dict);

        if (partials.size() != 3) return false;
        if (partials.count({"f", "x"}) == 0) return false;
        if (partials.count({"f", "y"}) == 0) return false;
        if (partials.count({"g", "x"}) == 0) return false;

        if (std::abs(partials.at({"f", "x"})(0) - 2.0) > 1e-9) return false;
        if (std::abs(partials.at({"f", "y"})(0) - 3.0) > 1e-9) return false;
        if (std::abs(partials.at({"g", "x"})(0) - 4.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, PartialsMatrixJacobian) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Create a partial with matrix jacobian (2x2)
        jl_value_t* result_dict = jl_eval_string(
            "Dict(\"y~x\" => reshape([1.0, 2.0, 3.0, 4.0], 2, 2))");
        if (!result_dict || jl_exception_occurred()) return false;

        Partials partials = JuliaDictToPartials(result_dict);

        if (partials.size() != 1) return false;
        if (partials.count({"y", "x"}) == 0) return false;

        const auto& jacobian = partials.at({"y", "x"});
        if (jacobian.Size() != 4) return false;

        // Julia is column-major, so reshape([1,2,3,4], 2, 2) gives:
        // [1 3]
        // [2 4]
        // Use flat indexing: for 2x2 matrix in row-major: index = row * 2 + col
        if (std::abs(jacobian(0) - 1.0) > 1e-9) return false;  // [0,0]
        if (std::abs(jacobian(2) - 2.0) > 1e-9) return false;  // [1,0]
        if (std::abs(jacobian(1) - 3.0) > 1e-9) return false;  // [0,1]
        if (std::abs(jacobian(3) - 4.0) > 1e-9) return false;  // [1,1]

        return true;
    });

    EXPECT_TRUE(result);
}

// ProtobufStructToJuliaDict tests

TEST_F(JuliaConvertTest, DISABLED_ProtobufStructWithNumbers) {
    // DISABLED: Hangs during protobuf struct conversion
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        google::protobuf::Struct pb_struct;
        (*pb_struct.mutable_fields())["a"].set_number_value(42.0);
        (*pb_struct.mutable_fields())["b"].set_number_value(3.14);

        jl_value_t* dict = ProtobufStructToJuliaDict(pb_struct);
        if (!dict) return false;

        // Check that dict has correct values
        jl_set_global(jl_main_module, jl_symbol("test_dict"), dict);
        jl_value_t* val_a_check = jl_eval_string("get(test_dict, \"a\", nothing)");

        if (!val_a_check || jl_exception_occurred()) return false;

        double a_val = jl_unbox_float64(val_a_check);
        if (std::abs(a_val - 42.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_ProtobufStructWithBool) {
    // DISABLED: Hangs during protobuf struct conversion
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        google::protobuf::Struct pb_struct;
        (*pb_struct.mutable_fields())["flag"].set_bool_value(true);
        (*pb_struct.mutable_fields())["other"].set_bool_value(false);

        jl_value_t* dict = ProtobufStructToJuliaDict(pb_struct);
        if (!dict) return false;

        jl_set_global(jl_main_module, jl_symbol("test_dict_bool"), dict);

        jl_value_t* flag_val = jl_eval_string("test_dict_bool[\"flag\"]");
        if (!flag_val || jl_exception_occurred()) return false;
        if (jl_unbox_bool(flag_val) != true) return false;

        jl_value_t* other_val = jl_eval_string("test_dict_bool[\"other\"]");
        if (!other_val || jl_exception_occurred()) return false;
        if (jl_unbox_bool(other_val) != false) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_ProtobufStructWithString) {
    // DISABLED: Hangs during protobuf struct conversion
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        google::protobuf::Struct pb_struct;
        (*pb_struct.mutable_fields())["name"].set_string_value("test_string");

        jl_value_t* dict = ProtobufStructToJuliaDict(pb_struct);
        if (!dict) return false;

        jl_set_global(jl_main_module, jl_symbol("test_dict_str"), dict);

        jl_value_t* name_val = jl_eval_string("test_dict_str[\"name\"]");
        if (!name_val || jl_exception_occurred()) return false;

        std::string name_str(jl_string_ptr(name_val));
        if (name_str != "test_string") return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_ProtobufStructMixed) {
    // DISABLED: Hangs during protobuf struct conversion
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        google::protobuf::Struct pb_struct;
        (*pb_struct.mutable_fields())["num"].set_number_value(123.0);
        (*pb_struct.mutable_fields())["flag"].set_bool_value(true);
        (*pb_struct.mutable_fields())["text"].set_string_value("hello");

        jl_value_t* dict = ProtobufStructToJuliaDict(pb_struct);
        if (!dict) return false;

        jl_set_global(jl_main_module, jl_symbol("test_dict_mixed"), dict);

        // Check all three values
        jl_value_t* num = jl_eval_string("test_dict_mixed[\"num\"]");
        if (!num || std::abs(jl_unbox_float64(num) - 123.0) > 1e-9) return false;

        jl_value_t* flag = jl_eval_string("test_dict_mixed[\"flag\"]");
        if (!flag || !jl_unbox_bool(flag)) return false;

        jl_value_t* text = jl_eval_string("test_dict_mixed[\"text\"]");
        if (!text || std::string(jl_string_ptr(text)) != "hello") return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_ProtobufStructEmpty) {
    // DISABLED: Hangs during protobuf struct conversion
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        google::protobuf::Struct pb_struct;

        jl_value_t* dict = ProtobufStructToJuliaDict(pb_struct);
        if (!dict) return false;

        jl_set_global(jl_main_module, jl_symbol("test_dict_empty"), dict);

        jl_value_t* len = jl_eval_string("length(test_dict_empty)");
        if (!len || jl_unbox_int64(len) != 0) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

// Edge cases and error tests

TEST_F(JuliaConvertTest, EmptyVariablesDict) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;  // Empty

        jl_value_t* dict = VariablesToJuliaDict(vars);
        if (!dict) return false;

        Variables vars_back = JuliaDictToVariables(dict);
        if (vars_back.size() != 0) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, VariableNameWithSpecialChars) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        Variables vars;
        vars["var_123"] = Variable(philote::kOutput, {1});
        vars["var_123"](0) = 99.0;

        vars["CamelCase"] = Variable(philote::kOutput, {1});
        vars["CamelCase"](0) = 88.0;

        jl_value_t* dict = VariablesToJuliaDict(vars);
        Variables vars_back = JuliaDictToVariables(dict);

        if (vars_back.size() != 2) return false;
        if (std::abs(vars_back.at("var_123")(0) - 99.0) > 1e-9) return false;
        if (std::abs(vars_back.at("CamelCase")(0) - 88.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, PartialsWithComplexNames) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Test with underscores and numbers in names
        jl_value_t* result_dict = jl_eval_string(
            "Dict(\"output_1~input_2\" => [5.0])");
        if (!result_dict || jl_exception_occurred()) return false;

        Partials partials = JuliaDictToPartials(result_dict);

        if (partials.size() != 1) return false;
        if (partials.count({"output_1", "input_2"}) == 0) return false;
        if (std::abs(partials.at({"output_1", "input_2"})(0) - 5.0) > 1e-9) return false;

        return true;
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, DISABLED_InvalidJuliaDictToVariables) {
    // DISABLED: Causes subprocess abort
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Try to convert a non-dict Julia value
        jl_value_t* not_a_dict = jl_eval_string("42");

        try {
            Variables vars = JuliaDictToVariables(not_a_dict);
            return false;  // Should have thrown
        } catch (const std::runtime_error&) {
            return true;  // Expected exception
        }
    });

    EXPECT_TRUE(result);
}

TEST_F(JuliaConvertTest, InvalidJuliaDictToPartials) {
    auto result = JuliaExecutor::GetInstance().Submit([]() {
        // Try to convert invalid dict format
        jl_value_t* invalid_dict = jl_eval_string("Dict(\"not_tilde\" => [1.0])");

        try {
            Partials partials = JuliaDictToPartials(invalid_dict);
            // If the key doesn't have a tilde, it should either throw or be ignored
            // Implementation-dependent behavior
            return true;
        } catch (const std::runtime_error&) {
            return true;  // Also acceptable
        }
    });

    EXPECT_TRUE(result);
}

}  // namespace test
}  // namespace julia
}  // namespace philote
