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

template <typename T>
class ComparablePointer {
    const T* ptr;
public:
    ComparablePointer(const T* ptr_) : ptr(ptr_) {}
    const T& operator*() const { return *ptr; }
    const T* operator->() const { return ptr; }

    ComparablePointer& operator=(const T* ptr_) { ptr = ptr_; return *this; }

    friend bool operator==(const ComparablePointer& x, const ComparablePointer& y) { return *x == *y; }
    friend bool operator!=(const ComparablePointer& x, const ComparablePointer& y) { return *x != *y; }
    friend bool operator<(const ComparablePointer& x, const ComparablePointer& y) { return *x < *y; }
    friend bool operator>(const ComparablePointer& x, const ComparablePointer& y) { return *x > *y; }
    friend bool operator<=(const ComparablePointer& x, const ComparablePointer& y) { return *x <= *y; }
    friend bool operator>=(const ComparablePointer& x, const ComparablePointer& y) { return *x >= *y; }
};

template <typename T>
ComparablePointer<T> MakeComparable(const T* x) { return ComparablePointer<T>(x); }

class Expander {
    const Graph* graph;
    ExpGraph* expgraph;

    size_t max_nodes;
    size_t max_thunks;

    struct Key {
        size_t len;
        size_t offset;
        size_t cutoff;
        const Graph::Node* ref;

        friend bool operator==(const Key& x, const Key& y) {
            return x.len == y.len && x.offset == y.offset && x.cutoff == y.cutoff && x.ref == y.ref;
        }

        friend bool operator<(const Key& x, const Key& y) {
            if (x.len != y.len) return x.len < y.len;
            if (x.cutoff != y.cutoff) return x.cutoff < y.cutoff;
            if (x.offset != y.offset) return x.offset < y.offset;
            return x.ref < y.ref;
        }

        Key() : len(0), offset(0), cutoff(0), ref(nullptr) {}
        Key(size_t len_, const Graph::Ref& ref_, size_t offset_ = 0, size_t cutoff_ = 0) : len(len_), offset(offset_), cutoff(cutoff_), ref(&*ref_) {}
        Key(size_t len_, const Graph::Node* ref_, size_t offset_ = 0, size_t cutoff_ = 0) : len(len_), offset(offset_), cutoff(cutoff_), ref(ref_) {}
    };

    std::map<ComparablePointer<std::set<std::string>>, ExpGraph::Ref> dictmap;
    std::map<std::pair<ExpGraph::Node::NodeType, ComparablePointer<std::vector<ExpGraph::Ref>>>, ExpGraph::Ref> nodemap;

    struct Thunk;
    typedef rclist<Thunk>::fixed_iterator ThunkRef;

    struct Thunk {

        enum ThunkType {
            DICT,
            CONCAT,
            DISJUNCT,
            DEDUP,
        };

        bool need_expansion;
        bool done;
        bool todo;

        Expander::Key key;
        ThunkType nodetype;
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
    bool ProcessThunk(ThunkRef ref, std::string& error);

public:
    Expander(const Graph* graph_, ExpGraph* expgraph_, size_t max_nodes_, size_t max_thunks_) : graph(graph_), expgraph(expgraph_), max_nodes(max_nodes_), max_thunks(max_thunks_) {}

    ~Expander();

    std::pair<ExpGraph::Ref, std::string> Expand(const Graph::Ref& ref, size_t len);
};

#endif
