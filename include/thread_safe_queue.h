#ifndef NETPULSE_THREAD_SAFE_QUEUE_H
#define NETPULSE_THREAD_SAFE_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
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

    T tryPop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item_ = std::move(queue_.front());
        queue_.pop();
        return item_;
    }
};
#endif //NETPULSE_THREAD_SAFE_QUEUE_H
