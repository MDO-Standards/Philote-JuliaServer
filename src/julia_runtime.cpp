// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_runtime.h"

#include <iostream>
#include <stdexcept>

namespace philote {
namespace julia {

std::once_flag JuliaRuntime::init_flag_;

JuliaRuntime::JuliaRuntime() {
    std::call_once(init_flag_, []() {
        jl_init();
    });
    initialized_.store(true);
}

JuliaRuntime::~JuliaRuntime() {
    if (initialized_.load()) {
        jl_atexit_hook(0);
        initialized_.store(false);
    }
}

JuliaRuntime& JuliaRuntime::GetInstance() {
    static JuliaRuntime instance;
    return instance;
}

jl_module_t* JuliaRuntime::LoadJuliaFile(const std::string& filepath) {
    if (!initialized_.load()) {
        throw std::runtime_error("Julia runtime not initialized");
    }

    // Use include() to load the file
    jl_function_t* include_fn = jl_get_function(jl_base_module, "include");
    if (!include_fn) {
        throw std::runtime_error("Could not find Base.include function");
    }

    jl_value_t* filepath_jl = jl_cstr_to_string(filepath.c_str());
    JL_GC_PUSH1(&filepath_jl);

    jl_value_t* result = jl_call1(include_fn, filepath_jl);

    JL_GC_POP();

    // Check for exceptions
    if (jl_exception_occurred()) {
        jl_value_t* ex = jl_exception_occurred();

        // Print full error to stderr using Julia's showerror
        jl_function_t* showerror_fn = jl_get_function(jl_base_module, "showerror");
        if (showerror_fn) {
            std::cerr << "\n[Julia Error] Loading file " << filepath << ":\n";
            std::cerr.flush();
            jl_call2(showerror_fn, jl_stderr_obj(), ex);
            std::cerr << "\n";
            std::cerr.flush();
        }

        // Also get full error message using sprint(showerror)
        jl_function_t* sprint_fn = jl_get_function(jl_base_module, "sprint");
        std::string detailed_msg;
        if (sprint_fn && showerror_fn) {
            // Clear exception before calling sprint
            jl_exception_clear();
            jl_value_t* msg_str = jl_call2(sprint_fn, reinterpret_cast<jl_value_t*>(showerror_fn), ex);
            if (!jl_exception_occurred() && msg_str && jl_is_string(msg_str)) {
                detailed_msg = jl_string_ptr(msg_str);
            }
        }

        if (detailed_msg.empty()) {
            detailed_msg = std::string("Julia error loading file: ") + jl_typeof_str(ex);
        }

        throw std::runtime_error(detailed_msg);
    }

    // Return Main module (where the file was included)
    return jl_main_module;
}

jl_value_t* JuliaRuntime::EvalString(const std::string& code) {
    if (!initialized_.load()) {
        throw std::runtime_error("Julia runtime not initialized");
    }

    jl_value_t* result = jl_eval_string(code.c_str());

    // Check for exceptions
    if (jl_exception_occurred()) {
        jl_value_t* ex = jl_exception_occurred();
        std::string error_msg = "Julia error: ";
        error_msg += jl_typeof_str(ex);
        throw std::runtime_error(error_msg);
    }

    return result;
}

}  // namespace julia
}  // namespace philote
