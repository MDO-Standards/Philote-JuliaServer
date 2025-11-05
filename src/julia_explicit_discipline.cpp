// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_explicit_discipline.h"

#include <stdexcept>

#include "julia_convert.h"
#include "julia_gc.h"
#include "julia_runtime.h"
#include "julia_thread.h"

namespace philote {
namespace julia {

thread_local bool JuliaExplicitDiscipline::julia_adopted_ = false;

JuliaExplicitDiscipline::JuliaExplicitDiscipline(
    const DisciplineConfig& config)
    : config_(config), module_(nullptr), discipline_obj_(nullptr) {
    // Discipline construction happens on main thread
    // Julia initialization and loading will happen in Initialize()
}

JuliaExplicitDiscipline::~JuliaExplicitDiscipline() {
    // Cleanup Julia objects
    // Note: Julia runtime cleanup handled by JuliaRuntime singleton
}

void JuliaExplicitDiscipline::Initialize() {
    // Call parent initialization
    ExplicitDiscipline::Initialize();

    // Initialize Julia runtime (singleton, idempotent)
    JuliaRuntime::GetInstance();

    // Load Julia discipline file and instantiate discipline
    LoadJuliaDiscipline();
}

void JuliaExplicitDiscipline::LoadJuliaDiscipline() {
    JuliaThreadGuard guard;  // Adopt main thread

    // Load Julia file
    module_ = JuliaRuntime::GetInstance().LoadJuliaFile(config_.julia_file);

    // Get Julia type
    jl_value_t* type =
        jl_get_global(module_, jl_symbol(config_.julia_type.c_str()));
    if (!type) {
        throw std::runtime_error("Julia type not found: " + config_.julia_type);
    }

    GCProtect protect_type(type);

    // Instantiate discipline (call constructor)
    discipline_obj_ = jl_call0(reinterpret_cast<jl_function_t*>(type));
    CheckJuliaException();

    if (!discipline_obj_) {
        throw std::runtime_error("Failed to instantiate Julia discipline: " +
                                 config_.julia_type);
    }

    // Protect from GC (Julia 1.12 API - use 2-argument version)
    // We'll keep a permanent reference by not letting the protect go out of scope
    // Note: In practice, discipline_obj_ stays alive for server lifetime
}

void JuliaExplicitDiscipline::Setup() {
    JuliaThreadGuard guard;  // Adopt main thread
    GCProtect protect(discipline_obj_);

    // Call Julia setup!() function
    jl_function_t* setup_fn = GetJuliaFunction("setup!");
    if (!setup_fn) {
        throw std::runtime_error(
            "Julia discipline missing required function: setup!()");
    }

    jl_call1(setup_fn, discipline_obj_);
    CheckJuliaException();

    // Extract I/O metadata and register with Philote-Cpp
    ExtractIOMetadata();
}

void JuliaExplicitDiscipline::ExtractIOMetadata() {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    // Get inputs metadata
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* inputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("inputs"));
    jl_value_t* outputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("outputs"));

    // Get inputs dict
    jl_value_t* inputs_dict = jl_call2(getproperty_fn, discipline_obj_, inputs_sym);
    CheckJuliaException();

