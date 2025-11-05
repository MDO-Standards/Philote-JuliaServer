// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include <grpc++/resource_quota.h>
#include <grpc++/server_builder.h>

#include <csignal>
#include <iostream>
#include <memory>

#include "julia_config.h"
#include "julia_explicit_discipline.h"
#include "julia_implicit_discipline.h"
#include "julia_runtime.h"

using philote::Discipline;
using philote::julia::JuliaExplicitDiscipline;
using philote::julia::JuliaImplicitDiscipline;
using philote::julia::JuliaRuntime;
using philote::julia::PhiloteConfig;

// Global server pointer for signal handler
std::unique_ptr<grpc::Server> g_server;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..."
              << std::endl;
    if (g_server) {
        g_server->Shutdown();
    }
}

int main(int argc, char** argv) {
    // Parse command line
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.yaml>\n";
        std::cerr << "\nStarts a Philote gRPC server hosting a Julia discipline.\n";
        std::cerr << "\nExample:\n";
        std::cerr << "  " << argv[0] << " paraboloid.yaml\n";
        return 1;
    }

    try {
        // 1. Load YAML configuration
        std::cout << "Loading configuration from " << argv[1] << "..." << std::endl;
        PhiloteConfig config = PhiloteConfig::FromYaml(argv[1]);
        config.Validate();

        std::cout << "Configuration loaded successfully:" << std::endl;
        std::cout << "  Discipline kind: " << config.discipline.kind << std::endl;
        std::cout << "  Julia file: " << config.discipline.julia_file << std::endl;
        std::cout << "  Julia type: " << config.discipline.julia_type << std::endl;
        std::cout << "  Server address: " << config.server.address << std::endl;
        std::cout << "  Max threads: " << config.server.max_threads << std::endl;

        // 2. Initialize Julia runtime (BEFORE creating gRPC server)
        std::cout << "\nInitializing Julia runtime..." << std::endl;
        JuliaRuntime::GetInstance();
        std::cout << "Julia runtime initialized successfully." << std::endl;

        // 3. Create discipline wrapper
        std::cout << "\nLoading Julia discipline..." << std::endl;
        std::unique_ptr<Discipline> discipline;

        if (config.discipline.kind == "explicit") {
            discipline = std::make_unique<JuliaExplicitDiscipline>(
                config.discipline);
        } else if (config.discipline.kind == "implicit") {
            discipline = std::make_unique<JuliaImplicitDiscipline>(
                config.discipline);
        } else {
            throw std::runtime_error("Invalid discipline kind: " +
                                    config.discipline.kind);
        }

        std::cout << "Julia discipline loaded successfully." << std::endl;

        // 4. Build gRPC server
        std::cout << "\nBuilding gRPC server..." << std::endl;
        grpc::ServerBuilder builder;

        // Set server address
        builder.AddListeningPort(config.server.address,
                                grpc::InsecureServerCredentials());

        // CRITICAL: Limit thread pool size for predictable Julia thread management
        grpc::ResourceQuota quota;
        quota.SetMaxThreads(config.server.max_threads);
        builder.SetResourceQuota(quota);

        // Register discipline services (uses Philote-Cpp infrastructure)
        discipline->RegisterServices(builder);

        // 5. Start server (creates thread pool HERE)
        g_server = builder.BuildAndStart();

        if (!g_server) {
            throw std::runtime_error("Failed to start gRPC server");
        }

        std::cout << "gRPC server built successfully." << std::endl;

        // Setup signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << "\n========================================" << std::endl;
        std::cout << "Julia discipline server listening on "
                  << config.server.address << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        std::cout << "========================================\n" << std::endl;

        // 6. Wait for shutdown signal
        g_server->Wait();

        std::cout << "\nServer shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }

    // 7. Julia cleanup happens in JuliaRuntime destructor
    return 0;
}
