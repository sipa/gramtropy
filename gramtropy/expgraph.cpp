#include <deque>
#include <map>
#include "expgraph.h"

#include <string.h>

ExpGraph::Ref ExpGraph::NewDict(std::set<std::string>&& dict) {
    assert(dict.size() > 0);
    auto ret = nodes.emplace_back(Node::NodeType::DICT);
    ret->dict = std::move(dict);
    ret->count = ret->dict.size();
    ret->len = ret->dict.begin()->size();
    return std::move(ret);
}

ExpGraph::Ref ExpGraph::NewConcat(std::vector<Ref>&& refs) {
    assert(refs.size() > 0);
    if (refs.size() == 1) {
        return std::move(refs[0]);
    }
    auto ret = nodes.emplace_back(Node::NodeType::CONCAT);
    BigNum count = refs[0]->count;
    int len = refs[0]->len;
    for (size_t i = 1; i < refs.size(); i++) {
        count *= refs[i]->count;
        assert(refs[i]->len >= 0);
        len += refs[i]->len;
    }
    ret->count = std::move(count);
    ret->refs = std::move(refs);
    ret->len = len;
    return std::move(ret);
}

ExpGraph::Ref ExpGraph::NewDisjunct(std::vector<Ref>&& refs) {
    assert(refs.size() > 0);
    if (refs.size() == 1) {
        return std::move(refs[0]);
    }
    int len = refs[0]->len;
    auto ret = nodes.emplace_back(Node::NodeType::DISJUNCT);
    BigNum count = refs[0]->count;
    for (size_t i = 1; i < refs.size(); i++) {
        count += refs[i]->count;
        if (len != refs[i]->len) {
            len = -1;
        }
    }
    ret->count = std::move(count);
    ret->refs = std::move(refs);
    ret->len = len;
    return std::move(ret);
}

namespace {

std::set<std::string> ExpandDict(const ExpGraph::Ref& ref, size_t offset = 0) {
    std::set<std::string> res;
    switch (ref->nodetype) {
    case ExpGraph::Node::NodeType::DICT:
        return ref->dict;
    case ExpGraph::Node::NodeType::DISJUNCT:
        for (const ExpGraph::Ref& sub : ref->refs) {
            std::set<std::string> s = ExpandDict(sub);
            if (s.size() < res.size()) {
                res.swap(s);
            }
            res.insert(s.begin(), s.end());
        }
        assert(res.size() == ref->count.get_ui());
        break;
    case ExpGraph::Node::NodeType::CONCAT:
        if (offset + 1 == ref->refs.size()) {
            return ExpandDict(ref->refs[offset]);
        } else {
            std::set<std::string> s1 = ExpandDict(ref->refs[offset]);
            std::set<std::string> s2 = ExpandDict(ref, offset + 1);
            for (const auto& str1 : s1) {
                for (const auto& str2 : s2) {
                    res.emplace(str1 + str2);
                }
            }
        }
        if (offset == 0) {
            assert(res.size() == ref->count.get_ui());
        }
        break;
    }
    return std::move(res);
}

bool Collectable(ExpGraph::Node::NodeType nodetype, const std::vector<ExpGraph::Ref>& input) {
    for (const ExpGraph::Ref& sub : input) {
        if (sub->nodetype == nodetype && sub.unique()) {
            return true;
        }
    }
    return false;
}

void Collect(ExpGraph::Node::NodeType nodetype, std::vector<ExpGraph::Ref>& output, std::vector<ExpGraph::Ref>&& input) {
    for (ExpGraph::Ref& sub : input) {
        if (sub->nodetype == nodetype && sub.unique()) {
            Collect(nodetype, output, std::move(sub->refs));
        } else {
            output.emplace_back(std::move(sub));
        }
    }
    input.clear();
}

bool Optimize(const ExpGraph::Ref& ref) {
    switch (ref->nodetype) {
    case ExpGraph::Node::NodeType::DICT:
        break;
    case ExpGraph::Node::NodeType::DISJUNCT:
        if (ref->count.bits() <= 13) {
            auto x = ExpandDict(ref);
            ref->dict = std::move(x);
            ref->nodetype = ExpGraph::Node::NodeType::DICT;
            ref->refs.clear();
            return true;
        }
    case ExpGraph::Node::NodeType::CONCAT:
        if (Collectable(ref->nodetype, ref->refs)) {
            std::vector<ExpGraph::Ref> result;
            Collect(ref->nodetype, result, std::move(ref->refs));
            ref->refs = std::move(result);
            return true;
        }
        break;
    }
    return false;
}

}

