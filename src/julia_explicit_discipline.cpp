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
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Loading Julia file: " << config_.julia_file << std::endl;
        std::cerr.flush();

        // Load Julia file
        module_ = JuliaRuntime::GetInstance().LoadJuliaFile(config_.julia_file);
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Julia file loaded, module = " << module_ << std::endl;
        std::cerr.flush();

        // Get Julia type
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Getting Julia type: " << config_.julia_type << std::endl;
        std::cerr.flush();

        jl_value_t* type =
            jl_get_global(module_, jl_symbol(config_.julia_type.c_str()));
        if (!type) {
            throw std::runtime_error("Julia type not found: " + config_.julia_type);
        }

        std::cerr << "[DEBUG] LoadJuliaDiscipline: Got type, type = " << type << std::endl;
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Type name: " << jl_typeof_str(type) << std::endl;
        std::cerr.flush();


        // Instantiate discipline (call constructor)
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Calling constructor (jl_call0)..." << std::endl;
        std::cerr.flush();

        discipline_obj_ = jl_call0(reinterpret_cast<jl_function_t*>(type));

        std::cerr << "[DEBUG] LoadJuliaDiscipline: Constructor called, checking for exception..." << std::endl;
        std::cerr.flush();

        CheckJuliaException();

        std::cerr << "[DEBUG] LoadJuliaDiscipline: No exception, discipline_obj_ = " << discipline_obj_ << std::endl;
        std::cerr.flush();

        if (!discipline_obj_) {
            throw std::runtime_error("Failed to instantiate Julia discipline: " +
                                     config_.julia_type);
        }

        // No need to store in globals - just keep as C++ member variable
        // GC protection is handled by GCProtect when we use it
        std::cerr << "[DEBUG] LoadJuliaDiscipline: Complete!" << std::endl;
        std::cerr.flush();
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

            // NOTE: discipline_obj is already globally rooted (stored as member variable)
            // No need for GCProtect here - it causes segfault with adopted threads

            // Call Julia setup!() function
            jl_function_t* setup_fn = GetJuliaFunction("setup!");
            std::cout << "[DEBUG] Got setup function: " << setup_fn << std::endl;
            if (!setup_fn) {
                throw std::runtime_error(
                    "Julia discipline missing required function: setup!()");
            }

            std::cout << "[DEBUG] Calling Julia setup!()..." << std::endl;
            std::cout << "[DEBUG] Discipline object address: " << discipline_obj << std::endl;
            std::cout << "[DEBUG] Discipline object type: " << jl_typeof_str(discipline_obj) << std::endl;
            std::cout << "[DEBUG] Setup function address: " << setup_fn << std::endl;
            std::cout << "[DEBUG] Setup function type: " << jl_typeof_str((jl_value_t*)setup_fn) << std::endl;
            std::cout.flush();

            // Verify object is valid
            if (!discipline_obj) {
                throw std::runtime_error("Discipline object is NULL!");
            }
            if (!setup_fn) {
                throw std::runtime_error("Setup function is NULL!");
            }

            // Now try the actual call
            std::cout << "[DEBUG] About to call jl_call1(setup_fn, discipline_obj)..." << std::endl;
            std::cout.flush();

            jl_call1(setup_fn, discipline_obj);

            std::cout << "[DEBUG] jl_call1 returned!" << std::endl;
            std::cout.flush();

            std::cout << "[DEBUG] Checking for Julia exceptions..." << std::endl;
            std::cout.flush();
            std::cout << "[DEBUG] Julia setup!() completed" << std::endl;

            // Extract I/O metadata and register with Philote-Cpp
            ExtractIOMetadata();
            std::cout << "[DEBUG] ExtractIOMetadata completed" << std::endl;

            // Declare all partials: dy/dx for all outputs and inputs
            // For now, we declare partials for all output-input pairs
            // TODO: Make this configurable through Julia interface
            std::cout << "[DEBUG] Declaring partials..." << std::endl;
            const auto& inputs_meta = var_meta();
            const auto& outputs_meta = var_meta();
            for (const auto& output_var : outputs_meta) {
                if (output_var.type() == philote::kOutput) {
                    for (const auto& input_var : inputs_meta) {
                        if (input_var.type() == philote::kInput) {
                            std::cout << "[DEBUG] Declaring partial d" << output_var.name()
                                      << "/d" << input_var.name() << std::endl;
                            DeclarePartials(output_var.name(), input_var.name());
                        }
                    }
                }
            }
            std::cout << "[DEBUG] Partials declared" << std::endl;
        });
        std::cout << "[DEBUG] Setup completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Setup failed: " << e.what() << std::endl;
        throw;
    }
}

