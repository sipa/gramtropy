#include "interpreter.h"
#include <assert.h>

namespace {

size_t Generate(std::vector<char>& out, size_t pos, const FlatGraph& graph, const FlatNode* ref, BigNum&& num) {
//    fprintf(stderr, "gen pos=%i len=%i type=%i\n", (int)pos, (int)ref->len, (int)ref->nodetype);
    if (ref->len >= 0) {
        if (out.size() < pos + ref->len) {
            out.resize(pos + ref->len);
        }
    }
    switch (ref->nodetype) {
    case FlatNode::NodeType::DICT: {
        assert(num.bits() <= 32);
        const Strings& strings = graph.dicts[ref->dict];
        uint32_t n = num.get_ui();
        memcpy(out.data() + pos, &*strings.StringBegin(n), ref->len);
        return pos + ref->len;
    }
    case FlatNode::NodeType::DISJUNCT:
        for (const auto& sub : ref->refs) {
            const FlatNode* subnode = &graph.nodes[sub.second];
            if (num < subnode->count) {
                return Generate(out, pos, graph, subnode, std::move(num));
            }
            num -= subnode->count;
        }
        assert(false);
    case FlatNode::NodeType::CONCAT:
        for (const auto& sub : ref->refs) {
            const FlatNode* subnode = &graph.nodes[sub.second];
            BigNum div = num.divmod(subnode->count);
            pos = Generate(out, pos + sub.first, graph, subnode, std::move(num));
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

bool Parse(const FlatGraph& graph, const FlatNode* ref, const char* chr, int len, BigNum& out) {
    if (ref->len >= 0) {
        if (len != ref->len) {
            return false;
        }
    }
    switch (ref->nodetype) {
    case FlatNode::NodeType::DICT: {
        const auto& dict = graph.dicts[ref->dict];
        int ret = dict.find(chr, len);
        if (ret == -1) {
//            fprintf(stderr, "Not find in dict: %.*s\n", (int)len, chr);
            return false;
        }
        out = ret;
        return true;
    }
    case FlatNode::NodeType::DISJUNCT: {
        BigNum ret;
        out = 0;
        for (const auto& sub : ref->refs) {
            const FlatNode* subnode = &graph.nodes[sub.second];
            if (Parse(graph, subnode, chr, len, ret)) {
                out += ret;
                return true;
            }
            out += subnode->count;
        }
//        fprintf(stderr, "Not find in disjunct: %.*s\n", (int)len, chr);
        return false;
    }
    case FlatNode::NodeType::CONCAT: {
        BigNum mult = 1;
        BigNum ret;
        out = 0;
        for (const auto& sub : ref->refs) {
            const FlatNode* subnode = &graph.nodes[sub.second];
            if (!Parse(graph, subnode, chr + sub.first, subnode->len, ret)) {
//                fprintf(stderr, "Not find in concat: %.*s\n", (int)len, chr);
                return false;
            }
            out += mult * ret;
            mult *= subnode->count;
        }
        return true;
    }
    }
    assert(false);
}

}

bool Parse(const FlatGraph& graph, const FlatNode* ref, const std::string& str, BigNum& out) {
    return Parse(graph, ref, str.data(), str.size(), out);
}

std::string Generate(const FlatGraph& graph, const FlatNode* ref, BigNum&& num) {
    std::vector<char> out;
    size_t len = Generate(out, 0, graph, ref, std::move(num));
    return std::string(out.begin(), out.begin() + len);
}

std::string Generate(const FlatGraph& graph, const FlatNode* ref) {
    BigNum num = RandomInteger(ref->count);
//    fprintf(stderr, "RNG: %s\n", num.hex().c_str());
    std::string str = Generate(graph, ref, BigNum(num));
//    fprintf(stderr, "GEN: %s\n", str.c_str());
    BigNum nnum;
    bool ret = Parse(graph, ref, str, nnum);
    assert(ret);
    assert(num == nnum);
    return str;
}
