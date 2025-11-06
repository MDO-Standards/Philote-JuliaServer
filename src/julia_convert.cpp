// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_convert.h"

#include <stdexcept>
#include <sstream>

#include "julia_gc.h"

namespace philote {
namespace julia {

void CheckJuliaException() {
    if (jl_exception_occurred()) {
        std::string msg = GetJuliaExceptionString();
        jl_exception_clear();  // Clear exception for next call
        throw std::runtime_error(msg);
    }
}

std::string GetJuliaExceptionString() {
    jl_value_t* ex = jl_exception_occurred();
    if (!ex) {
        return "Unknown Julia exception";
    }

    // Get exception type
    std::string msg = std::string(jl_typeof_str(ex));

    std::cerr << "[DEBUG] Julia exception type: " << msg << std::endl;
    std::cerr.flush();

    // Try to get detailed error message using Julia's display system
    // We need to be careful not to trigger another exception
    jl_function_t* sprint_fn = jl_get_function(jl_base_module, "sprint");
    jl_function_t* showerror_fn = jl_get_function(jl_base_module, "showerror");

    if (sprint_fn && showerror_fn) {
        std::cerr << "[DEBUG] Calling sprint(showerror, ex)..." << std::endl;
        std::cerr.flush();

        // Clear any previous exceptions before calling sprint
        jl_exception_clear();

        jl_value_t* msg_str = jl_call2(sprint_fn, reinterpret_cast<jl_value_t*>(showerror_fn), ex);

        // Check if sprint itself threw an exception
        if (jl_exception_occurred()) {
            std::cerr << "[DEBUG] sprint(showerror) threw an exception, using basic error" << std::endl;
            std::cerr.flush();
            jl_exception_clear();
            return msg;
        }

        if (msg_str && jl_is_string(msg_str)) {
            std::string detailed_msg = jl_string_ptr(msg_str);
            std::cerr << "[DEBUG] Got detailed error message: " << detailed_msg << std::endl;
            std::cerr.flush();
            return detailed_msg;
        } else {
            std::cerr << "[DEBUG] sprint returned non-string result" << std::endl;
            std::cerr.flush();
        }
    } else {
        std::cerr << "[DEBUG] Could not find sprint or showerror functions" << std::endl;
        std::cerr.flush();
    }

    return msg;
}

jl_value_t* VariablesToJuliaDict(const philote::Variables& vars) {
    // Get Dict constructor
    // Create empty Dict{String, Vector{Float64}} with proper type parameters
    // First get the Dict type constructor
    jl_value_t* dict_type = jl_get_global(jl_base_module, jl_symbol("Dict"));
    if (!dict_type) {
        throw std::runtime_error("Could not find Base.Dict type");
    }

    // Create the parameterized type Dict{String, Vector{Float64}}
    jl_value_t* string_type = reinterpret_cast<jl_value_t*>(jl_string_type);
    jl_value_t* vector_float64_type = jl_apply_array_type(
        reinterpret_cast<jl_value_t*>(jl_float64_type), 1);

    jl_value_t* dict_params[2] = {string_type, vector_float64_type};
    jl_value_t* dict_parameterized = jl_apply_type(dict_type, dict_params, 2);
    CheckJuliaException();

    // Create empty instance of Dict{String, Vector{Float64}}
    jl_value_t* dict = jl_call0(reinterpret_cast<jl_function_t*>(dict_parameterized));
    CheckJuliaException();


    // Get setindex! function for adding to dict
    jl_function_t* setindex_fn =
        jl_get_function(jl_base_module, "setindex!");
    if (!setindex_fn) {
        throw std::runtime_error("Could not find Base.setindex! function");
    }

    // Convert each variable
    for (const auto& [name, var] : vars) {
        // Create Julia array from Variable data
        const auto& shape = var.Shape();
        size_t total_size = var.Size();

        // Create Julia array type
        jl_value_t* array_type = jl_apply_array_type(
            reinterpret_cast<jl_value_t*>(jl_float64_type), shape.size());

        // Create dimensions tuple
        jl_svec_t* dims = jl_alloc_svec(shape.size());

        for (size_t i = 0; i < shape.size(); ++i) {
            jl_svecset(dims, i, jl_box_int64(shape[i]));
        }

        // Create array
        jl_array_t* jl_array = jl_alloc_array_1d(array_type, total_size);

        // Copy data (C++ row-major to Julia column-major)
        double* jl_data = jl_array_data(jl_array, double);

        // For 1D arrays, direct copy
        if (shape.size() == 1) {
            for (size_t i = 0; i < total_size; ++i) {
                jl_data[i] = var(i);
            }
        } else if (shape.size() == 2) {
            // For 2D: transpose from row-major to column-major
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    jl_data[j * rows + i] = var(i * cols + j);
                }
            }
        } else {
            // For higher dimensions, use direct copy (assume compatible layout)
            for (size_t i = 0; i < total_size; ++i) {
                jl_data[i] = var(i);
            }
        }

