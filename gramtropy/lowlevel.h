#ifndef _GRAMTROPY_LOWLEVEL_H_
#define _GRAMTROPY_LOWLEVEL_H_

#include "stringvector.h"
#include "bignum.h"


class Graph {
    enum NodeType {
        DICT,
        CAT,
        OR
    };

    struct Node {
        NodeType typ;
        uint32_t a,b;
        BigNum count;
        int refcount;
    };

    std::vector<StringVector> vect;
    std::vector<Node> nodes;

public:

    typedef uint32_t id;

    Graph() {
        vect.resize(1);
        nodes = {Node{DICT,0,0,0,0}};
    }

    id Empty() const { return 0; }

    id AddDict(
};
