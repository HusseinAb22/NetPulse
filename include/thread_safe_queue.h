#ifndef NETPULSE_THREAD_SAFE_QUEUE_H
#define NETPULSE_THREAD_SAFE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;

    std::condition_variable_any cv_;
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // class method to push and pop items safely
    void push(T item) {
        std::lock_guard lock(mutex_);

        queue_.push(std::move(item));
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty(); });

        T item_ = std::move(queue_.front());
        queue_.pop();
        return item_;
    }
    
    // Blocks until an item is available OR `token` is triggered. Returns
    // std::nullopt only when stop was requested AND the queue is empty, so a
    // consumer loop still drains anything queued before exiting.
    std::optional<T> pop(std::stop_token token) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait(lock, token, [this]() { return !queue_.empty(); })) {
            return std::nullopt;  // stop requested, nothing left to hand out
        }
        T item_ = std::move(queue_.front());
        queue_.pop();
        return item_;
    }

    std::optional<T> tryPop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item_ = std::move(queue_.front());
        queue_.pop();
        return item_;
    }
};
#endif  // NETPULSE_THREAD_SAFE_QUEUE_H
