#ifndef UTIL_STATISTICS_H
#define UTIL_STATISTICS_H

#include <cmath>
#include <vector>
#include <cassert>
#include <algorithm>
#include <stdexcept>

#include <string.h>

namespace util {

namespace statistics {

template <typename T>
class Percentile {

private:
    bool less_better;
    bool sorted;
    std::vector<T> elements;

public:
    Percentile(bool _less_better) :
            less_better(_less_better), sorted(false) {}

    void add(const T& x) {
        elements.emplace_back(x);
        sorted = false;
    }

    void add(const T* array, size_t count) {
        size_t n = elements.size();
        elements.resize(n + count);
        memcpy(elements.data() + n, array, count * sizeof(T));
        sorted = false;
    }

    T best() {
        if (elements.empty()) {
            throw std::runtime_error("no data to profile!");
        }
        prepareProfile();
        return elements.front();
    }

    T worst() {
        if (elements.empty()) {
            throw std::runtime_error("no data to profile!");
        }
        prepareProfile();
        return elements.back();
    }

    double average() {
        size_t count = elements.size();
        double sum = 0;
        for (size_t i = 0; i < count; i++) {
            sum += (double)elements[i];
        }
        return sum / count;
    }

    T operator ()(double percentage) {
        if (percentage < 0.0 || percentage > 100.0) {
            throw std::runtime_error("<percentage> should be within "
                    "[0.0, 100.0]!");
        }
        size_t count = elements.size();
        if (count == 0) {
            throw std::runtime_error("no data to profile!");
        }
        prepareProfile();
        size_t n = std::min(count, std::max<size_t>(1,
                (size_t)std::ceil(count * percentage / 100.0)));
        assert(0 < n && n <= count);
        return elements[n - 1];
    }

private:
    void prepareProfile() {
        if (!sorted) {
            if (less_better) {
                std::sort(elements.begin(), elements.end(), std::less<T>());
            }
            else {
                std::sort(elements.begin(), elements.end(), std::greater<T>());
            }
            sorted = true;
        }
    }
};

}

}

#endif