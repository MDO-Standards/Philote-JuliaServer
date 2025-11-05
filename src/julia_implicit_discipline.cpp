// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_implicit_discipline.h"

#include "julia_convert.h"
#include "julia_gc.h"
#include "julia_runtime.h"
#include "julia_thread.h"

namespace philote {
namespace julia {

JuliaImplicitDiscipline::JuliaImplicitDiscipline(
    const DisciplineConfig& config)
    : config_(config), module_(nullptr), discipline_obj_(nullptr) {
}

JuliaImplicitDiscipline::~JuliaImplicitDiscipline() {
}

void JuliaImplicitDiscipline::Initialize() {
    ImplicitDiscipline::Initialize();
    JuliaRuntime::GetInstance();
    LoadJuliaDiscipline();
}

void JuliaImplicitDiscipline::LoadJuliaDiscipline() {
    JuliaThreadGuard guard;

    module_ = JuliaRuntime::GetInstance().LoadJuliaFile(config_.julia_file);

    jl_value_t* type =
        jl_get_global(module_, jl_symbol(config_.julia_type.c_str()));
    if (!type) {
        throw std::runtime_error("Julia type not found: " + config_.julia_type);
    }

    GCProtect protect_type(type);

    discipline_obj_ = jl_call0(reinterpret_cast<jl_function_t*>(type));
    CheckJuliaException();

    if (!discipline_obj_) {
        throw std::runtime_error("Failed to instantiate Julia discipline");
    }

    // CRITICAL: Store module and discipline object as global Julia variables
    // This provides permanent GC rooting that works across all threads
    jl_module_t* main_module = jl_main_module;
    jl_set_global(main_module, jl_symbol("_philote_discipline_module"),
                  reinterpret_cast<jl_value_t*>(module_));
    jl_set_global(main_module, jl_symbol("_philote_discipline_obj"),
                  discipline_obj_);
}

void JuliaImplicitDiscipline::Setup() {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    jl_function_t* setup_fn = GetJuliaFunction("setup!");
    if (!setup_fn) {
        throw std::runtime_error("Julia discipline missing setup!()");
    }

    jl_call1(setup_fn, discipline_obj_);
    CheckJuliaException();

    ExtractIOMetadata();
}

void JuliaImplicitDiscipline::ExtractIOMetadata() {
    // Same implementation as JuliaExplicitDiscipline
    // Extract inputs and outputs from Julia discipline metadata
    // For brevity, simplified version here
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    // TODO: Implement full metadata extraction
    // For now, assume discipline registers its own I/O
}

void JuliaImplicitDiscipline::SetupPartials() {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    jl_function_t* setup_partials_fn = GetJuliaFunction("setup_partials!");
    if (setup_partials_fn) {
        jl_call1(setup_partials_fn, discipline_obj_);
        CheckJuliaException();
    }

    ExtractPartialsMetadata();
}

void JuliaImplicitDiscipline::ExtractPartialsMetadata() {
    // TODO: Implement partials metadata extraction
}

void JuliaImplicitDiscipline::ComputeResiduals(
    const philote::Variables& inputs,
    const philote::Variables& outputs,
    philote::Variables& residuals) {
    JuliaThreadGuard guard;
    std::lock_guard<std::mutex> lock(compute_mutex_);

    GCProtect protect(discipline_obj_);

    jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
    jl_value_t* outputs_dict = VariablesToJuliaDict(outputs);
    GCProtect protect_dicts({inputs_dict, outputs_dict});

    jl_function_t* compute_residuals_fn = GetJuliaFunction("compute_residuals");
    if (!compute_residuals_fn) {
        throw std::runtime_error("Missing compute_residuals()");
    }

    jl_value_t** args = new jl_value_t*[3];
    args[0] = discipline_obj_;
    args[1] = inputs_dict;
    args[2] = outputs_dict;

    jl_value_t* result = jl_call(compute_residuals_fn, args, 3);
    delete[] args;

    CheckJuliaException();

    GCProtect protect_result(result);
    residuals = JuliaDictToVariables(result);
}

void JuliaImplicitDiscipline::SolveResiduals(
    const philote::Variables& inputs,
    philote::Variables& outputs) {
    JuliaThreadGuard guard;
    std::lock_guard<std::mutex> lock(compute_mutex_);

    GCProtect protect(discipline_obj_);

    jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
    GCProtect protect_inputs(inputs_dict);

    jl_function_t* solve_residuals_fn = GetJuliaFunction("solve_residuals");
    if (!solve_residuals_fn) {
        throw std::runtime_error("Missing solve_residuals()");
    }

    jl_value_t* result = jl_call2(solve_residuals_fn, discipline_obj_, inputs_dict);
    CheckJuliaException();

    GCProtect protect_result(result);
    outputs = JuliaDictToVariables(result);
}

void JuliaImplicitDiscipline::ComputeResidualGradients(
    const philote::Variables& inputs,
    const philote::Variables& outputs,
    philote::Partials& partials) {
    JuliaThreadGuard guard;
    std::lock_guard<std::mutex> lock(compute_mutex_);

    GCProtect protect(discipline_obj_);

    jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
    jl_value_t* outputs_dict = VariablesToJuliaDict(outputs);
    GCProtect protect_dicts({inputs_dict, outputs_dict});

    jl_function_t* compute_residual_gradients_fn =
        GetJuliaFunction("compute_residual_gradients");
    if (!compute_residual_gradients_fn) {
        throw std::runtime_error("Missing compute_residual_gradients()");
    }

    jl_value_t** args = new jl_value_t*[3];
    args[0] = discipline_obj_;
    args[1] = inputs_dict;
    args[2] = outputs_dict;

    jl_value_t* result = jl_call(compute_residual_gradients_fn, args, 3);
    delete[] args;

    CheckJuliaException();

    GCProtect protect_result(result);
    partials = JuliaDictToPartials(result);
}

void JuliaImplicitDiscipline::SetOptions(
    const google::protobuf::Struct& options) {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    jl_value_t* options_dict = ProtobufStructToJuliaDict(options);
    GCProtect protect_options(options_dict);

    jl_function_t* set_options_fn = GetJuliaFunction("set_options!");
    if (set_options_fn) {
        jl_call2(set_options_fn, discipline_obj_, options_dict);
        CheckJuliaException();
    }

    ImplicitDiscipline::SetOptions(options);
}

jl_function_t* JuliaImplicitDiscipline::GetJuliaFunction(
    const std::string& name) {
    return jl_get_function(module_, name.c_str());
}

}  // namespace julia
}  // namespace philote
