#pragma once
/*
  backend/ring.hpp
  Lock-free SPSC ring buffer for float stereo audio.
*/

#include "audio_common.hpp"

struct Ring {
    std::vector<float>      buf;
    std::atomic<size_t>     head{0}, tail{0};
    std::mutex              mx;
    std::condition_variable cv_data, cv_space;

    explicit Ring(size_t f) : buf(f * OUT_CH, 0.f) {}

    size_t cap()   const { return buf.size() / OUT_CH; }
    size_t avail() const {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        return (t - h + cap()) % cap();
    }
    size_t space() const { return cap() - avail() - 1; }

    size_t push(const float* s, size_t n) {
        size_t c = cap(), t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);
        size_t w = std::min(n, (h - t - 1 + c) % c);
        for (size_t i = 0; i < w; ++i) {
            size_t idx = ((t + i) % c) * OUT_CH;
            buf[idx] = s[i * OUT_CH]; buf[idx + 1] = s[i * OUT_CH + 1];
        }
        tail.store((t + w) % c, std::memory_order_release);
        if (w) cv_data.notify_one();
        return w;
    }

    size_t pop(float* d, size_t n) {
        size_t c = cap(), h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);
        size_t r = std::min(n, (t - h + c) % c);
        for (size_t i = 0; i < r; ++i) {
            size_t idx = ((h + i) % c) * OUT_CH;
            d[i * OUT_CH] = buf[idx]; d[i * OUT_CH + 1] = buf[idx + 1];
        }
        head.store((h + r) % c, std::memory_order_release);
        if (r) cv_space.notify_one();
        return r;
    }

    void clear() {
        head.store(0, std::memory_order_release);
        tail.store(0, std::memory_order_release);
    }
};
