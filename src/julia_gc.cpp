// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_gc.h"

namespace philote {
namespace julia {

GCProtect::GCProtect(jl_value_t* obj) : count_(1) {
    protected_objects_.push_back(obj);
    JL_GC_PUSH1(&protected_objects_[0]);
}

GCProtect::GCProtect(std::initializer_list<jl_value_t*> objs)
    : protected_objects_(objs), count_(objs.size()) {
    // Use appropriate JL_GC_PUSH macro based on count
    switch (count_) {
        case 0:
            break;
        case 1: {
            JL_GC_PUSH1(&protected_objects_[0]);
            break;
        }
        case 2: {
            JL_GC_PUSH2(&protected_objects_[0], &protected_objects_[1]);
            break;
        }
        case 3: {
            JL_GC_PUSH3(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2]);
            break;
        }
        case 4: {
            JL_GC_PUSH4(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2], &protected_objects_[3]);
            break;
        }
        case 5: {
            JL_GC_PUSH5(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2], &protected_objects_[3],
                        &protected_objects_[4]);
            break;
        }
        case 6: {
            JL_GC_PUSH6(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2], &protected_objects_[3],
                        &protected_objects_[4], &protected_objects_[5]);
            break;
        }
        case 7: {
            JL_GC_PUSH7(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2], &protected_objects_[3],
                        &protected_objects_[4], &protected_objects_[5],
                        &protected_objects_[6]);
            break;
        }
        case 8: {
            JL_GC_PUSH8(&protected_objects_[0], &protected_objects_[1],
                        &protected_objects_[2], &protected_objects_[3],
                        &protected_objects_[4], &protected_objects_[5],
                        &protected_objects_[6], &protected_objects_[7]);
            break;
        }
        default: {
            // For more than 8 objects, use PUSHARGS
            jl_value_t** args = protected_objects_.data();
            JL_GC_PUSHARGS(args, count_);
            break;
        }
    }
}

GCProtect::~GCProtect() {
    if (count_ > 0) {
        JL_GC_POP();
    }
}

}  // namespace julia
}  // namespace philote
