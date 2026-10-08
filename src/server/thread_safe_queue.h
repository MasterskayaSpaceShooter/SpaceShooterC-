#pragma once

#include <mutex>
#include <queue>
#include <utility>
#include <vector>
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

    std::vector<T> drain() {
        std::vector<T> result;
        {
            std::lock_guard lock(mutex_);
            result.reserve(queue_.size());
            while (!queue_.empty()) {
                result.push_back(std::move(queue_.front()));
                queue_.pop();
            }
        }
        return result;
    }

private:
    std::mutex mutex_;
    std::queue<T> queue_;
};
