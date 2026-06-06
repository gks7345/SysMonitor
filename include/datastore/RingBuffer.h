#pragma once
#include <vector>
#include <stdexcept>

template <typename Q>
class RingBuffer {
private:
    size_t sz;
    size_t cap;
    size_t frontIdx;
    size_t backIdx;
    std::vector<Q> data;

public:
    RingBuffer(size_t capacity)
        : sz(0), cap(capacity)
        , frontIdx(0), backIdx(0)
        , data(capacity) {
    }

    void enQueue(const Q* q, size_t block) {
        for (size_t i = 0; i < block; i++) {
            enQueue(q[i]);
        }
    }

    void enQueue(const Q& q) {
        data[backIdx] = q;
        backIdx = (backIdx + 1) % cap;

        if (sz >= cap) {
            //가득 찼으면 front도 같이 이동
            frontIdx = (frontIdx + 1) % cap;
            // sz는 그대로 두기
            return;
        }

        sz++;
    }

    std::vector<Q> deQueue() {
        std::vector<Q> result;
        result.reserve(sz);

        size_t idx = frontIdx;
        for (size_t i = 0; i < sz; i++) {
            result.push_back(data[idx]);
            idx = (idx + 1) % cap;
        }

        sz = 0;
        frontIdx = 0;
        backIdx = 0;
        return result;
    }

    std::vector<Q> latest(size_t n) const {
        n = (std::min)(n, sz);
        std::vector<Q> result;
        result.reserve(n);

        size_t idx = (backIdx + cap - n) % cap;
        for (size_t i = 0; i < n; i++) {
            result.push_back(data[idx]);
            idx = (idx + 1) % cap;
        }
        return result;
    }

    std::vector<Q> popQueue(size_t n) {
        n = (std::min)(n, sz);
        std::vector<Q> result;
        result.reserve(n);

        for (size_t i = 0; i < n; i++) {
            result.push_back(data[frontIdx]);
            frontIdx = (frontIdx + 1) % cap;
        }
        sz -= n;
        return result;
    }

    size_t getSize()     const { return sz; }
    size_t getCapacity() const { return cap; }
    bool   isEmpty()     const { return sz == 0; }
    bool   isFull()      const { return sz == cap; }
};