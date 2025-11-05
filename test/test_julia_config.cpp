// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include "julia_config.h"

using philote::julia::PhiloteConfig;
using philote::julia::DisciplineConfig;
using philote::julia::ServerConfig;

TEST(JuliaConfigTest, ValidateKind) {
    DisciplineConfig config;
    config.kind = "invalid";
    config.julia_file = "/tmp/test.jl";
    config.julia_type = "TestDiscipline";

    EXPECT_THROW(config.Validate(), std::runtime_error);

    config.kind = "explicit";
    // Still throws because file doesn't exist
    EXPECT_THROW(config.Validate(), std::runtime_error);
}

TEST(JuliaConfigTest, ValidateThreads) {
    ServerConfig config;
    config.max_threads = 0;

    EXPECT_THROW(config.Validate(), std::runtime_error);

    config.max_threads = 10;
    EXPECT_NO_THROW(config.Validate());
}
