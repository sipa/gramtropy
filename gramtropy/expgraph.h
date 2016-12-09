#ifndef _GRAMTROPY_EXPGRAPH_H_
#define _GRAMTROPY_EXPGRAPH_H_ 1

#include "rclist.h"
#include "bignum.h"
#include "graph.h"
#include "strings.h"

#include <vector>

class ExpGraph {
public:
    class Node;

    typedef rclist<Node>::fixed_iterator Ref;

    class Node {
        public:
        enum NodeType {
            DICT,
            CONCAT,
            DISJUNCT
        };

        NodeType nodetype;
        BigNum count;
        std::vector<Ref> refs;
        std::set<std::string> dict;
        int len;

        Node(NodeType nodetype_) : nodetype(nodetype_), len(-1) {}
    };

    Ref NewDict(std::set<std::string>&& dict);
    Ref NewConcat(std::vector<Ref>&& refs);
    Ref NewDisjunct(std::vector<Ref>&& refs);

    rclist<Node> nodes;
};

void Optimize(ExpGraph& graph);

std::string Generate(const ExpGraph::Ref& ref, BigNum&& num);
std::string Generate(const ExpGraph::Ref& ref);

bool Parse(const ExpGraph::Ref& ref, const std::string& str);

#endif
