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

    // Get exception type and message
    std::string msg = "Julia exception: ";
    msg += jl_typeof_str(ex);

    // Try to get exception message if it's an ErrorException
    jl_function_t* sprint_fn = jl_get_function(jl_base_module, "sprint");
    jl_function_t* showerror_fn =
        jl_get_function(jl_base_module, "showerror");

    if (sprint_fn && showerror_fn) {
        jl_value_t* msg_str =
            jl_call2(sprint_fn, showerror_fn, ex);
        if (msg_str && jl_is_string(msg_str)) {
            msg += ": ";
            msg += jl_string_ptr(msg_str);
        }
    }

    return msg;
}

jl_value_t* VariablesToJuliaDict(const philote::Variables& vars) {
    // Get Dict constructor
    jl_function_t* dict_fn = jl_get_function(jl_base_module, "Dict");
    if (!dict_fn) {
        throw std::runtime_error("Could not find Base.Dict function");
    }

    // Create empty Dict{String, Array{Float64}}
    jl_value_t* dict = jl_call0(dict_fn);
    CheckJuliaException();

    GCProtect protect_dict(dict);

    // Get setindex! function for adding to dict
    jl_function_t* setindex_fn =
        jl_get_function(jl_base_module, "setindex!");
    if (!setindex_fn) {
        throw std::runtime_error("Could not find Base.setindex! function");
    }

    // Convert each variable
    for (const auto& [name, var] : vars) {
        // Create Julia array from Variable data
        const auto& shape = var.GetShape();
        size_t total_size = var.Size();

        // Create Julia array type
        jl_value_t* array_type = jl_apply_array_type(
            reinterpret_cast<jl_value_t*>(jl_float64_type), shape.size());
        GCProtect protect_type(array_type);

        // Create dimensions tuple
        jl_value_t* dims = jl_alloc_svec(shape.size());
        GCProtect protect_dims(dims);

        for (size_t i = 0; i < shape.size(); ++i) {
            jl_svecset(dims, i, jl_box_int64(shape[i]));
        }

        // Create array
        jl_array_t* jl_array = jl_alloc_array_1d(array_type, total_size);
        GCProtect protect_array(reinterpret_cast<jl_value_t*>(jl_array));

        // Copy data (C++ row-major to Julia column-major)
        double* jl_data = reinterpret_cast<double*>(jl_array_data(jl_array));
        const double* cpp_data = var.Data();

        // For 1D arrays, direct copy
        if (shape.size() == 1) {
            std::copy(cpp_data, cpp_data + total_size, jl_data);
        } else if (shape.size() == 2) {
            // For 2D: transpose from row-major to column-major
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    jl_data[j * rows + i] = cpp_data[i * cols + j];
                }
            }
        } else {
            // For higher dimensions, use direct copy (assume compatible layout)
            // TODO: Implement proper multi-dimensional transpose if needed
            std::copy(cpp_data, cpp_data + total_size, jl_data);
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
        GCProtect protect_key(key);

        jl_call3(setindex_fn, dict, reinterpret_cast<jl_value_t*>(jl_array),
                 key);
        CheckJuliaException();
    }

    return dict;
}

philote::Variables JuliaDictToVariables(jl_value_t* dict) {
    if (!dict || !jl_is_dict(dict)) {
        throw std::runtime_error(
            "Expected Julia Dict, got " +
            std::string(dict ? jl_typeof_str(dict) : "null"));
    }

    philote::Variables vars;

    // Get dict keys and values
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");

    if (!keys_fn || !getindex_fn) {
        throw std::runtime_error("Could not find Base.keys or getindex");
    }

    jl_value_t* keys = jl_call1(keys_fn, dict);
    CheckJuliaException();
    GCProtect protect_keys(keys);

    // Convert keys to array
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");
    jl_array_t* keys_array =
        reinterpret_cast<jl_array_t*>(jl_call1(collect_fn, keys));
    CheckJuliaException();
    GCProtect protect_keys_array(reinterpret_cast<jl_value_t*>(keys_array));

    size_t num_keys = jl_array_len(keys_array);

    // Iterate through keys
    for (size_t i = 0; i < num_keys; ++i) {
        jl_value_t* key = jl_arrayref(keys_array, i);
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
        double* jl_data = reinterpret_cast<double*>(jl_array_data(jl_array));
        size_t total_size = jl_array_len(jl_array);

        // Create Variable
        philote::Variable var(shape);

        // Copy data (Julia column-major to C++ row-major)
        double* cpp_data = var.Data();

        if (ndims == 1) {
            // Direct copy for 1D
            std::copy(jl_data, jl_data + total_size, cpp_data);
        } else if (ndims == 2) {
            // Transpose for 2D
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    cpp_data[i * cols + j] = jl_data[j * rows + i];
                }
            }
        } else {
            // Direct copy for higher dimensions
            std::copy(jl_data, jl_data + total_size, cpp_data);
        }

        vars[name] = var;
    }

    return vars;
}

