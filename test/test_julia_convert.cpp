// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "julia_convert.h"
#include "julia_runtime.h"
#include "julia_thread.h"

using philote::Variables;
using philote::Variable;
using philote::julia::VariablesToJuliaDict;
using philote::julia::JuliaDictToVariables;
using philote::julia::JuliaRuntime;
using philote::julia::JuliaThreadGuard;

TEST(JuliaConvertTest, RoundtripScalar) {
    JuliaRuntime::GetInstance();
    JuliaThreadGuard guard;

    Variables vars;
    vars["x"] = Variable({1});
    vars["x"](0) = 42.0;

    jl_value_t* dict = VariablesToJuliaDict(vars);
    ASSERT_NE(dict, nullptr);

    Variables vars_back = JuliaDictToVariables(dict);

    EXPECT_EQ(vars_back.size(), 1);
    EXPECT_TRUE(vars_back.count("x") > 0);
    EXPECT_DOUBLE_EQ(vars_back.at("x")(0), 42.0);
}

TEST(JuliaConvertTest, RoundtripVector) {
    JuliaRuntime::GetInstance();
    JuliaThreadGuard guard;

    Variables vars;
    vars["vec"] = Variable({3});
    vars["vec"](0) = 1.0;
    vars["vec"](1) = 2.0;
    vars["vec"](2) = 3.0;

    jl_value_t* dict = VariablesToJuliaDict(vars);
    Variables vars_back = JuliaDictToVariables(dict);

    ASSERT_TRUE(vars_back.count("vec") > 0);
    EXPECT_EQ(vars_back.at("vec").Size(), 3);
    EXPECT_DOUBLE_EQ(vars_back.at("vec")(0), 1.0);
    EXPECT_DOUBLE_EQ(vars_back.at("vec")(1), 2.0);
    EXPECT_DOUBLE_EQ(vars_back.at("vec")(2), 3.0);
}
