#pragma once
#include <cstddef>
#include <vector>

// Fixed-capacity object pool. All slots are allocated once in the constructor;
// spawn/kill never allocate. Live items are always packed in [0, size()).
template <typename T>
class Pool {
public:
    explicit Pool(std::size_t capacity) : items_(capacity) {}

    // Returns nullptr when full, so callers decide what to do (usually: skip).
    T* spawn() {
        if (count_ == items_.size()) return nullptr;
        items_[count_] = T{};
        return &items_[count_++];
    }

    // O(1) removal by swap-and-pop. Order is not preserved.
    // Iterate BACKWARDS when killing in a loop.
    void kill(std::size_t i) { items_[i] = items_[--count_]; }

    void clear() { count_ = 0; }
    std::size_t size() const { return count_; }
    std::size_t capacity() const { return items_.size(); }
    T& operator[](std::size_t i) { return items_[i]; }
    const T& operator[](std::size_t i) const { return items_[i]; }

private:
    std::vector<T> items_;
    std::size_t count_ = 0;
};