        // Reshape array if multi-dimensional
        if (shape.size() > 1) {
            jl_function_t* reshape_fn =
                jl_get_function(jl_base_module, "reshape");
            jl_value_t** reshape_args = new jl_value_t*[shape.size() + 1];
            reshape_args[0] = reinterpret_cast<jl_value_t*>(jl_array);
            for (size_t i = 0; i < shape.size(); ++i) {
                reshape_args[i + 1] = jl_box_int64(shape[i]);
            }

            jl_array = reinterpret_cast<jl_array_t*>(
                jl_call(reshape_fn, reshape_args, shape.size() + 1));
            delete[] reshape_args;
            CheckJuliaException();
        }

        // Add to dictionary: dict[name] = array
        jl_value_t* key = jl_cstr_to_string(name.c_str());

        jl_call3(setindex_fn, dict, reinterpret_cast<jl_value_t*>(jl_array),
                 key);
        CheckJuliaException();
    }

    return dict;
}

philote::Variables JuliaDictToVariables(jl_value_t* dict) {
    if (!dict) {
        throw std::runtime_error("Expected Julia Dict, got null");
    }
    // Note: We'll trust that it's a dict-like object that supports keys/getindex

    philote::Variables vars;

    // Get dict keys and values
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

    if (!keys_fn || !getindex_fn) {
        throw std::runtime_error("Could not find Base.keys or getindex");
    }

    jl_value_t* keys = jl_call1(keys_fn, dict);
    CheckJuliaException();

    // Convert keys to array
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
    jl_array_t* keys_array =
        reinterpret_cast<jl_array_t*>(jl_call1(collect_fn, keys));
    CheckJuliaException();

    size_t num_keys = jl_array_len(keys_array);

    // Iterate through keys
    for (size_t i = 0; i < num_keys; ++i) {
        jl_value_t* key = jl_array_ptr_ref(keys_array, i);
        if (!jl_is_string(key)) {
            throw std::runtime_error("Dict key is not a string");
        }

        std::string name = jl_string_ptr(key);

        // Get value
        jl_value_t* value = jl_call2(getindex_fn, dict, key);
        CheckJuliaException();

        if (!jl_is_array(value)) {
            throw std::runtime_error("Dict value for '" + name +
                                     "' is not an array");
        }

        jl_array_t* jl_array = reinterpret_cast<jl_array_t*>(value);

        // Get array dimensions
        size_t ndims = jl_array_ndims(jl_array);
        std::vector<size_t> shape(ndims);
        for (size_t d = 0; d < ndims; ++d) {
            shape[d] = jl_array_dim(jl_array, d);
        }

        // Get array data
        double* jl_data = reinterpret_cast<double*>(jl_array_data(jl_array, double));
        size_t total_size = jl_array_len(jl_array);

        // Create Variable
        philote::Variable var(philote::kOutput, shape);

        // Copy data (Julia column-major to C++ row-major)
        if (ndims == 1) {
            // Direct copy for 1D
            for (size_t i = 0; i < total_size; ++i) {
                var(i) = jl_data[i];
            }
        } else if (ndims == 2) {
            // Transpose for 2D
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    var(i * cols + j) = jl_data[j * rows + i];
                }
            }
        } else {
            // Direct copy for higher dimensions
            for (size_t i = 0; i < total_size; ++i) {
                var(i) = jl_data[i];
            }
        }

        vars[name] = var;
    }

    return vars;
}

