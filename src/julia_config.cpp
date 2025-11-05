// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_config.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace philote {
namespace julia {

void DisciplineConfig::Validate() const {
    if (kind != "explicit" && kind != "implicit") {
        throw std::runtime_error(
            "Invalid discipline kind: '" + kind +
            "'. Must be 'explicit' or 'implicit'");
    }

    if (julia_file.empty()) {
        throw std::runtime_error("julia_file cannot be empty");
    }

    if (julia_type.empty()) {
        throw std::runtime_error("julia_type cannot be empty");
    }

    // Check if file exists
    if (!std::filesystem::exists(julia_file)) {
        throw std::runtime_error("Julia file does not exist: " + julia_file);
    }
}

void ServerConfig::Validate() const {
    if (max_threads < 1) {
        throw std::runtime_error("max_threads must be >= 1");
    }

    if (address.empty()) {
        throw std::runtime_error("server address cannot be empty");
    }
}

void PhiloteConfig::Validate() const {
    discipline.Validate();
    server.Validate();
}

PhiloteConfig PhiloteConfig::FromYaml(const std::string& yaml_path) {
    // Check if file exists
    if (!std::filesystem::exists(yaml_path)) {
        throw std::runtime_error("YAML file does not exist: " + yaml_path);
    }

    // Get directory of YAML file (for resolving relative paths)
    std::filesystem::path yaml_dir =
        std::filesystem::path(yaml_path).parent_path();

    // Load YAML
    YAML::Node config;
    try {
        config = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to parse YAML: " +
                                 std::string(e.what()));
    }

    PhiloteConfig result;

    // Parse discipline section (required)
    if (!config["discipline"]) {
        throw std::runtime_error("Missing required 'discipline' section");
    }

    const YAML::Node& disc = config["discipline"];

    // Parse kind (required)
    if (!disc["kind"]) {
        throw std::runtime_error("Missing required field: discipline.kind");
    }
    result.discipline.kind = disc["kind"].as<std::string>();

    // Parse julia_file (required)
    if (!disc["julia_file"]) {
        throw std::runtime_error(
            "Missing required field: discipline.julia_file");
    }
    std::string julia_file = disc["julia_file"].as<std::string>();

    // Resolve relative path based on YAML file location
    std::filesystem::path julia_path(julia_file);
    if (julia_path.is_relative()) {
        julia_path = yaml_dir / julia_path;
    }
    result.discipline.julia_file = julia_path.string();

    // Parse julia_type (required)
    if (!disc["julia_type"]) {
        throw std::runtime_error(
            "Missing required field: discipline.julia_type");
    }
    result.discipline.julia_type = disc["julia_type"].as<std::string>();

    // Parse options (optional)
    if (disc["options"] && disc["options"].IsMap()) {
        for (const auto& opt : disc["options"]) {
            std::string key = opt.first.as<std::string>();
            const YAML::Node& value = opt.second;

            // Determine value type and store
            if (value.IsScalar()) {
                try {
                    // Try as double first
                    result.discipline.options[key] = value.as<double>();
                } catch (...) {
                    try {
                        // Try as bool
                        result.discipline.options[key] = value.as<bool>();
                    } catch (...) {
                        // Default to string
                        result.discipline.options[key] =
                            value.as<std::string>();
                    }
                }
            }
        }
    }

    // Parse server section (optional)
    if (config["server"]) {
        const YAML::Node& srv = config["server"];

        if (srv["address"]) {
            result.server.address = srv["address"].as<std::string>();
        }

        if (srv["max_threads"]) {
            result.server.max_threads = srv["max_threads"].as<int>();
        }
    }

    // Validate configuration
    result.Validate();

    return result;
}

void PhiloteConfig::ToYaml(const std::string& yaml_path) const {
    YAML::Emitter out;

    out << YAML::BeginMap;

    // Discipline section
    out << YAML::Key << "discipline";
    out << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "kind" << YAML::Value << discipline.kind;
    out << YAML::Key << "julia_file" << YAML::Value << discipline.julia_file;
    out << YAML::Key << "julia_type" << YAML::Value << discipline.julia_type;

    if (!discipline.options.empty()) {
        out << YAML::Key << "options";
        out << YAML::Value << YAML::BeginMap;
        for (const auto& [key, value] : discipline.options) {
            out << YAML::Key << key;
            out << YAML::Value;
            std::visit(
                [&out](auto&& arg) {
                    out << arg;
                },
                value);
        }
        out << YAML::EndMap;
    }

    out << YAML::EndMap;

    // Server section
    out << YAML::Key << "server";
    out << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "address" << YAML::Value << server.address;
    out << YAML::Key << "max_threads" << YAML::Value << server.max_threads;
    out << YAML::EndMap;

    out << YAML::EndMap;

    // Write to file
    std::ofstream file(yaml_path);
    if (!file) {
        throw std::runtime_error("Could not open file for writing: " +
                                 yaml_path);
    }
    file << out.c_str();
}

}  // namespace julia
}  // namespace philote