    if (inputs_dict) {
        GCProtect protect_inputs(inputs_dict);

        // Iterate through inputs and add to discipline
        jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
        jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
        jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

        jl_value_t* keys = jl_call1(keys_fn, inputs_dict);
        CheckJuliaException();
        GCProtect protect_keys(keys);

        jl_array_t* keys_array = reinterpret_cast<jl_array_t*>(
            jl_call1(collect_fn, keys));
        CheckJuliaException();

        size_t num_inputs = jl_array_len(keys_array);

        for (size_t i = 0; i < num_inputs; ++i) {
            jl_value_t* key = jl_array_ptr_ref(keys_array, i);
            if (!jl_is_string(key)) continue;

            std::string name = jl_string_ptr(key);

            // Get metadata for this input
            jl_value_t* meta = jl_call2(getindex_fn, inputs_dict, key);
            CheckJuliaException();

            // Extract shape and units from metadata
            // Assume metadata has 'shape' and 'units' fields
            jl_value_t* shape_sym = reinterpret_cast<jl_value_t*>(jl_symbol("shape"));
            jl_value_t* units_sym = reinterpret_cast<jl_value_t*>(jl_symbol("units"));

            jl_value_t* shape_val = jl_call2(getproperty_fn, meta, shape_sym);
            jl_value_t* units_val = jl_call2(getproperty_fn, meta, units_sym);

            // Convert shape to vector (int64_t for Philote-Cpp)
            std::vector<int64_t> shape;
            if (jl_is_tuple(shape_val)) {
                size_t ndims = jl_nfields(shape_val);
                for (size_t d = 0; d < ndims; ++d) {
                    jl_value_t* dim = jl_fieldref(shape_val, d);
                    shape.push_back(jl_unbox_int64(dim));
                }
            }

            // Get units string
            std::string units;
            if (jl_is_string(units_val)) {
                units = jl_string_ptr(units_val);
            }

            // Add input to discipline
            AddInput(name, shape, units);
        }
    }

    // Get outputs metadata (same process)
    jl_value_t* outputs_dict = jl_call2(getproperty_fn, discipline_obj_, outputs_sym);
    CheckJuliaException();

    if (outputs_dict && 1) {
        GCProtect protect_outputs(outputs_dict);

        jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
        jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
        jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

        jl_value_t* keys = jl_call1(keys_fn, outputs_dict);
        CheckJuliaException();
        GCProtect protect_keys(keys);

        jl_array_t* keys_array = reinterpret_cast<jl_array_t*>(
            jl_call1(collect_fn, keys));
        CheckJuliaException();

        size_t num_outputs = jl_array_len(keys_array);

        for (size_t i = 0; i < num_outputs; ++i) {
            jl_value_t* key = jl_array_ptr_ref(keys_array, i);
            if (!jl_is_string(key)) continue;

            std::string name = jl_string_ptr(key);

            jl_value_t* meta = jl_call2(getindex_fn, outputs_dict, key);
            CheckJuliaException();

            jl_value_t* shape_sym = reinterpret_cast<jl_value_t*>(jl_symbol("shape"));
            jl_value_t* units_sym = reinterpret_cast<jl_value_t*>(jl_symbol("units"));

            jl_value_t* shape_val = jl_call2(getproperty_fn, meta, shape_sym);
            jl_value_t* units_val = jl_call2(getproperty_fn, meta, units_sym);

            std::vector<int64_t> shape;
            if (jl_is_tuple(shape_val)) {
                size_t ndims = jl_nfields(shape_val);
                for (size_t d = 0; d < ndims; ++d) {
                    jl_value_t* dim = jl_fieldref(shape_val, d);
                    shape.push_back(jl_unbox_int64(dim));
                }
            }

            std::string units;
            if (jl_is_string(units_val)) {
                units = jl_string_ptr(units_val);
            }

            AddOutput(name, shape, units);
        }
    }
}

void JuliaExplicitDiscipline::SetupPartials() {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    // Call Julia setup_partials!() if it exists
    jl_function_t* setup_partials_fn = GetJuliaFunction("setup_partials!");
    if (setup_partials_fn) {
        jl_call1(setup_partials_fn, discipline_obj_);
        CheckJuliaException();
    }

    // Extract partials metadata
    ExtractPartialsMetadata();
}

