// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_GC_H
#define PHILOTE_JULIA_SERVER_JULIA_GC_H

#include <julia.h>

#include <initializer_list>
#include <vector>

namespace philote {
namespace julia {

/**
 * @brief RAII wrapper for Julia garbage collection protection
 *
 * This class uses Julia's GC stack to protect Julia objects from being
 * collected while they are in use by C++ code. Objects are automatically
 * unrooted when the GCProtect object goes out of scope.
 *
 * Usage:
 * @code
 * jl_value_t* obj = jl_eval_string("some_value");
 * GCProtect protect(obj);  // Object protected from GC
 * // Use obj safely...
 * // obj automatically unrooted when protect goes out of scope
 * @endcode
 *
 * @note Thread Safety: GC protection is per-thread (uses thread-local GC stack)
 */
class GCProtect {
public:
    /**
     * @brief Protect a single Julia object
     * @param obj Julia object to protect
     */
    explicit GCProtect(jl_value_t* obj);

    /**
     * @brief Protect multiple Julia objects
     * @param objs Initializer list of Julia objects to protect
     */
    GCProtect(std::initializer_list<jl_value_t*> objs);

    /**
     * @brief Destructor - unroots all protected objects
     */
    ~GCProtect();

    // Prevent copying and moving (GC stack is positional)
    GCProtect(const GCProtect&) = delete;
    GCProtect& operator=(const GCProtect&) = delete;
    GCProtect(GCProtect&&) = delete;
    GCProtect& operator=(GCProtect&&) = delete;

private:
    std::vector<jl_value_t*> protected_objects_;
    size_t count_;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_GC_H
