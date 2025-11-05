// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_IMPLICIT_DISCIPLINE_H
#define PHILOTE_JULIA_SERVER_JULIA_IMPLICIT_DISCIPLINE_H

#include <julia.h>
#include <philote/implicit.h>

#include <mutex>

#include "julia_config.h"

namespace philote {
namespace julia {

/**
 * @brief Wrapper for Julia implicit disciplines
 *
 * Similar to JuliaExplicitDiscipline but for implicit disciplines.
 * Thread-safe implementation with mutex serialization.
 *
 * @note Inherits from philote::ImplicitDiscipline (Philote-Cpp library)
 */
class JuliaImplicitDiscipline : public philote::ImplicitDiscipline {
public:
    explicit JuliaImplicitDiscipline(const DisciplineConfig& config);
    ~JuliaImplicitDiscipline() override;

    JuliaImplicitDiscipline(const JuliaImplicitDiscipline&) = delete;
    JuliaImplicitDiscipline& operator=(const JuliaImplicitDiscipline&) = delete;

protected:
    void Initialize() override;
    void Setup() override;
    void SetupPartials() override;

    void ComputeResiduals(const philote::Variables& inputs,
                         const philote::Variables& outputs,
                         philote::Variables& residuals) override;

    void SolveResiduals(const philote::Variables& inputs,
                       philote::Variables& outputs) override;

    void ComputeResidualGradients(const philote::Variables& inputs,
                                 const philote::Variables& outputs,
                                 philote::Partials& partials) override;

    void SetOptions(const google::protobuf::Struct& options) override;

private:
    void LoadJuliaDiscipline();
    void ExtractIOMetadata();
    void ExtractPartialsMetadata();
    jl_function_t* GetJuliaFunction(const std::string& name);

    DisciplineConfig config_;
    jl_module_t* module_;
    jl_value_t* discipline_obj_;
    mutable std::mutex compute_mutex_;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_IMPLICIT_DISCIPLINE_H