void JuliaExplicitDiscipline::ExtractPartialsMetadata() {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    // Get partials metadata from discipline
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* partials_sym = reinterpret_cast<jl_value_t*>(jl_symbol("partials"));

    jl_value_t* partials_dict = jl_call2(getproperty_fn, discipline_obj_, partials_sym);
    CheckJuliaException();

    if (!partials_dict || !1) {
        return;  // No partials defined
    }

    GCProtect protect_partials(partials_dict);

    // Iterate through partials
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");

    jl_value_t* keys = jl_call1(keys_fn, partials_dict);
    CheckJuliaException();
    GCProtect protect_keys(keys);

    jl_array_t* keys_array = reinterpret_cast<jl_array_t*>(
        jl_call1(collect_fn, keys));
    CheckJuliaException();

    size_t num_partials = jl_array_len(keys_array);

    for (size_t i = 0; i < num_partials; ++i) {
        jl_value_t* key = jl_array_ptr_ref(keys_array, i);

        // Key should be a tuple (output, input)
        if (!jl_is_tuple(key) || jl_nfields(key) != 2) continue;

        jl_value_t* output_name = jl_fieldref(key, 0);
        jl_value_t* input_name = jl_fieldref(key, 1);

        if (!jl_is_string(output_name) || !jl_is_string(input_name)) continue;

        std::string output = jl_string_ptr(output_name);
        std::string input = jl_string_ptr(input_name);

        // Declare partial
        DeclarePartials(output, input);
    }
}

void JuliaExplicitDiscipline::Compute(const philote::Variables& inputs,
                                      philote::Variables& outputs) {
    // CRITICAL: Adopt this gRPC worker thread
    JuliaThreadGuard guard;

    // CRITICAL: Serialize Julia calls for thread safety
    std::lock_guard<std::mutex> lock(compute_mutex_);

    // Protect Julia objects from GC
    GCProtect protect(discipline_obj_);

    // Convert C++ Variables to Julia Dict
    jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
    GCProtect protect_inputs(inputs_dict);

    // Call Julia compute function
    jl_function_t* compute_fn = GetJuliaFunction("compute");
    if (!compute_fn) {
        throw std::runtime_error(
            "Julia discipline missing required function: compute()");
    }

    jl_value_t* result = jl_call2(compute_fn, discipline_obj_, inputs_dict);
    CheckJuliaException();

    if (!result) {
        throw std::runtime_error("Julia compute() returned null");
    }

    GCProtect protect_result(result);

    // Convert Julia Dict back to C++ Variables
    outputs = JuliaDictToVariables(result);
}

void JuliaExplicitDiscipline::ComputePartials(const philote::Variables& inputs,
                                              philote::Partials& partials) {
    JuliaThreadGuard guard;
    std::lock_guard<std::mutex> lock(compute_mutex_);

    GCProtect protect(discipline_obj_);

    // Convert inputs
    jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
    GCProtect protect_inputs(inputs_dict);

    // Call Julia compute_partials function
    jl_function_t* compute_partials_fn = GetJuliaFunction("compute_partials");
    if (!compute_partials_fn) {
        throw std::runtime_error(
            "Julia discipline missing function: compute_partials()");
    }

    jl_value_t* result =
        jl_call2(compute_partials_fn, discipline_obj_, inputs_dict);
    CheckJuliaException();

    if (!result) {
        throw std::runtime_error("Julia compute_partials() returned null");
    }

    GCProtect protect_result(result);

    // Convert Julia Dict to Partials
    partials = JuliaDictToPartials(result);
}

void JuliaExplicitDiscipline::SetOptions(
    const google::protobuf::Struct& options) {
    JuliaThreadGuard guard;
    GCProtect protect(discipline_obj_);

    // Convert protobuf Struct to Julia Dict
    jl_value_t* options_dict = ProtobufStructToJuliaDict(options);
    GCProtect protect_options(options_dict);

    // Call Julia set_options!() if it exists
    jl_function_t* set_options_fn = GetJuliaFunction("set_options!");
    if (set_options_fn) {
        jl_call2(set_options_fn, discipline_obj_, options_dict);
        CheckJuliaException();
    }

    // Call parent to invoke Configure()
    ExplicitDiscipline::SetOptions(options);
}

jl_function_t* JuliaExplicitDiscipline::GetJuliaFunction(
    const std::string& name) {
    jl_function_t* fn = jl_get_function(module_, name.c_str());
    return fn;
}

}  // namespace julia
}  // namespace philote
