#ifndef _GRAMTROPY_EXPANDER_H_
#define _GRAMTROPY_EXPANDER_H_ 1

#include "rclist.h"
#include "bignum.h"
#include "graph.h"
#include "expgraph.h"

#include <deque>
#include <vector>
#include <set>
#include <map>

class Expander {
    const Graph* graph;
    ExpGraph* expgraph;

    struct Key {
        size_t len;
        size_t offset;
        const Graph::Node* ref;

        friend bool operator==(const Key& x, const Key& y) {
            return x.len == y.len && x.offset == y.offset && x.ref == y.ref;
        }

        friend bool operator<(const Key& x, const Key& y) {
            return x.len < y.len || (x.len == y.len && ((x.offset == y.offset && x.ref < y.ref) || x.offset < y.offset));
        }

        Key() : len(0), offset(0), ref(nullptr) {}
        Key(size_t len_, const Graph::Ref& ref_, size_t offset_ = 0) : len(len_), offset(offset_), ref(&*ref_) {}
        Key(size_t len_, const Graph::Node* ref_, size_t offset_ = 0) : len(len_), offset(offset_), ref(ref_) {}
    };

    ExpGraph::Ref empty;

    struct Thunk;
    typedef rclist<Thunk>::fixed_iterator ThunkRef;

    struct Thunk {
        bool need_expansion;
        bool done;
        bool todo;

        Expander::Key key;
        ExpGraph::Node::NodeType nodetype;
        ExpGraph::Ref result;
        std::vector<ThunkRef> deps;
        std::set<ThunkRef> forward;

        Thunk(const Expander::Key& key_) : need_expansion(true), done(false), todo(false), key(key_) {}
        Thunk() : need_expansion(false), done(false), todo(false) {}
    };

    rclist<Thunk> thunks;
    std::deque<ThunkRef> todo;
    std::map<Key, ThunkRef> thunkmap;

    void AddTodo(const ThunkRef& ref, bool priority = false);
    void AddDep(const Key& key, const ThunkRef& parent);
    void ProcessThunk(ThunkRef ref);

public:
    Expander(const Graph* graph_, ExpGraph* expgraph_) : graph(graph_), expgraph(expgraph_) {}

    ExpGraph::Ref Expand(const Graph::Ref& ref, size_t len);
};

#endif