void JuliaExplicitDiscipline::ExtractIOMetadata() {
    // Called from Setup() which is already on Julia executor thread
    // NOTE: discipline_obj is globally rooted, no GCProtect needed
    // All temporary Julia objects here are short-lived and immediately processed
    std::cout << "[DEBUG] ExtractIOMetadata: Starting..." << std::endl;
    jl_value_t* discipline_obj = GetDisciplineObject();

    // Get inputs metadata
    std::cout << "[DEBUG] ExtractIOMetadata: Getting getproperty function..." << std::endl;
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* inputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("inputs"));
    jl_value_t* outputs_sym = reinterpret_cast<jl_value_t*>(jl_symbol("outputs"));

    // Get inputs dict
    std::cout << "[DEBUG] ExtractIOMetadata: Getting inputs dict..." << std::endl;
    jl_value_t* inputs_dict = jl_call2(getproperty_fn, discipline_obj, inputs_sym);
    CheckJuliaException();
    std::cout << "[DEBUG] ExtractIOMetadata: Got inputs dict: " << inputs_dict << std::endl;

    if (inputs_dict) {
        std::cout << "[DEBUG] ExtractIOMetadata: Processing inputs dict..." << std::endl;

        // Iterate through inputs and add to discipline
        std::cout << "[DEBUG] ExtractIOMetadata: Getting Julia functions..." << std::endl;
        jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
        jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
        jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

        std::cout << "[DEBUG] ExtractIOMetadata: Getting keys..." << std::endl;
        jl_value_t* keys = jl_call1(keys_fn, inputs_dict);
        CheckJuliaException();

        std::cout << "[DEBUG] ExtractIOMetadata: Collecting keys to array..." << std::endl;
        jl_array_t* keys_array = reinterpret_cast<jl_array_t*>(
            jl_call1(collect_fn, keys));
        CheckJuliaException();

        size_t num_inputs = jl_array_len(keys_array);
        std::cout << "[DEBUG] ExtractIOMetadata: Found " << num_inputs << " inputs" << std::endl;

        for (size_t i = 0; i < num_inputs; ++i) {
            std::cout << "[DEBUG] ExtractIOMetadata: Processing input " << i << std::endl;
            jl_value_t* key = jl_array_ptr_ref(keys_array, i);
            if (!jl_is_string(key)) continue;

            std::string name = jl_string_ptr(key);
            std::cout << "[DEBUG] ExtractIOMetadata: Input name: " << name << std::endl;

            // Get metadata for this input
            std::cout << "[DEBUG] ExtractIOMetadata: Getting metadata for " << name << std::endl;
            jl_value_t* meta = jl_call2(getindex_fn, inputs_dict, key);
            CheckJuliaException();
            std::cout << "[DEBUG] ExtractIOMetadata: Got metadata: " << meta << std::endl;

            // Metadata is a tuple (shape_vector, units_string)
            // Access tuple elements by index, not by property name
            std::cout << "[DEBUG] ExtractIOMetadata: Extracting shape and units from tuple..." << std::endl;

            // meta should be a tuple with 2 elements: ([shape...], "units")
            if (!jl_is_tuple(meta) || jl_nfields(meta) != 2) {
                std::cerr << "[ERROR] Expected metadata to be a 2-element tuple" << std::endl;
                continue;
            }

            jl_value_t* shape_val = jl_fieldref(meta, 0);  // First element: shape vector
            jl_value_t* units_val = jl_fieldref(meta, 1);  // Second element: units string

            // Convert shape to vector (int64_t for Philote-Cpp)
            // Shape can be either a Julia Vector or a Tuple
            std::vector<int64_t> shape;
            std::cout << "[DEBUG] ExtractIOMetadata: Converting shape..." << std::endl;
            if (jl_is_array(shape_val)) {
                // Shape is a Vector{Int}
                jl_array_t* shape_array = reinterpret_cast<jl_array_t*>(shape_val);
                size_t ndims = jl_array_len(shape_array);
                int64_t* data = jl_array_data(shape_array, int64_t);
                for (size_t d = 0; d < ndims; ++d) {
                    shape.push_back(data[d]);
                }
            } else if (jl_is_tuple(shape_val)) {
                // Shape is a Tuple
                size_t ndims = jl_nfields(shape_val);
                for (size_t d = 0; d < ndims; ++d) {
                    jl_value_t* dim = jl_fieldref(shape_val, d);
                    shape.push_back(jl_unbox_int64(dim));
                }
            }

            // Get units string
            std::string units;
            std::cout << "[DEBUG] ExtractIOMetadata: Converting units..." << std::endl;
            if (jl_is_string(units_val)) {
                units = jl_string_ptr(units_val);
            }

            // Add input to discipline
            std::cout << "[DEBUG] ExtractIOMetadata: Adding input " << name << " with shape size " << shape.size() << " and units \"" << units << "\"" << std::endl;
            AddInput(name, shape, units);
            std::cout << "[DEBUG] ExtractIOMetadata: Added input " << name << std::endl;
        }
    }

    // Get outputs metadata (same process)
    std::cout << "[DEBUG] ExtractIOMetadata: Getting outputs dict..." << std::endl;
    jl_value_t* outputs_dict = jl_call2(getproperty_fn, discipline_obj, outputs_sym);
    CheckJuliaException();
    std::cout << "[DEBUG] ExtractIOMetadata: Got outputs dict: " << outputs_dict << std::endl;

    if (outputs_dict) {
        std::cout << "[DEBUG] ExtractIOMetadata: Processing outputs dict..." << std::endl;

        jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
        jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
        jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

        jl_value_t* keys = jl_call1(keys_fn, outputs_dict);
        CheckJuliaException();

        jl_array_t* keys_array = reinterpret_cast<jl_array_t*>(
            jl_call1(collect_fn, keys));
        CheckJuliaException();

        size_t num_outputs = jl_array_len(keys_array);
        std::cout << "[DEBUG] ExtractIOMetadata: Found " << num_outputs << " outputs" << std::endl;

        for (size_t i = 0; i < num_outputs; ++i) {
            std::cout << "[DEBUG] ExtractIOMetadata: Processing output " << i << std::endl;
            jl_value_t* key = jl_array_ptr_ref(keys_array, i);
            if (!jl_is_string(key)) continue;

            std::string name = jl_string_ptr(key);
            std::cout << "[DEBUG] ExtractIOMetadata: Output name: " << name << std::endl;

            jl_value_t* meta = jl_call2(getindex_fn, outputs_dict, key);
            CheckJuliaException();

            // Metadata is a tuple (shape_vector, units_string) - access by index
            if (!jl_is_tuple(meta) || jl_nfields(meta) != 2) {
                std::cerr << "[ERROR] Expected output metadata to be a 2-element tuple" << std::endl;
                continue;
            }

            jl_value_t* shape_val = jl_fieldref(meta, 0);
            jl_value_t* units_val = jl_fieldref(meta, 1);

            std::vector<int64_t> shape;
            if (jl_is_array(shape_val)) {
                jl_array_t* shape_array = reinterpret_cast<jl_array_t*>(shape_val);
                size_t ndims = jl_array_len(shape_array);
                int64_t* data = jl_array_data(shape_array, int64_t);
                for (size_t d = 0; d < ndims; ++d) {
                    shape.push_back(data[d]);
                }
            } else if (jl_is_tuple(shape_val)) {
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

            std::cout << "[DEBUG] ExtractIOMetadata: Adding output " << name << " with shape size " << shape.size() << " and units \"" << units << "\"" << std::endl;
            AddOutput(name, shape, units);
            std::cout << "[DEBUG] ExtractIOMetadata: Added output " << name << std::endl;
        }
    }
    std::cout << "[DEBUG] ExtractIOMetadata: Complete!" << std::endl;
}

