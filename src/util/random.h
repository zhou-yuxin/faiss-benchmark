#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

#include <random>
#include <stdexcept>

#include <time.h>

namespace util {

namespace random {

template <typename T>
class Sequence {

private:
    T base;
    T range;
    T count;
    T current;
    std::default_random_engine engine;

public:
    Sequence(T start, T end, T _count) {
        base = start;
        range = end - start;
        count = _count;
        current = 0;
        engine.seed(time(nullptr));
    }

    T next() {
        if (current == count) {
            throw std::runtime_error("no more random element available!");
        }
        T a = range * current / count;
        T b = range * (++current) / count - 1;
        T offset = std::uniform_int_distribution<T>(a, b)(engine);
        return base + offset;
    }

};

}

}

#endif