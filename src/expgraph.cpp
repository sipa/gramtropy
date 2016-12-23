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

std::vector<std::string> InlineDict(const ExpGraph::Ref& ref, size_t offset = 0) {
    std::vector<std::string> res;
    switch (ref->nodetype) {
    case ExpGraph::Node::NodeType::DICT:
        return std::vector<std::string>(ref->dict.begin(), ref->dict.end());
    case ExpGraph::Node::NodeType::DISJUNCT:
        for (const ExpGraph::Ref& sub : ref->refs) {
            std::vector<std::string> s = InlineDict(sub);
            if (s.size() < res.size()) {
                res.swap(s);
            }
            res.insert(res.end(), s.begin(), s.end());
        }
        break;
    case ExpGraph::Node::NodeType::CONCAT:
        if (offset + 1 == ref->refs.size()) {
            return InlineDict(ref->refs[offset]);
        } else {
            std::vector<std::string> s1 = InlineDict(ref->refs[offset]);
            std::vector<std::string> s2 = InlineDict(ref, offset + 1);
            for (const auto& str1 : s1) {
                for (const auto& str2 : s2) {
                    res.emplace_back(str1 + str2);
                }
            }
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
        if (ref->count.bits() <= 6) {
            auto x = InlineDict(ref);
            std::set<std::string> dict;
            assert(ref->count.get_ui() == x.size());
            for (auto& str : x) {
                if (dict.count(str)) {
                    fprintf(stderr, "Duplicate expansion for %s\n", str.c_str());
                } else {
                    dict.emplace(std::move(str));
                }
            }
            ref->dict = std::move(dict);
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

std::set<std::string> Inline(const ExpGraph::Ref& ref) {
    auto x = InlineDict(ref);
    std::set<std::string> dict;
    for (auto& str : x) {
        dict.emplace(std::move(str));
    }
    return dict;
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
