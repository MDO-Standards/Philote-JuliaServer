// Copyright 2025 Christopher A. Lupp
// Licensed under the Apache License, Version 2.0

#include "test_helpers.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "julia_config.h"

namespace philote {
namespace julia {
namespace test {

// JuliaTestServerManager implementation

JuliaTestServerManager::JuliaTestServerManager(const std::string& config_file, int port)
    : config_file_(config_file), port_(port), started_(false) {

    // Load configuration
    DisciplineConfig config = LoadDisciplineConfig(config_file_);

    // Override port if specified
    if (port_ == 0) {
        ServerConfig server_config = LoadServerConfig(config_file_);
        // Parse port from address (e.g., "[::]:50051" -> 50051)
        std::string addr = server_config.address;
        size_t colon_pos = addr.rfind(':');
        if (colon_pos != std::string::npos) {
            port_ = std::stoi(addr.substr(colon_pos + 1));
        } else {
            port_ = 50051;  // Default
        }
    }

    address_ = "localhost:" + std::to_string(port_);

    // Create discipline
    discipline_ = std::make_unique<JuliaExplicitDiscipline>(config);
}

JuliaTestServerManager::~JuliaTestServerManager() {
    Stop();
}

void JuliaTestServerManager::Start() {
    if (started_) {
        return;
    }

    // Build server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());

    // Create and register service
    server_ = std::make_unique<philote::DisciplineServer>(discipline_.get());
    builder.RegisterService(server_.get());

    // Start server in background thread
    std::unique_ptr<grpc::Server> grpc_server = builder.BuildAndStart();

    if (!grpc_server) {
        throw std::runtime_error("Failed to start test server on " + address_);
    }

    // Move server to member (note: we're simplifying by not actually running async)
    // For real async server, you'd need a thread

    started_ = true;

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void JuliaTestServerManager::Stop() {
    if (!started_) {
        return;
    }

    // Stop server
    if (server_) {
        // Server cleanup
        server_.reset();
    }

    started_ = false;
}

// Helper function implementations

std::string CreateTempJuliaFile(const std::string& content) {
    // Create temp file in /tmp
    std::string temp_pattern = "/tmp/julia_test_XXXXXX.jl";
    char* temp_path = new char[temp_pattern.size() + 1];
    std::strcpy(temp_path, temp_pattern.c_str());

    // Create unique temporary file
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        delete[] temp_path;
        throw std::runtime_error("Failed to create temporary Julia file");
    }

    // Write content
    std::string path(temp_path);
    delete[] temp_path;
    close(fd);

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open temporary file: " + path);
    }

    file << content;
    file.close();

    return path;
}

std::string GetTestDisciplinePath(const std::string& filename) {
    // Construct path to test disciplines
    std::filesystem::path examples_dir = std::filesystem::current_path() / "examples" / "test_disciplines";
    std::filesystem::path file_path = examples_dir / filename;

    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("Test discipline not found: " + file_path.string());
    }

    return file_path.string();
}

std::string CreateTempConfigFile(const std::string& julia_file,
                                  const std::string& julia_type,
                                  int port) {
    if (port == 0) {
        port = FindAvailablePort();
    }

    // Create temp YAML file
    std::string temp_pattern = "/tmp/julia_config_XXXXXX.yaml";
    char* temp_path = new char[temp_pattern.size() + 1];
    std::strcpy(temp_path, temp_pattern.c_str());

    int fd = mkstemp(temp_path);
    if (fd == -1) {
        delete[] temp_path;
        throw std::runtime_error("Failed to create temporary config file");
    }

    std::string path(temp_path);
    delete[] temp_path;
    close(fd);

    // Write YAML content
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open temporary config file: " + path);
    }

    file << "discipline:\n";
    file << "  kind: explicit\n";
    file << "  julia_file: " << julia_file << "\n";
    file << "  julia_type: " << julia_type << "\n";
    file << "\n";
    file << "server:\n";
    file << "  address: \"[::]:" << port << "\"\n";
    file << "  max_threads: 10\n";

    file.close();

    return path;
}

int FindAvailablePort() {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket for port finding");
    }

    // Bind to port 0 (let OS choose)
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        throw std::runtime_error("Failed to bind socket for port finding");
    }

    // Get chosen port
    socklen_t len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr*)&addr, &len) < 0) {
        close(sock);
        throw std::runtime_error("Failed to get socket name for port finding");
    }

    int port = ntohs(addr.sin_port);
    close(sock);

    return port;
}

