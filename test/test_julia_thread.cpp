// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "julia_runtime.h"
#include "julia_thread.h"

using philote::julia::JuliaRuntime;
using philote::julia::JuliaThreadGuard;

TEST(JuliaThreadTest, AdoptionBasic) {
    // Initialize Julia first
    JuliaRuntime::GetInstance();

    // Test adoption
    EXPECT_FALSE(JuliaThreadGuard::IsAdopted());

    {
        JuliaThreadGuard guard;
        EXPECT_TRUE(JuliaThreadGuard::IsAdopted());
    }

    // Still adopted after guard goes out of scope
    EXPECT_TRUE(JuliaThreadGuard::IsAdopted());
}

TEST(JuliaThreadTest, IdempotentAdoption) {
    JuliaRuntime::GetInstance();

    JuliaThreadGuard guard1;
    EXPECT_TRUE(JuliaThreadGuard::IsAdopted());

    // Second adoption should be safe
    JuliaThreadGuard guard2;
    EXPECT_TRUE(JuliaThreadGuard::IsAdopted());
}
