#ifndef _GRAMTROPY_STRINGS_H_
#define _GRAMTROPY_STRINGS_H_

#include <string>
#include <vector>
#include <map>
#include <string.h>

class Strings {
    size_t len;
    size_t count;
    std::vector<char> buf;

public:
    Strings(std::vector<char>&& data, size_t len_) : len(len_), count(data.size() / len_), buf(std::move(data)) {}

    size_t size() const {
        return count;
    }

    bool empty() const {
        return count == 0;
    }

    std::vector<char>::const_iterator StringBegin(size_t num) const {
        return buf.begin() + num * len;
    }

    std::vector<char>::const_iterator StringEnd(size_t num) const {
        if ((num + 1) * len == buf.size()) {
            return buf.end();
        }
        return buf.begin() + (num + 1) * len;
    }

    std::string operator[](size_t num) const {
        return std::string(StringBegin(num), StringEnd(num));
    }

    int find(const char* str, size_t len_) const {
        if (len != len_) {
            return -1;
        }
        int first = 0;
        int after = count;
        do {
            int mid = (first + after) >> 1;
            int r = memcmp(str, &*StringBegin(mid), len_);
            if (r == 0) {
                return mid;
            } else if (r < 0) {
                after = mid;
            } else {
                first = mid + 1;
            }
        } while (after > first);
        return -1;
    }
};

#endif
