// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_EXECUTOR_H
#define PHILOTE_JULIA_SERVER_JULIA_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace philote {
namespace julia {

/**
 * @brief Single-threaded executor for Julia calls
 *
 * ALL Julia operations must be executed on a single dedicated thread.
 * gRPC worker threads submit tasks to this executor and wait for completion.
 * This ensures no concurrent Julia calls regardless of gRPC threading.
 */
class JuliaExecutor {
public:
    static JuliaExecutor& GetInstance();

    /**
     * @brief Start the Julia executor thread
     * Must be called after Julia runtime is initialized
     */
    void Start();

    /**
     * @brief Stop the Julia executor thread
     */
    void Stop();

    /**
     * @brief Submit a task to execute on the Julia thread
     * Blocks until the task completes
     * @param task Function to execute on Julia thread
     */
    template<typename Func>
    auto Submit(Func&& task) -> decltype(task()) {
        using ReturnType = decltype(task());

        std::promise<ReturnType> promise;
        auto future = promise.get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push([&promise, task = std::forward<Func>(task)]() {
                try {
                    if constexpr (std::is_void_v<ReturnType>) {
                        task();
                        promise.set_value();
                    } else {
                        promise.set_value(task());
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            });
        }
        cv_.notify_one();

        return future.get();
    }

private:
    JuliaExecutor() = default;
    ~JuliaExecutor();

    void ExecutorLoop();

    std::thread executor_thread_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_EXECUTOR_H
