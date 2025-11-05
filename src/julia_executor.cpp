// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#include "julia_executor.h"

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
    // This thread handles ALL Julia calls - no concurrency!
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });

            if (stop_ && task_queue_.empty()) {
                break;
            }

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }

        if (task) {
            task();  // Execute Julia call on this dedicated thread
        }
    }
}

}  // namespace julia
}  // namespace philote
