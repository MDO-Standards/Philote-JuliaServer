// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>
#include "test_helpers.h"

using philote::julia::test::JuliaTestEnvironment;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Register Julia environment - this initializes Julia runtime and executor once
    ::testing::AddGlobalTestEnvironment(new JuliaTestEnvironment);

    return RUN_ALL_TESTS();
}