philote::Partials JuliaDictToPartials(jl_value_t* dict) {
    std::cerr << "[DEBUG] JuliaDictToPartials: Starting..." << std::endl;
    std::cerr.flush();
    if (!dict) {
        throw std::runtime_error("Expected Julia Dict for partials, got null");
    }

    philote::Partials partials;

    std::cerr << "[DEBUG] JuliaDictToPartials: Getting Julia functions..." << std::endl;
    std::cerr.flush();
    // Expect flat dict format: Dict{String, Vector{Float64}}
    // Keys are encoded as "output~input"
    // NOTE: Variable names cannot contain '~' (reserved as delimiter)
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");

    std::cerr << "[DEBUG] JuliaDictToPartials: Getting keys..." << std::endl;
    std::cerr.flush();
    jl_value_t* keys = jl_call1(keys_fn, dict);
    CheckJuliaException();

    jl_array_t* keys_array =
        reinterpret_cast<jl_array_t*>(jl_call1(collect_fn, keys));
    CheckJuliaException();

    size_t num_keys = jl_array_len(keys_array);
    std::cerr << "[DEBUG] JuliaDictToPartials: Found " << num_keys << " partial(s)" << std::endl;
    std::cerr.flush();

    for (size_t i = 0; i < num_keys; ++i) {
        jl_value_t* key = jl_array_ptr_ref(keys_array, i);
        if (!jl_is_string(key)) {
            throw std::runtime_error("Partials key must be a string");
        }

        std::string encoded_key = jl_string_ptr(key);
        std::cerr << "[DEBUG] JuliaDictToPartials: Processing key: " << encoded_key << std::endl;
        std::cerr.flush();

        // Parse "output~input" format (~ is reserved, cannot be in variable names)
        size_t tilde_pos = encoded_key.find('~');
        if (tilde_pos == std::string::npos) {
            throw std::runtime_error("Partials key must be in format 'output~input' (with tilde delimiter), got: " + encoded_key);
        }

        std::string output_name = encoded_key.substr(0, tilde_pos);
        std::string input_name = encoded_key.substr(tilde_pos + 1);

        std::cerr << "[DEBUG] JuliaDictToPartials:   Parsed as output=" << output_name
                  << ", input=" << input_name << std::endl;
        std::cerr.flush();

        // Get array value
        jl_value_t* value = jl_call2(getindex_fn, dict, key);
        CheckJuliaException();

        if (!jl_is_array(value)) {
            throw std::runtime_error("Partials value must be an array");
        }

        jl_array_t* jl_array = reinterpret_cast<jl_array_t*>(value);

        // Convert array to Variable
        size_t ndims = jl_array_ndims(jl_array);
        std::vector<size_t> shape(ndims);
        for (size_t d = 0; d < ndims; ++d) {
            shape[d] = jl_array_dim(jl_array, d);
        }

        std::cerr << "[DEBUG] JuliaDictToPartials: Array has " << ndims << " dimensions, shape = [";
        for (size_t d = 0; d < ndims; ++d) {
            if (d > 0) std::cerr << ", ";
            std::cerr << shape[d];
        }
        std::cerr << "]" << std::endl;
        std::cerr.flush();

        double* jl_data = reinterpret_cast<double*>(jl_array_data(jl_array, double));
        size_t total_size = jl_array_len(jl_array);

        std::cerr << "[DEBUG] JuliaDictToPartials: total_size = " << total_size << std::endl;
        std::cerr.flush();

        philote::Variable var(philote::kOutput, shape);
        std::cerr << "[DEBUG] JuliaDictToPartials: Created Variable with Size() = " << var.Size() << std::endl;
        std::cerr.flush();

        if (ndims == 1) {
            for (size_t k = 0; k < total_size; ++k) {
                var(k) = jl_data[k];
            }
        } else if (ndims == 2) {
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t r = 0; r < rows; ++r) {
                for (size_t c = 0; c < cols; ++c) {
                    var(r * cols + c) = jl_data[c * rows + r];
                }
            }
        } else {
            for (size_t k = 0; k < total_size; ++k) {
                var(k) = jl_data[k];
            }
        }

        partials[{output_name, input_name}] = var;
    }

    std::cerr << "[DEBUG] JuliaDictToPartials: Complete!" << std::endl;
    std::cerr.flush();
    return partials;
}

jl_value_t* ProtobufStructToJuliaDict(const google::protobuf::Struct& s) {
    // Get Dict constructor
    jl_function_t* dict_fn = jl_get_function(jl_base_module, "Dict");
    if (!dict_fn) {
        throw std::runtime_error("Could not find Base.Dict");
    }

    jl_value_t* dict = jl_call0(dict_fn);
    CheckJuliaException();

    jl_function_t* setindex_fn =
        jl_get_function(jl_base_module, "setindex!");

    // Iterate through struct fields
    for (const auto& [key, value] : s.fields()) {
        jl_value_t* jl_key = jl_cstr_to_string(key.c_str());

        jl_value_t* jl_value = nullptr;

        // Convert based on value type
        switch (value.kind_case()) {
            case google::protobuf::Value::kNumberValue:
                jl_value = jl_box_float64(value.number_value());
                break;
            case google::protobuf::Value::kBoolValue:
                jl_value = jl_box_bool(value.bool_value());
                break;
            case google::protobuf::Value::kStringValue:
                jl_value = jl_cstr_to_string(value.string_value().c_str());
                break;
            default:
                // Skip unsupported types (null, list, struct)
                continue;
        }

        if (jl_value) {
            jl_call3(setindex_fn, dict, jl_value, jl_key);
            CheckJuliaException();
        }
    }

    return dict;
}

}  // namespace julia
}  // namespace philote
