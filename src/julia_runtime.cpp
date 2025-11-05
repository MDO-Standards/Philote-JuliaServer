// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_runtime.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace philote {
namespace julia {

std::once_flag JuliaRuntime::init_flag_;

JuliaRuntime::JuliaRuntime() {
    std::call_once(init_flag_, []() {
        jl_init();

        // Prevent BLAS from spawning extra threads
        // This avoids thread explosion when Julia does linear algebra
        jl_eval_string("using LinearAlgebra; BLAS.set_num_threads(1)");
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

    // Convert to absolute path
    std::filesystem::path abs_path = std::filesystem::absolute(filepath);
    std::string abs_path_str = abs_path.string();

    std::cout << "[DEBUG] Loading Julia file: " << filepath << std::endl;
    std::cout << "[DEBUG] Absolute path: " << abs_path_str << std::endl;

    // Use jl_eval_string with include() - this gives better error messages
    std::string include_cmd = "include(\"" + abs_path_str + "\")";
    std::cout << "[DEBUG] Eval string: " << include_cmd << std::endl;

    jl_value_t* result = jl_eval_string(include_cmd.c_str());

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
