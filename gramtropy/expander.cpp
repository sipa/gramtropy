#include "expander.h"

void Expander::AddDep(const Key& key, const ThunkRef& parent) {
    auto it = thunkmap.find(key);
    ThunkRef res;
    if (it == thunkmap.end()) {
        res = thunks.emplace_back(key);
        thunkmap[key] = res;
    } else {
        res = it->second;
    }
    if (!res->done) {
        AddTodo(res);
        if (parent) {
            res->forward.emplace(parent);
        }
    }
    if (parent) {
        parent->deps.emplace_back(std::move(res));
    }
}

void Expander::ProcessThunk(const ThunkRef& ref) {
//    fprintf(stderr, "Processing thunk %p\n", &*ref);
    if (ref->done) {
//        fprintf(stderr, "  done\n");
        return;
    }

    if (ref->need_expansion) {
//        fprintf(stderr, "  expanding: len=%i offset=%i\n", (int)ref->key.len, (int)ref->key.offset);
        ref->need_expansion = false;
        switch (ref->key.ref->nodetype) {
        case Graph::Node::NodeType::NONE:
//            fprintf(stderr, "    none\n");
            ref->done = true;
            break;
        case Graph::Node::NodeType::EMPTY:
//            fprintf(stderr, "    empty\n");
            ref->done = true;
            if (ref->key.len == 0) {
                if (!empty) {
                    empty = expgraph->NewDict({""});
                    assert(empty->len == 0);
                }
                ref->result = empty;
            }
            break;
        case Graph::Node::NodeType::DICT: {
            std::vector<std::string> vec;
            for (const auto& str : ref->key.ref->dict) {
                if (str.size() == ref->key.len) {
                    vec.push_back(str);
                }
            }
            ref->done = true;
//            fprintf(stderr, "    dict vec=%i/%i\n", (int)vec.size(), (int)ref->key.ref->dict.size());
            if (vec.size() > 0) {
                ref->result = expgraph->NewDict(std::move(vec));
                assert(ref->result->len == (int)ref->key.len);
            }
            break;
        }
        case Graph::Node::NodeType::DISJUNCT:
            assert(ref->key.ref->refs.size() >= 2);
//            fprintf(stderr, "    disjunct n=%i\n", (int)ref->key.ref->refs.size());
            ref->nodetype = ExpGraph::Node::NodeType::DISJUNCT;
            for (size_t i = 0; i < ref->key.ref->refs.size(); i++) {
                Key key(ref->key.len, ref->key.ref->refs[i]);
                AddDep(key, ref);
            }
            break;
        case Graph::Node::NodeType::CONCAT:
            assert(ref->key.ref->refs.size() >= 2 + ref->key.offset);
//            fprintf(stderr, "    concat n=%i\n", (int)ref->key.ref->refs.size());
            ref->nodetype = ExpGraph::Node::NodeType::DISJUNCT;
            for (size_t s = 0; s <= ref->key.len; s++) {
                auto sub = thunks.emplace_back();
                ref->deps.push_back(sub);
                sub->forward.insert(ref);
                sub->nodetype = ExpGraph::Node::NodeType::CONCAT;
                Key key1(s, ref->key.ref->refs[ref->key.offset]);
                AddDep(key1, sub);
                if (ref->key.ref->refs.size() == 2 + ref->key.offset) {
                    Key key2(ref->key.len - s, ref->key.ref->refs[ref->key.offset + 1]);
                    AddDep(key2, sub);
                } else {
                    Key key2(ref->key.len - s, ref->key.ref, ref->key.offset + 1);
                    AddDep(key2, sub);
                }
                AddTodo(sub);
            }
            break;
        default:
            assert(!"Unhandled graph type");
        }
    }

    if (!ref->done) {
//        fprintf(stderr, "  finalizing");
        switch (ref->nodetype) {
        case ExpGraph::Node::NodeType::DISJUNCT: {
            std::vector<ExpGraph::Ref> refs;
            BigNum count;
            bool waiting = false;
            for (const auto &sub : ref->deps) {
                if (!sub->done) {
                    waiting = true;
                    break;
                }
                if (sub->result) {
                    count += sub->result->count;
                    refs.push_back(sub->result);
                }
            }
            if (waiting) {
//                fprintf(stderr,"    disjunction: waiting\n");
                break;
            }
            ref->done = true;
//            fprintf(stderr, "    disjunction: done %i/%i\n", (int)refs.size(), (int)ref->deps.size());
            if (!refs.empty()) {
                ref->result = expgraph->NewDisjunct(std::move(refs));
            }
            break;
        }
        case ExpGraph::Node::NodeType::CONCAT: {
            std::vector<ExpGraph::Ref> refs;
            BigNum count = 1;
            bool waiting = false;
            bool none = false;
            for (const auto &sub : ref->deps) {
                if (!sub->done) {
                    waiting = true;
                } else if (!sub->result) {
                    none = true;
                    break;
                } else {
//                    fprintf(stderr, "    concatenation: adding sub len=%i count=%s\n", sub->result->len, sub->result->count.hex().c_str());
                    if (sub->result->len != 0) {
                        count *= sub->result->count;
                        refs.push_back(sub->result);
                    }
                }
            }
            if (waiting && !none) {
//                fprintf(stderr, "    concatenation: waiting\n");
                break;
            }
            ref->done = true;
            if (none || refs.empty()) {
//                fprintf(stderr, "    concatenation: none\n");
            } else {
//                fprintf(stderr, "    concatenation: done %i/%i\n", (int)refs.size(), (int)ref->deps.size());
                ref->result = expgraph->NewConcat(std::move(refs));
            }
            break;
        }
        default:
            assert(!"Unknown subgraph type");
        }
    }

    if (ref->done) {
        for (auto const &x : ref->forward) {
            AddTodo(x, true);
        }
        ref->forward.clear();
        for (auto const &x : ref->deps) {
            x->forward.erase(ref);
        }
        ref->deps.clear();
    }
}

void Expander::AddTodo(const ThunkRef& ref, bool priority) {
    auto ret = todo_set.insert(ref);
    if (ret.second) {
        if (priority) {
            todo.push_front(ret.first);
        } else {
            todo.push_back(ret.first);
        }
    }
}

ExpGraph::Ref Expander::Expand(const Graph::Ref& ref, size_t len) {
    Key key(len, ref);

    ThunkRef dummy;
    AddDep(key, dummy);

    while (!thunkmap[key]->done) {
        if (todo.empty()) {
            assert(!"Nothing to process left");
        }
        auto nowit = todo.front();
        todo.pop_front();
        auto now = *nowit;
        todo_set.erase(nowit);

        ProcessThunk(now);
    }

    fprintf(stderr, "%i thunks left todo\n", (int)todo.size());

    return thunkmap[key]->result;
}
