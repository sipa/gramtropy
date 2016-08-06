#ifndef _GRAMTROPY_STRINGVECTOR_H_
#define _GRAMTROPY_STRINGVECTOR_H_

class StringVector {
    std::vector<char> buf;
    std::vector<uint32_t> index;

public:
    void Append(const std::string& str) {
        index.push_back(buf.size());
        buf.insert(buf.end(), str.begin(), str.end());
    }

    void Append(const std::vector<char>& vec) {
        index.push_back(buf.size());
        buf.insert(buf.end(), vec.begin(), vec.end());
    }

    size_t size() const {
        return index.size();
    }

    std::vector<char>::const_iterator StringBegin(size_t num) const {
        return buf.begin() + index[num];
    }

    std::vector<char>::const_iterator StringEnd(size_t num) const {
        if (num + 1 == index.size()) {
            return buf.end();
        }
        return buf.begin() + index[num + 1];
    }
};

#endif
