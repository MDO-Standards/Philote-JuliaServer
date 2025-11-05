// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_CONFIG_H
#define PHILOTE_JULIA_SERVER_JULIA_CONFIG_H

#include <map>
#include <string>
#include <variant>

namespace philote {
namespace julia {

/**
 * @brief Configuration for a Julia discipline
 */
struct DisciplineConfig {
    std::string kind;        // "explicit" or "implicit"
    std::string julia_file;  // Absolute path to .jl file
    std::string julia_type;  // Julia type name to instantiate
    std::map<std::string, std::variant<double, int, bool, std::string>>
        options;  // Optional discipline options

    /**
     * @brief Validate discipline configuration
     * @throws std::runtime_error if configuration is invalid
     */
    void Validate() const;
};

/**
 * @brief Configuration for the gRPC server
 */
struct ServerConfig {
    std::string address = "[::]:50051";  // Server address
    int max_threads = 10;  // Maximum worker threads for thread pool

    /**
     * @brief Validate server configuration
     * @throws std::runtime_error if configuration is invalid
     */
    void Validate() const;
};

/**
 * @brief Complete Philote-JuliaServer configuration
 */
struct PhiloteConfig {
    DisciplineConfig discipline;
    ServerConfig server;

    /**
     * @brief Load configuration from YAML file
     * @param yaml_path Path to YAML configuration file
     * @return Loaded configuration
     * @throws std::runtime_error if file cannot be loaded or parsed
     */
    static PhiloteConfig FromYaml(const std::string& yaml_path);

    /**
     * @brief Save configuration to YAML file
     * @param yaml_path Path to write YAML file
     * @throws std::runtime_error if file cannot be written
     */
    void ToYaml(const std::string& yaml_path) const;

    /**
     * @brief Validate entire configuration
     * @throws std::runtime_error if configuration is invalid
     */
    void Validate() const;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_CONFIG_H