void Optimize(ExpGraph& graph) {
    bool any;
    do {
        any = false;
        for (auto it = graph.nodes.begin(); it != graph.nodes.end(); it++) {
            bool now = Optimize(it);
            any |= now;
        }
    } while(any);
}

namespace {

size_t Generate(std::vector<char>& out, size_t pos, const ExpGraph::Ref& ref, BigNum&& num) {
//    fprintf(stderr, "gen pos=%i len=%i type=%i\n", (int)pos, (int)ref->len, (int)ref->nodetype);
    if (ref->len >= 0) {
        if (out.size() < pos + ref->len) {
            out.resize(pos + ref->len);
        }
    }
    switch (ref->nodetype) {
    case ExpGraph::Node::NodeType::DICT: {
        assert(num.bits() <= 32);
        uint32_t n = num.get_ui();
        auto it = ref->dict.begin();
        while (--n) ++it;
        memcpy(out.data() + pos, it->data(), it->size());
        return pos + ref->len;
    }
    case ExpGraph::Node::NodeType::DISJUNCT:
        for (const ExpGraph::Ref& sub : ref->refs) {
            if (num < sub->count) {
                return Generate(out, pos, sub, std::move(num));
            }
            num -= sub->count;
        }
        assert(false);
    case ExpGraph::Node::NodeType::CONCAT:
        for (const ExpGraph::Ref& sub : ref->refs) {
            BigNum div = num.divmod(sub->count);
            pos = Generate(out, pos, sub, std::move(num));
            num = std::move(div);
        }
        return pos;
    }
    assert(false);
}

BigNum RandomInteger(const BigNum& range) {
    BigNum out;
    int bits = range.bits();
    std::vector<uint8_t> data;
    data.resize((bits + 7)/8);
    FILE* rng = fopen("/dev/urandom", "rb");
    do {
        size_t r = fread(&data[0], data.size(), 1, rng);
        if (r != 1) {
            throw std::runtime_error("Unable to read from RNG");
        }
        if (bits % 8) {
            data[0] >>= (8 - (bits % 8));
        }
        out = BigNum(&data[0], data.size());
    } while (out >= range);
    fclose(rng);
    return out;
}

bool Parse(const ExpGraph::Ref& ref, const char* chr, size_t len, BigNum& out) {
    if (ref->len >= 0) {
        assert(ref->len == (int)len);
    }
    switch (ref->nodetype) {
    case ExpGraph::Node::NodeType::DICT: {
        auto it = ref->dict.find(std::string(chr, len));
        if (it == ref->dict.end()) {
//            fprintf(stderr, "Not find in dict: %.*s\n", (int)len, chr);
            return false;
        }
        uint32_t n = 0;
        while (it != ref->dict.begin()) {
            --it;
            ++n;
        }
        out = n;
        return true;
    }
    case ExpGraph::Node::NodeType::DISJUNCT: {
        BigNum ret;
        out = 0;
        for (const ExpGraph::Ref& sub : ref->refs) {
            if (Parse(sub, chr, len, ret)) {
                out += ret;
                return true;
            }
            out += sub->count;
        }
//        fprintf(stderr, "Not find in disjunct: %.*s\n", (int)len, chr);
        return false;
    }
    case ExpGraph::Node::NodeType::CONCAT: {
        BigNum mult = 1;
        BigNum ret;
        out = 0;
        for (const ExpGraph::Ref& sub : ref->refs) {
            if (!Parse(sub, chr, sub->len, ret)) {
//                fprintf(stderr, "Not find in concat: %.*s\n", (int)len, chr);
                return false;
            }
            chr += sub->len;
            out += mult * ret;
            mult *= sub->count;
        }
        return true;
    }
    }
    assert(false);
}

}


bool Parse(const ExpGraph::Ref& ref, const std::string& str, BigNum& out) {
    return Parse(ref, str.data(), str.size(), out);
}

std::string Generate(const ExpGraph::Ref& ref, BigNum&& num) {
    std::vector<char> out;
    size_t len = Generate(out, 0, ref, std::move(num));
    return std::string(out.begin(), out.begin() + len);
}

std::string Generate(const ExpGraph::Ref& ref) {
    BigNum num = RandomInteger(ref->count);
//    fprintf(stderr, "RNG: %s\n", num.hex().c_str());
    std::string str = Generate(ref, BigNum(num));
//    fprintf(stderr, "GEN: %s\n", str.c_str());
    BigNum nnum;
    bool ret = Parse(ref, str, nnum);
    assert(ret);
    assert(num == nnum);
    return str;
}
