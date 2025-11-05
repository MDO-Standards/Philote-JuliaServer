// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_RUNTIME_H
#define PHILOTE_JULIA_SERVER_JULIA_RUNTIME_H

#include <atomic>
#include <julia.h>
#include <mutex>
#include <string>

namespace philote {
namespace julia {

/**
 * @brief Singleton class managing Julia runtime initialization and shutdown
 *
 * This class ensures Julia is initialized exactly once in the application
 * lifecycle, before any gRPC server threads are created. It provides
 * thread-safe access to the Julia runtime.
 *
 * @note Thread Safety: Initialization uses std::call_once for thread-safe
 *       single initialization. The instance itself is thread-safe.
 */
class JuliaRuntime {
public:
    /**
     * @brief Get the singleton instance, initializing Julia if needed
     * @return Reference to the singleton instance
     */
    static JuliaRuntime& GetInstance();

    /**
     * @brief Check if Julia has been initialized
     * @return true if Julia is initialized, false otherwise
     */
    bool IsInitialized() const { return initialized_.load(); }

    /**
     * @brief Get Julia's Main module
     * @return Pointer to jl_main_module
     */
    jl_module_t* GetMainModule() const { return jl_main_module; }

    /**
     * @brief Get Julia's Base module
     * @return Pointer to jl_base_module
     */
    jl_module_t* GetBaseModule() const { return jl_base_module; }

    /**
     * @brief Load a Julia source file
     * @param filepath Absolute path to .jl file
     * @return Pointer to the loaded module (Main module after include)
     * @throws std::runtime_error if file cannot be loaded
     */
    jl_module_t* LoadJuliaFile(const std::string& filepath);

    /**
     * @brief Evaluate Julia code string
     * @param code Julia code to evaluate
     * @return Result of evaluation
     * @throws std::runtime_error if evaluation fails
     */
    jl_value_t* EvalString(const std::string& code);

    // Prevent copying and moving
    JuliaRuntime(const JuliaRuntime&) = delete;
    JuliaRuntime& operator=(const JuliaRuntime&) = delete;
    JuliaRuntime(JuliaRuntime&&) = delete;
    JuliaRuntime& operator=(JuliaRuntime&&) = delete;

    /**
     * @brief Destructor - cleans up Julia runtime
     */
    ~JuliaRuntime();

private:
    /**
     * @brief Private constructor - initializes Julia
     */
    JuliaRuntime();

    std::atomic<bool> initialized_{false};
    static std::once_flag init_flag_;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_RUNTIME_H
