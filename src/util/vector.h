#ifndef UTIL_VECTOR_H
#define UTIL_VECTOR_H

#include <cmath>
#include <vector>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

namespace util {

namespace vector {

template <typename TSrc, typename TDst>
struct Converter {

    std::vector<TDst> operator ()(const std::vector<TSrc>& src) {
        size_t count = src.size();
        std::vector<TDst> dst;
        dst.resize(count);
        for (size_t i = 0; i < count; i++) {
            dst[i] = static_cast<TDst>(src[i]);
        }
        return dst;
    }

    void operator ()(TDst* dst, const std::vector<TSrc>& src) {
        size_t count = src.size();
        for (size_t i = 0; i < count; i++) {
            dst[i] = static_cast<TDst>(src[i]);
        }
    }

};

template <typename T>
struct Converter<T, T> {

    std::vector<T>& operator ()(std::vector<T>& src) {
        return src;
    }

    const std::vector<T>& operator ()(const std::vector<T>& src) {
        return src;
    }

    void operator ()(T* dst, const std::vector<T>& src) {
        memcpy(dst, src.data(), src.size() * sizeof(T));
    }

};

template <typename TV1, typename TV2, typename TResult>
class DistanceAlgo {

public:
    virtual ~DistanceAlgo() {}

    virtual TResult operator ()(const std::vector<TV1>& v1,
            const std::vector<TV2>& v2) = 0;

};

template <typename TV1, typename TV2, typename TResult>
class DistanceL1 : public DistanceAlgo<TV1, TV2, TResult> {

public:
    TResult operator ()(const std::vector<TV1>& v1,
            const std::vector<TV2>& v2) override {
        size_t dim = v1.size();
        if (v2.size() != dim) {
            char buf[256];
            sprintf(buf, "argument <v1> has %lu dimensions, "
                    "while <v2> has %lu dimensions!", dim, v2.size());
            throw std::runtime_error(buf);
        }
        TResult sum = 0;
        for (size_t i = 0; i < dim; i++) {
            TResult delta = static_cast<TResult>(v1[i]) - 
                    static_cast<TResult>(v2[i]);
            sum += std::abs(delta);
        }
        return sum;
    }

};

template <typename TV1, typename TV2, typename TResult>
class DistanceL2Sqr : public DistanceAlgo<TV1, TV2, TResult> {

public:
    TResult operator ()(const std::vector<TV1>& v1,
            const std::vector<TV2>& v2) override {
        size_t dim = v1.size();
        if (v2.size() != dim) {
            char buf[256];
            sprintf(buf, "argument <v1> has %lu dimensions, "
                    "while <v2> has %lu dimensions!", dim, v2.size());
            throw std::runtime_error(buf);
        }
        TResult sum = 0;
        for (size_t i = 0; i < dim; i++) {
            TResult delta = static_cast<TResult>(v1[i]) - 
                    static_cast<TResult>(v2[i]);
            sum += delta * delta;
        }
        return sum;
    }

};

}

}

#endif