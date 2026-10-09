#pragma once

#include <mutex>
#include <queue>
#include <utility>
template <typename T>
class ThreadSafeQueue {
public:
    void push(const T& value) {
        std::lock_guard lock(mutex_);
        queue_.push(value);
    }

    void push(T&& value) {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(value));
    }

    template <typename... A>
    void push(A&&... value) {
        std::lock_guard lock(mutex_);
        queue_.push(std::forward<A>(value)...);
    }

    std::queue<T> drain() {
        std::queue<T> res;
        {
            std::lock_guard lock(mutex_);
            std::swap(res, queue_);
        }
        return res;
    }

    bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
};