philote::Partials JuliaDictToPartials(jl_value_t* dict) {
    if (!dict || !jl_is_dict(dict)) {
        throw std::runtime_error("Expected Julia Dict for partials");
    }

    philote::Partials partials;

    // Get keys
    jl_function_t* keys_fn = jl_get_function(jl_base_module, "keys");
    jl_function_t* getindex_fn = jl_get_function(jl_base_module, "getindex");
    jl_function_t* collect_fn = jl_get_function(jl_base_module, "collect");

    jl_value_t* keys = jl_call1(keys_fn, dict);
    CheckJuliaException();
    GCProtect protect_keys(keys);

    jl_array_t* keys_array =
        reinterpret_cast<jl_array_t*>(jl_call1(collect_fn, keys));
    CheckJuliaException();
    GCProtect protect_keys_array(reinterpret_cast<jl_value_t*>(keys_array));

    size_t num_keys = jl_array_len(keys_array);

    for (size_t i = 0; i < num_keys; ++i) {
        jl_value_t* key = jl_arrayref(keys_array, i);

        // Key should be a tuple (output, input)
        if (!jl_is_tuple(key) || jl_nfields(key) != 2) {
            throw std::runtime_error(
                "Partials dict key must be a 2-tuple (output, input)");
        }

        jl_value_t* output_name = jl_fieldref(key, 0);
        jl_value_t* input_name = jl_fieldref(key, 1);

        if (!jl_is_string(output_name) || !jl_is_string(input_name)) {
            throw std::runtime_error("Partials tuple elements must be strings");
        }

        std::string output = jl_string_ptr(output_name);
        std::string input = jl_string_ptr(input_name);

        // Get value (array)
        jl_value_t* value = jl_call2(getindex_fn, dict, key);
        CheckJuliaException();

        if (!jl_is_array(value)) {
            throw std::runtime_error("Partials value must be an array");
        }

        jl_array_t* jl_array = reinterpret_cast<jl_array_t*>(value);

        // Convert array to Variable (same as JuliaDictToVariables)
        size_t ndims = jl_array_ndims(jl_array);
        std::vector<size_t> shape(ndims);
        for (size_t d = 0; d < ndims; ++d) {
            shape[d] = jl_array_dim(jl_array, d);
        }

        double* jl_data = reinterpret_cast<double*>(jl_array_data(jl_array));
        size_t total_size = jl_array_len(jl_array);

        philote::Variable var(shape);
        double* cpp_data = var.Data();

        if (ndims == 1) {
            std::copy(jl_data, jl_data + total_size, cpp_data);
        } else if (ndims == 2) {
            size_t rows = shape[0];
            size_t cols = shape[1];
            for (size_t i = 0; i < rows; ++i) {
                for (size_t j = 0; j < cols; ++j) {
                    cpp_data[i * cols + j] = jl_data[j * rows + i];
                }
            }
        } else {
            std::copy(jl_data, jl_data + total_size, cpp_data);
        }

        partials[{output, input}] = var;
    }

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
    GCProtect protect_dict(dict);

    jl_function_t* setindex_fn =
        jl_get_function(jl_base_module, "setindex!");

    // Iterate through struct fields
    for (const auto& [key, value] : s.fields()) {
        jl_value_t* jl_key = jl_cstr_to_string(key.c_str());
        GCProtect protect_key(jl_key);

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
            GCProtect protect_value(jl_value);
            jl_call3(setindex_fn, dict, jl_value, jl_key);
            CheckJuliaException();
        }
    }

    return dict;
}

}  // namespace julia
}  // namespace philote
