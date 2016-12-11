#ifndef _GRAMTROPY_GRAPH_H_
#define _GRAMTROPY_GRAPH_H_

#include <assert.h>

#include "rclist.h"

#include <string>
#include <vector>
#include <set>
#include <stdint.h>

class Graph;

class GraphNode {
    public:
    enum NodeType {
        UNDEF,
        NONE, // {}
        EMPTY, // {""}
        DICT, // {a...}
        CONCAT,
        DISJUNCT,
        DEDUP,
    };

    GraphNode(NodeType typ) : nodetype(typ) {}

    NodeType nodetype;
    std::vector<std::string> dict;
    std::vector<rclist<GraphNode>::fixed_iterator> refs;
};

class Graph : public rclist<GraphNode> {
public:
    typedef GraphNode Node;
    typedef fixed_iterator Ref;

    Ref NewNode(Node::NodeType);

    Ref NewNone();
    Ref NewEmpty();
    Ref NewUndefined();
    Ref NewDict(std::vector<std::string>&& dict);
    Ref NewConcat(std::vector<Ref>&& refs);
    Ref NewDisjunct(std::vector<Ref>&& refs);
    Ref NewDedup(Ref&& ref);

    template<typename S>
    Ref NewString(S&& str) {
        return NewDict({std::forward<S>(str)});
    }

    template<typename T1, typename T2>
    Ref NewConcat(T1&& t1, T2&& t2) {
        return NewConcat({std::forward<T1>(t1), std::forward<T2>(t2)});
    }

    template<typename T1, typename T2>
    Ref NewDisjunct(T1&& t1, T2&& t2) {
        return NewDisjunct({std::forward<T1>(t1), std::forward<T2>(t2)});
    }

    void Define(const Ref& undef, Ref&& definition);
    bool FullyDefined();
    bool IsDefined(const Ref& ref);

};

void Optimize(Graph& graph);
void OptimizeRef(Graph& graph, Graph::Ref& ref);

#endif