std::shared_ptr<grpc::Channel> CreateTestChannel(const std::string& address) {
    return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

bool VerifyGradientCorrectness(philote::Discipline* discipline,
                                const philote::Variables& inputs,
                                const philote::Partials& analytical_partials,
                                double epsilon,
                                double tolerance) {
    // Compute outputs at base point
    philote::Variables base_outputs;
    for (const auto& var : discipline->var_meta()) {
        if (var.type() == philote::kOutput) {
            std::vector<size_t> shape;
            for (const auto& dim : var.shape()) {
                shape.push_back(dim);
            }
            base_outputs[var.name()] = philote::Variable(philote::kOutput, shape);
        }
    }
    discipline->Compute(inputs, base_outputs);

    // For each partial derivative
    bool all_correct = true;
    for (const auto& [key, analytical_partial] : analytical_partials) {
        const std::string& output_name = key.first;
        const std::string& input_name = key.second;

        // Get input variable
        auto input_it = inputs.find(input_name);
        if (input_it == inputs.end()) {
            continue;
        }
        const philote::Variable& input_var = input_it->second;

        // Compute numerical partial for each input element
        philote::Variable numerical_partial = analytical_partial;  // Same shape

        for (size_t i = 0; i < input_var.Size(); ++i) {
            // Perturb input
            philote::Variables perturbed_inputs = inputs;
            perturbed_inputs[input_name](i) += epsilon;

            // Compute perturbed output
            philote::Variables perturbed_outputs;
            for (const auto& var : discipline->var_meta()) {
                if (var.type() == philote::kOutput) {
                    std::vector<size_t> shape;
                    for (const auto& dim : var.shape()) {
                        shape.push_back(dim);
                    }
                    perturbed_outputs[var.name()] = philote::Variable(philote::kOutput, shape);
                }
            }
            discipline->Compute(perturbed_inputs, perturbed_outputs);

            // Compute numerical derivative
            const auto& base_output = base_outputs[output_name];
            const auto& perturbed_output = perturbed_outputs[output_name];

            for (size_t j = 0; j < base_output.Size(); ++j) {
                size_t flat_index = j * input_var.Size() + i;
                if (flat_index < numerical_partial.Size()) {
                    numerical_partial(flat_index) =
                        (perturbed_output(j) - base_output(j)) / epsilon;
                }
            }
        }

        // Compare analytical vs numerical
        for (size_t i = 0; i < analytical_partial.Size(); ++i) {
            double analytical_val = analytical_partial(i);
            double numerical_val = numerical_partial(i);
            double abs_diff = std::abs(analytical_val - numerical_val);
            double rel_diff = abs_diff / (std::abs(analytical_val) + 1e-10);

            if (rel_diff > tolerance) {
                std::cerr << "Gradient mismatch for d" << output_name << "/d" << input_name
                          << "[" << i << "]: analytical=" << analytical_val
                          << ", numerical=" << numerical_val
                          << ", rel_diff=" << rel_diff << std::endl;
                all_correct = false;
            }
        }
    }

    return all_correct;
}

void ExpectVariableEquals(const philote::Variable& expected,
                          const philote::Variable& actual,
                          double tolerance) {
    ASSERT_EQ(expected.Size(), actual.Size())
        << "Variable sizes differ: expected " << expected.Size()
        << ", got " << actual.Size();

    const auto& expected_shape = expected.Shape();
    const auto& actual_shape = actual.Shape();
    ASSERT_EQ(expected_shape.size(), actual_shape.size())
        << "Variable dimensions differ";

    for (size_t i = 0; i < expected_shape.size(); ++i) {
        ASSERT_EQ(expected_shape[i], actual_shape[i])
            << "Shape mismatch at dimension " << i;
    }

    for (size_t i = 0; i < expected.Size(); ++i) {
        EXPECT_NEAR(expected(i), actual(i), tolerance)
            << "Value mismatch at index " << i;
    }
}

void ExpectVariablesEqual(const philote::Variables& expected,
                          const philote::Variables& actual,
                          double tolerance) {
    ASSERT_EQ(expected.size(), actual.size())
        << "Number of variables differs: expected " << expected.size()
        << ", got " << actual.size();

    for (const auto& [name, expected_var] : expected) {
        ASSERT_TRUE(actual.count(name) > 0)
            << "Variable '" << name << "' not found in actual";

        const auto& actual_var = actual.at(name);
        ExpectVariableEquals(expected_var, actual_var, tolerance);
    }
}

void ExpectPartialsEqual(const philote::Partials& expected,
                         const philote::Partials& actual,
                         double tolerance) {
    ASSERT_EQ(expected.size(), actual.size())
        << "Number of partials differs: expected " << expected.size()
        << ", got " << actual.size();

    for (const auto& [key, expected_partial] : expected) {
        ASSERT_TRUE(actual.count(key) > 0)
            << "Partial d" << key.first << "/d" << key.second << " not found in actual";

        const auto& actual_partial = actual.at(key);
        ExpectVariableEquals(expected_partial, actual_partial, tolerance);
    }
}

}  // namespace test
}  // namespace julia
}  // namespace philote
