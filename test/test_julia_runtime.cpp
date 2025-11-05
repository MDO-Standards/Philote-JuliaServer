// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "julia_runtime.h"

using philote::julia::JuliaRuntime;

TEST(JuliaRuntimeTest, Singleton) {
    JuliaRuntime& runtime1 = JuliaRuntime::GetInstance();
    JuliaRuntime& runtime2 = JuliaRuntime::GetInstance();

    EXPECT_EQ(&runtime1, &runtime2);
    EXPECT_TRUE(runtime1.IsInitialized());
}

TEST(JuliaRuntimeTest, EvalString) {
    JuliaRuntime& runtime = JuliaRuntime::GetInstance();

    jl_value_t* result = runtime.EvalString("2 + 2");
    ASSERT_NE(result, nullptr);

    int value = jl_unbox_int64(result);
    EXPECT_EQ(value, 4);
}
