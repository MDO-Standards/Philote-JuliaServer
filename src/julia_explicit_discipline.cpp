// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_explicit_discipline.h"

#include <stdexcept>

#include "julia_convert.h"
#include "julia_executor.h"
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
    std::cout << "[DEBUG] JuliaExplicitDiscipline constructor" << std::endl;
    Initialize();
    std::cout << "[DEBUG] JuliaExplicitDiscipline constructor complete" << std::endl;
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
    JuliaExecutor::GetInstance().Submit([this]() {
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

        // CRITICAL: Store module and discipline object as global Julia variables
        // This provides permanent GC rooting that works across all threads
        jl_module_t* main_module = jl_main_module;
        jl_set_global(main_module, jl_symbol("_philote_discipline_module"),
                      reinterpret_cast<jl_value_t*>(module_));
        jl_set_global(main_module, jl_symbol("_philote_discipline_obj"),
                      discipline_obj_);
    });
}

void JuliaExplicitDiscipline::Setup() {
    std::cout << "[DEBUG] JuliaExplicitDiscipline::Setup() called" << std::endl;
    // Execute on dedicated Julia thread
    try {
        std::cout << "[DEBUG] About to submit task to executor..." << std::endl;
        JuliaExecutor::GetInstance().Submit([this]() {
            std::cout << "[DEBUG] Setup lambda starting..." << std::endl;
            jl_value_t* discipline_obj = GetDisciplineObject();
            std::cout << "[DEBUG] Got discipline object: " << discipline_obj << std::endl;
            GCProtect protect(discipline_obj);

            // Call Julia setup!() function
            jl_function_t* setup_fn = GetJuliaFunction("setup!");
            std::cout << "[DEBUG] Got setup function: " << setup_fn << std::endl;
            if (!setup_fn) {
                throw std::runtime_error(
                    "Julia discipline missing required function: setup!()");
            }

            std::cout << "[DEBUG] Calling Julia setup!()..." << std::endl;
            jl_call1(setup_fn, discipline_obj);
            CheckJuliaException();
            std::cout << "[DEBUG] Julia setup!() completed" << std::endl;

            // Extract I/O metadata and register with Philote-Cpp
            ExtractIOMetadata();
            std::cout << "[DEBUG] ExtractIOMetadata completed" << std::endl;
        });
        std::cout << "[DEBUG] Setup completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Setup failed: " << e.what() << std::endl;
        throw;
    }
}

void JuliaExplicitDiscipline::ExtractIOMetadata() {
    // Called from Setup() which is already on Julia executor thread
    jl_value_t* discipline_obj = GetDisciplineObject();
    GCProtect protect(discipline_obj);

    // Get inputs metadata
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* inputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("inputs"));
    jl_value_t* outputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("outputs"));

    // Get inputs dict
    jl_value_t* inputs_dict = jl_call2(getproperty_fn, discipline_obj, inputs_sym);
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
    jl_value_t* outputs_dict = jl_call2(getproperty_fn, discipline_obj, outputs_sym);
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
    // Execute on dedicated Julia thread
    JuliaExecutor::GetInstance().Submit([this]() {
        jl_value_t* discipline_obj = GetDisciplineObject();
        GCProtect protect(discipline_obj);

        // Call Julia setup_partials!() if it exists
        jl_function_t* setup_partials_fn = GetJuliaFunction("setup_partials!");
        if (setup_partials_fn) {
            jl_call1(setup_partials_fn, discipline_obj);
            CheckJuliaException();
        }

        // Extract partials metadata
        ExtractPartialsMetadata();
    });
}

void JuliaExplicitDiscipline::ExtractPartialsMetadata() {
    // Called from SetupPartials() which is already on Julia executor thread
    jl_value_t* discipline_obj = GetDisciplineObject();
    GCProtect protect(discipline_obj);

    // Get partials metadata from discipline
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* partials_sym = reinterpret_cast<jl_value_t*>(jl_symbol("partials"));

    jl_value_t* partials_dict = jl_call2(getproperty_fn, discipline_obj, partials_sym);
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
    // Execute on dedicated Julia thread - NO CONCURRENCY
    outputs = JuliaExecutor::GetInstance().Submit([this, &inputs]() {
        // All Julia calls happen on single executor thread
        jl_value_t* discipline_obj = GetDisciplineObject();
        GCProtect protect(discipline_obj);

        jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);
        GCProtect protect_inputs(inputs_dict);

        jl_function_t* compute_fn = GetJuliaFunction("compute");
        if (!compute_fn) {
            throw std::runtime_error(
                "Julia discipline missing required function: compute()");
        }

        jl_value_t* result = jl_call2(compute_fn, discipline_obj, inputs_dict);
        CheckJuliaException();

        if (!result) {
            throw std::runtime_error("Julia compute() returned null");
        }

        GCProtect protect_result(result);
        return JuliaDictToVariables(result);
    });
}

void JuliaExplicitDiscipline::ComputePartials(const philote::Variables& inputs,
                                              philote::Partials& partials) {
    // Execute on dedicated Julia thread - NO CONCURRENCY
    partials = JuliaExecutor::GetInstance().Submit([this, &inputs]() {
        jl_value_t* discipline_obj = GetDisciplineObject();
        GCProtect protect(discipline_obj);

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
            jl_call2(compute_partials_fn, discipline_obj, inputs_dict);
        CheckJuliaException();

        if (!result) {
            throw std::runtime_error("Julia compute_partials() returned null");
        }

        GCProtect protect_result(result);
        return JuliaDictToPartials(result);
    });
}

void JuliaExplicitDiscipline::SetOptions(
    const google::protobuf::Struct& options) {
    // Execute on dedicated Julia thread
    JuliaExecutor::GetInstance().Submit([this, &options]() {
        jl_value_t* discipline_obj = GetDisciplineObject();
        GCProtect protect(discipline_obj);

        // Convert protobuf Struct to Julia Dict
        jl_value_t* options_dict = ProtobufStructToJuliaDict(options);
        GCProtect protect_options(options_dict);

        // Call Julia set_options!() if it exists
        jl_function_t* set_options_fn = GetJuliaFunction("set_options!");
        if (set_options_fn) {
            jl_call2(set_options_fn, discipline_obj, options_dict);
            CheckJuliaException();
        }
    });

    // Call parent to invoke Configure() (C++ only, not Julia)
    ExplicitDiscipline::SetOptions(options);
}

jl_value_t* JuliaExplicitDiscipline::GetDisciplineObject() {
    // Retrieve discipline object from Julia globals
    // This ensures thread-safe access without relying on C++ member pointers
    jl_module_t* main_module = jl_main_module;
    jl_value_t* discipline_obj = jl_get_global(main_module, jl_symbol("_philote_discipline_obj"));
    if (!discipline_obj) {
        throw std::runtime_error("Discipline object not found in Julia globals");
    }
    return discipline_obj;
}

jl_function_t* JuliaExplicitDiscipline::GetJuliaFunction(
    const std::string& name) {
    // Retrieve module from Julia globals EVERY time
    // This ensures thread-safe access without relying on C++ member pointers
    jl_module_t* main_module = jl_main_module;
    jl_value_t* module_val = jl_get_global(main_module, jl_symbol("_philote_discipline_module"));
    if (!module_val) {
        throw std::runtime_error("Module not found in Julia globals");
    }

    jl_module_t* module = reinterpret_cast<jl_module_t*>(module_val);
    jl_function_t* fn = jl_get_function(module, name.c_str());
    return fn;
}

}  // namespace julia
}  // namespace philote
