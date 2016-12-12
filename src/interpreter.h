#ifndef _GRAMTROPY_INTERPRETER_H_
#define _GRAMTROPY_INTERPRETER_H_

#include "bignum.h"
#include <vector>
#include <string>
#include "strings.h"

struct FlatNode {
    enum NodeType {
        DICT,
        DISJUNCT,
        CONCAT
    };
    NodeType nodetype;
    BigNum count;
    size_t dict;
    std::vector<std::pair<size_t, size_t>> refs;
    int len;

    FlatNode(NodeType typ, int len_) : nodetype(typ), count(0), dict(0), len(len_) {}
};

struct FlatGraph {
    std::vector<FlatNode> nodes;
    std::vector<Strings> dicts;
};

bool Parse(const FlatGraph& graph, const FlatNode* ref, const std::string& str, BigNum& out);
std::string Generate(const FlatGraph& graph, const FlatNode* ref, BigNum&& num);

#endif
