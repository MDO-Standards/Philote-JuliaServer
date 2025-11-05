// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_executor.h"

#include <julia.h>
#include <iostream>

namespace philote {
namespace julia {

JuliaExecutor& JuliaExecutor::GetInstance() {
    static JuliaExecutor instance;
    return instance;
}

void JuliaExecutor::Start() {
    executor_thread_ = std::thread(&JuliaExecutor::ExecutorLoop, this);
}

void JuliaExecutor::Stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    cv_.notify_one();

    if (executor_thread_.joinable()) {
        executor_thread_.join();
    }
}

JuliaExecutor::~JuliaExecutor() {
    Stop();
}

void JuliaExecutor::ExecutorLoop() {
    std::cout << "[EXECUTOR] Thread starting..." << std::endl;

    // CRITICAL: Adopt this thread for Julia
    // Julia runtime was initialized on main thread, but this thread needs adoption
    jl_adopt_thread();
    std::cout << "[EXECUTOR] Thread adopted by Julia" << std::endl;
    std::cout << "[EXECUTOR] Thread ID: " << std::this_thread::get_id() << std::endl;

    // This thread handles ALL Julia calls - no concurrency!
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            std::cout << "[EXECUTOR] Waiting for task..." << std::endl;
            cv_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });

            if (stop_ && task_queue_.empty()) {
                std::cout << "[EXECUTOR] Stopping..." << std::endl;
                break;
            }

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
                std::cout << "[EXECUTOR] Got task from queue" << std::endl;
            }
        }

        if (task) {
            std::cout << "[EXECUTOR] Executing task..." << std::endl;
            try {
                task();  // Execute Julia call on this dedicated thread
                std::cout << "[EXECUTOR] Task completed successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EXECUTOR ERROR] Task failed: " << e.what() << std::endl;
                throw;
            }
        }
    }
    std::cout << "[EXECUTOR] Thread exiting" << std::endl;
}

}  // namespace julia
}  // namespace philote
