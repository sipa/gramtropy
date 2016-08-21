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
    Strings() : len(0), count(0) {}

    void Append(const std::string& str) {
        assert(empty() || str.size() == len);
        len = str.size();
        ++count;
        buf.insert(buf.end(), str.begin(), str.end());
    }

    void Append(const std::vector<char>& vec) {
        assert(buf.empty() || vec.size() == len);
        len = vec.size();
        ++count;
        buf.insert(buf.end(), vec.begin(), vec.end());
    }

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

    Strings& operator+=(const Strings& str) {
         if (empty()) {
             *this = str;
         } else if (!str.empty()) {
             assert(len == str.len);
             buf.insert(buf.end(), str.buf.begin(), str.buf.end());
             count += str.count;
         }
         return *this;
    }

    friend Strings operator*(const Strings& s1, const Strings& s2) {
        Strings res;
        if (s1.empty()) {
            return res;
        }
        if (s2.empty()) {
            return res;
        }
        for (size_t i = 0; i < s1.size(); i++) {
            for (size_t j = 0; j < s2.size(); j++) {
                res.Append(s1[i] + s2[j]);
            }
        }
        return res;
    }

    Strings& operator*=(const Strings& str) {
        *this = (*this * str);
        return *this;
    }

    int find(const char* str, size_t len_) {
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

    void Resort() {
        std::set<std::string> srt;
        for (size_t i = 0; i < size(); i++) {
            srt.insert((*this)[i]);
        }
        buf.clear();
        count = 0;
        for (const auto& str : srt) {
            Append(str);
        }
    }
};

#endif
