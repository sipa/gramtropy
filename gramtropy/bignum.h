#ifndef _GRAMTROPY_BIGNUM_H_
#define _GRAMTROPY_BIGNUM_H_

#include <vector>
#include <stdint.h>

class BigNum {
    std::vector<uint32_t> pn;

    void shrink() {
        while (!pn.empty() && pn.back() == 0) pn.pop_back();
    }

    void shift_right_one() {
        std::vector<uint32_t> r;
        r.resize(pn.size());
        for (unsigned int i = 0; i < pn.size(); i++) {
            if (i >= 1) {
                r[i - 1] |= (pn[i] << 31);
            }
            r[i] |= (pn[i] >> 1);
        }
        pn = std::move(r);
        shrink();
    }

    void shift_left(int shift) {
        std::vector<uint32_t> r;
        r.resize((bits() + shift + 31) / 32);
        int k = shift / 32;
        shift %= 32;
        for (unsigned int i = 0; i < pn.size(); i++) {
            if (shift != 0) {
                r[i + k + 1] |= (pn[i] >> (32 - shift));
            }
            r[i + k] |= (pn[i] << shift);
        }
        pn = std::move(r);
        shrink();
    }

    int compare(const BigNum& b) const {
        unsigned int s = pn.size();
        if (s < b.pn.size()) return -1;
        if (s > b.pn.size()) return 1;
        for (unsigned int i = 0; i < s; i++) {
            if (pn[s - 1 - i] < b.pn[s - 1 - i]) return -1;
            if (pn[s - 1 - i] > b.pn[s - 1 - i]) return 1;
        }
        return 0;
    }

public:
    BigNum() {}

    BigNum(uint32_t n) {
        if (n > 0) {
            pn.resize(1);
            pn[0] = n;
        }
    }

    BigNum(uint8_t* data, size_t len) {
        pn.assign((len + 3) / 4, 0);
        for (unsigned i = 0; i < len; i++) {
            pn[i / 4] |= ((uint32_t)data[len - 1 - i]) << (8 * i);
        }
        shrink();
    }

    BigNum& operator+=(const BigNum& b) {
        pn.resize(std::max(pn.size(), b.pn.size()) + 1);
        uint64_t carry = 0;
        for (unsigned int i = 0; i < b.pn.size(); i++) {
            carry += (uint64_t)pn[i] + b.pn[i];
            pn[i] = carry;
            carry >>= 32;
        }
        for (unsigned int i = b.pn.size(); i < pn.size(); i++) {
            carry += pn[i];
            pn[i] = carry;
            carry >>= 32;
        }
        shrink();
        return *this;
    }

    BigNum& operator-=(const BigNum& b) {
        uint64_t carry = 1;
        for (unsigned int i = 0; i < b.pn.size(); i++) {
            carry += (uint64_t)pn[i] + ~b.pn[i];
            pn[i] = carry;
            carry >>= 32;
        }
        for (unsigned int i = b.pn.size(); i < pn.size(); i++) {
            carry += (uint64_t)pn[i] + 0xFFFFFFFFUL;
            pn[i] = carry;
            carry >>= 32;
        }
        shrink();
        return *this;
    }

    friend BigNum operator*(const BigNum& a, const BigNum& b) {
        BigNum r;
        r.pn.resize(a.pn.size() + b.pn.size());
        for (unsigned int j = 0; j < a.pn.size(); j++) {
            uint64_t carry = 0;
            for (unsigned int i = 0; i < b.pn.size(); i++) {
                carry += r.pn[i + j] + (uint64_t)a.pn[j] * b.pn[i];
                r.pn[i + j] = carry;
                carry >>= 32;
            }
            r.pn[b.pn.size() + j] += carry;
        }
        r.shrink();
        return r;
    }

    int bits() const {
        if (pn.size() == 0) return 0;
        int ret = pn.size() * 32;
        uint32_t x = pn.back();
        while (x <= 0x7FFFFFFF) {
            x <<= 1;
            ret--;
        }
        return ret;
    }

    friend bool operator<(const BigNum& a, const BigNum& b) { return a.compare(b) < 0; }
    friend bool operator<=(const BigNum& a, const BigNum& b) { return a.compare(b) <= 0; }
    friend bool operator>(const BigNum& a, const BigNum& b) { return a.compare(b) > 0; }
    friend bool operator>=(const BigNum& a, const BigNum& b) { return a.compare(b) >= 0; }
    friend bool operator==(const BigNum& a, const BigNum& b) { return a.compare(b) == 0; }
    friend bool operator!=(const BigNum& a, const BigNum& b) { return a.compare(b) != 0; }

    /* Return *this / denom and replace *this with *this % denom. */
    BigNum divmod(const BigNum& denom) {
        int numbits = bits();
        int divbits = denom.bits();
        if (divbits > numbits) {
            return BigNum();
        }
        BigNum r;
        r.pn.resize((numbits - divbits)/32 + 1);
        int shift = numbits - divbits;
        BigNum div = denom;
        div.shift_left(shift);
        while (shift >= 0) {
            if (*this >= div) {
                *this -= div;
                r.pn[shift / 32] |= (1 << (shift & 31));
            }
            div.shift_right_one();
            shift--;
        }
        r.shrink();
        return r;
    }

    std::string hex() const {
        static const char cv[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
        unsigned int digits = (bits() + 3) / 4;
        std::string ret;
        for (unsigned int i = 0; i < digits; i++) {
            ret = cv[(pn[i / 8] >> (4 * (i % 8))) % 16] + ret;
        }
        return ret;
    }

    double get_d() const {
        double ret = 0;
        for (unsigned int i = 0; i < pn.size(); i++) {
            ret = ret * 4294967296.0 + pn[pn.size() - i - 1];
        }
        return ret;
    }

    uint32_t get_ui() const {
        if (pn.empty()) return 0;
        return pn[0];
    }

    bool is_zero() const {
        return pn.empty();
    }

    void operator++(int) {
        for (unsigned int i = 0; i < pn.size(); i++) {
            if (++pn[i]) {
                return;
            }
        }
        pn.push_back(1);
    }
};

#endif
