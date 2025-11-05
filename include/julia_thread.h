// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_THREAD_H
#define PHILOTE_JULIA_SERVER_JULIA_THREAD_H

#include <julia.h>

namespace philote {
namespace julia {

/**
 * @brief RAII guard that adopts the current thread for Julia execution
 *
 * This class ensures that C++ threads (such as gRPC worker threads) are
 * adopted by Julia before calling any Julia API functions. Thread adoption
 * is required as of Julia 1.9+ to safely call Julia from non-Julia threads.
 *
 * Usage:
 * @code
 * void Compute(...) {
 *     JuliaThreadGuard guard;  // Adopts thread if not already adopted
 *     // Now safe to call Julia functions
 *     jl_call(my_function, args);
 * }
 * @endcode
 *
 * @note Thread Safety: Uses thread_local storage to track adoption per-thread.
 *       Calling jl_adopt_thread() multiple times on the same thread is safe
 *       (it's idempotent), but we optimize by tracking adoption state.
 */
class JuliaThreadGuard {
public:
    /**
     * @brief Constructor - adopts current thread if not already adopted
     */
    JuliaThreadGuard();

    /**
     * @brief Destructor - no-op (adoption persists for thread lifetime)
     */
    ~JuliaThreadGuard() = default;

    /**
     * @brief Check if current thread has been adopted
     * @return true if current thread is adopted, false otherwise
     */
    static bool IsAdopted();

private:
    /**
     * @brief Adopt the current thread for Julia execution
     *
     * Calls jl_adopt_thread() and marks the thread as adopted.
     */
    static void AdoptCurrentThread();

    // Thread-local flag tracking if this thread has been adopted
    static thread_local bool adopted_;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_THREAD_H
