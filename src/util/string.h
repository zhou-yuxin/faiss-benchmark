#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <cassert>

#include <string.h>

namespace util {

namespace string {

template <typename T>
int split(const char* str, const char* seperator, T* handler) {
    size_t seperator_len = strlen(seperator);
    while (true) {
        const char* end = strstr(str, seperator);
        size_t len = end ? end - str : strlen(str);
        int ret = (*handler)(str, len);
        if (ret) {
            return ret;
        }
        if (!end) {
            return 0;
        }
        str = end + seperator_len;
    }
}

}

}

#endif