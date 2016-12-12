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
            Generate(out, pos + sub.first, graph, subnode, std::move(num));
            num = std::move(div);
        }
        return pos + ref->len;
    }
    assert(false);
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
        assert(out < ref->count);
        return true;
    }
    case FlatNode::NodeType::DISJUNCT: {
        BigNum ret;
        out = 0;
        for (const auto& sub : ref->refs) {
            const FlatNode* subnode = &graph.nodes[sub.second];
            if (Parse(graph, subnode, chr, len, ret)) {
                out += ret;
                assert(out < ref->count);
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
        assert(out < ref->count);
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
