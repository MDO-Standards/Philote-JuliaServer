// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_thread.h"

namespace philote {
namespace julia {

thread_local bool JuliaThreadGuard::adopted_ = false;

JuliaThreadGuard::JuliaThreadGuard() {
    if (!adopted_) {
        AdoptCurrentThread();
    }
}

bool JuliaThreadGuard::IsAdopted() {
    return adopted_;
}

void JuliaThreadGuard::AdoptCurrentThread() {
    jl_adopt_thread();
    adopted_ = true;
}

}  // namespace julia
}  // namespace philote