void JuliaExplicitDiscipline::SetupPartials() {
    // Execute on dedicated Julia thread
    JuliaExecutor::GetInstance().Submit([this]() {
        jl_value_t* discipline_obj = GetDisciplineObject();

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

    // Get partials metadata from discipline
    jl_function_t* getproperty_fn = jl_get_function(jl_base_module, "getproperty");
    jl_value_t* partials_sym = reinterpret_cast<jl_value_t*>(jl_symbol("partials"));

    jl_value_t* partials_dict = jl_call2(getproperty_fn, discipline_obj, partials_sym);
    CheckJuliaException();

    if (!partials_dict || !1) {
        return;  // No partials defined
    }


    // Iterate through partials
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");

    jl_value_t* keys = jl_call1(keys_fn, partials_dict);
    CheckJuliaException();

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

        jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);

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

        return JuliaDictToVariables(result);
    });
}

void JuliaExplicitDiscipline::ComputePartials(const philote::Variables& inputs,
                                              philote::Partials& partials) {
    std::cout << "[DEBUG] ComputePartials() called" << std::endl;
    std::cout.flush();
    // Execute on dedicated Julia thread - NO CONCURRENCY
    partials = JuliaExecutor::GetInstance().Submit([this, &inputs]() {
        std::cout << "[DEBUG] ComputePartials lambda starting..." << std::endl;
        std::cout.flush();
        jl_value_t* discipline_obj = GetDisciplineObject();

        std::cout << "[DEBUG] Converting inputs to Julia dict..." << std::endl;
        std::cout.flush();
        // Convert inputs
        jl_value_t* inputs_dict = VariablesToJuliaDict(inputs);

        std::cout << "[DEBUG] Getting compute_partials function..." << std::endl;
        std::cout.flush();
        // Call Julia compute_partials function
        jl_function_t* compute_partials_fn = GetJuliaFunction("compute_partials");
        if (!compute_partials_fn) {
            throw std::runtime_error(
                "Julia discipline missing function: compute_partials()");
        }

        std::cout << "[DEBUG] Calling Julia compute_partials()..." << std::endl;
        std::cout.flush();
        jl_value_t* result =
            jl_call2(compute_partials_fn, discipline_obj, inputs_dict);
        CheckJuliaException();

        std::cout << "[DEBUG] compute_partials returned: " << result << std::endl;
        std::cout.flush();
        if (!result) {
            throw std::runtime_error("Julia compute_partials() returned null");
        }

        std::cout << "[DEBUG] Converting result to C++ partials..." << std::endl;
        std::cout.flush();
        philote::Partials result_partials = JuliaDictToPartials(result);

        std::cout << "[DEBUG] Converted " << result_partials.size() << " partial(s):" << std::endl;
        for (const auto& [key, value] : result_partials) {
            std::cout << "[DEBUG]   d" << key.first << "/d" << key.second
                      << " = [" << value.Size() << " elements]";
            if (value.Size() > 0) {
                std::cout << " first value = " << value(0);
            }
            std::cout << std::endl;
        }
        std::cout.flush();

        return result_partials;
    });
    std::cout << "[DEBUG] ComputePartials() completed, returning " << partials.size() << " partial(s)" << std::endl;
    std::cout.flush();
}

void JuliaExplicitDiscipline::SetOptions(
    const google::protobuf::Struct& options) {
    // Execute on dedicated Julia thread
    JuliaExecutor::GetInstance().Submit([this, &options]() {
        jl_value_t* discipline_obj = GetDisciplineObject();

        // Convert protobuf Struct to Julia Dict
        jl_value_t* options_dict = ProtobufStructToJuliaDict(options);

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
    // Return C++ member variable directly
    // GC protection is handled by GCProtect when we use it
    if (!discipline_obj_) {
        throw std::runtime_error("Discipline object not initialized");
    }
    return discipline_obj_;
}

jl_function_t* JuliaExplicitDiscipline::GetJuliaFunction(
    const std::string& name) {
    // Functions are always defined in Main module (where we include() the Julia file)
    jl_module_t* main_module = jl_main_module;
    jl_function_t* fn = jl_get_function(main_module, name.c_str());
    return fn;
}

}  // namespace julia
}  // namespace philote
