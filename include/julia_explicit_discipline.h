// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_EXPLICIT_DISCIPLINE_H
#define PHILOTE_JULIA_SERVER_JULIA_EXPLICIT_DISCIPLINE_H

#include <julia.h>
#include <philote/explicit.h>

#include <mutex>

#include "julia_config.h"

namespace philote {
namespace julia {

/**
 * @brief Wrapper for Julia explicit disciplines
 *
 * This class wraps a Julia-based explicit discipline, allowing it to be hosted
 * via gRPC using the Philote-Cpp infrastructure. It handles:
 * - Thread-safe Julia calls from gRPC worker threads
 * - Data conversion between C++ and Julia
 * - Julia GC protection
 * - Exception propagation
 *
 * Thread Safety:
 * - Initialize(), Setup(), SetupPartials() called from main thread
 * - Compute(), ComputePartials() called from gRPC worker threads concurrently
 * - Uses mutex to serialize Julia calls (required for thread safety)
 * - Each thread adopts itself via JuliaThreadGuard
 *
 * @note Inherits from philote::ExplicitDiscipline (Philote-Cpp library)
 */
class JuliaExplicitDiscipline : public philote::ExplicitDiscipline {
public:
    /**
     * @brief Constructor - loads Julia discipline
     * @param config Discipline configuration
     * @throws std::runtime_error if Julia discipline cannot be loaded
     */
    explicit JuliaExplicitDiscipline(const DisciplineConfig& config);

    /**
     * @brief Destructor - cleans up Julia objects
     */
    ~JuliaExplicitDiscipline() override;

    // Prevent copying and moving
    JuliaExplicitDiscipline(const JuliaExplicitDiscipline&) = delete;
    JuliaExplicitDiscipline& operator=(const JuliaExplicitDiscipline&) = delete;
    JuliaExplicitDiscipline(JuliaExplicitDiscipline&&) = delete;
    JuliaExplicitDiscipline& operator=(JuliaExplicitDiscipline&&) = delete;

protected:
    /**
     * @brief Initialize discipline (called from main thread)
     *
     * Initializes Julia runtime and loads the Julia discipline file.
     */
    void Initialize() override;

    /**
     * @brief Setup discipline I/O (called from main thread)
     *
     * Calls Julia setup!() function to configure inputs and outputs,
     * then extracts metadata to register with Philote-Cpp.
     */
    void Setup() override;

    /**
     * @brief Setup partial derivatives (called from main thread)
     *
     * Calls Julia setup_partials!() if available, then extracts
     * partial derivative metadata.
     */
    void SetupPartials() override;

    /**
     * @brief Compute discipline outputs (called from gRPC worker threads)
     *
     * Thread-safe method that:
     * 1. Adopts calling thread for Julia
     * 2. Converts C++ inputs to Julia Dict
     * 3. Calls Julia compute() function
     * 4. Converts Julia Dict outputs back to C++
     *
     * @param inputs Input variables
     * @param outputs Output variables (populated by this method)
     */
    void Compute(const philote::Variables& inputs,
                 philote::Variables& outputs) override;

    /**
     * @brief Compute partial derivatives (called from gRPC worker threads)
     *
     * Thread-safe method that calls Julia compute_partials() function.
     *
     * @param inputs Input variables
     * @param partials Partial derivatives (populated by this method)
     */
    void ComputePartials(const philote::Variables& inputs,
                        philote::Partials& partials) override;

    /**
     * @brief Set discipline options (called from main thread)
     *
     * Converts protobuf Struct to Julia Dict and calls Julia set_options!()
     * if available.
     *
     * @param options Options as protobuf Struct
     */
    void SetOptions(const google::protobuf::Struct& options) override;

private:
    /**
     * @brief Load Julia discipline from file
     *
     * Loads the Julia file and instantiates the discipline type.
     * Must be called after Julia runtime is initialized.
     */
    void LoadJuliaDiscipline();

    /**
     * @brief Extract I/O metadata from Julia discipline
     *
     * Queries Julia discipline for inputs and outputs, then registers
     * them with Philote-Cpp using AddInput() and AddOutput().
     */
    void ExtractIOMetadata();

    /**
     * @brief Extract partial derivative metadata from Julia discipline
     *
     * Queries Julia discipline for available partials, then registers
     * them with Philote-Cpp using DeclarePartials().
     */
    void ExtractPartialsMetadata();

    /**
     * @brief Get Julia function from discipline module
     * @param name Function name
     * @return Julia function or nullptr if not found
     */
    jl_function_t* GetJuliaFunction(const std::string& name);

    DisciplineConfig config_;
    jl_module_t* module_;           // Julia module containing discipline
    jl_value_t* discipline_obj_;    // Julia discipline instance

    // Thread safety: Mutex to serialize Julia calls
    mutable std::mutex compute_mutex_;

    // Thread-local adoption tracking (defined in cpp file)
    static thread_local bool julia_adopted_;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_EXPLICIT_DISCIPLINE_H
