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

void Expander::ProcessThunk(ThunkRef ref) {
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
            std::set<std::string> vec;
            for (const auto& str : ref->key.ref->dict) {
                if (str.size() == ref->key.len) {
                    vec.insert(str);
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
            if (ref->key.ref->refs.size() == 0) {
                ref->done = true;
            } else {
//                fprintf(stderr, "    disjunct n=%i\n", (int)ref->key.ref->refs.size());
                ref->nodetype = Thunk::ThunkType::DISJUNCT;
                for (size_t i = 0; i < ref->key.ref->refs.size(); i++) {
                    Key key(ref->key.len, ref->key.ref->refs[i]);
                    AddDep(key, ref);
                }
                break;
            }
        case Graph::Node::NodeType::CONCAT:
//            fprintf(stderr, "    concat n=%i\n", (int)ref->key.ref->refs.size());
            ref->nodetype = Thunk::ThunkType::DISJUNCT;
            for (size_t s = 0; s <= ref->key.len; s++) {
                Key key1(s, ref->key.ref->refs[ref->key.offset]);
                Key key2;
                if (ref->key.ref->refs.size() == 2 + ref->key.offset) {
                    key2 = Key(ref->key.len - s, ref->key.ref->refs[ref->key.offset + 1]);
                } else {
                    key2 = Key(ref->key.len - s, ref->key.ref, ref->key.offset + 1);
                }
                auto fnd1 = thunkmap.find(key1), fnd2 = thunkmap.find(key2);
                if (fnd1 != thunkmap.end() && fnd1->second->done && !fnd1->second->result) {
                    continue;
                }
                if (fnd2 != thunkmap.end() && fnd2->second->done && !fnd2->second->result) {
                    continue;
                }
                auto sub = thunks.emplace_back();
                ref->deps.push_back(sub);
                sub->forward.insert(ref);
                sub->nodetype = Thunk::ThunkType::CONCAT;
                if (key1.len <= key2.len) {
                    AddDep(key1, sub);
                    AddDep(key2, sub);
                } else {
                    AddDep(key2, sub);
                    AddDep(key1, sub);
                    sub->deps[0].swap(sub->deps[1]);
                }
                AddTodo(sub, true);
            }
            if (ref->deps.size() == 0) {
                ref->done = true;
            }
            break;
        case Graph::Node::NodeType::DEDUP: {
            ref->nodetype = Thunk::ThunkType::DEDUP;
            assert(ref->key.ref->refs.size() == 1);
            Key key(ref->key.len, ref->key.ref->refs[0]);
            AddDep(key, ref);
            break;
        }
        default:
            assert(!"Unhandled graph type");
        }
    }

    if (!ref->done) {
//        fprintf(stderr, "  finalizing");
        switch (ref->nodetype) {
        case Thunk::ThunkType::DISJUNCT: {
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
        case Thunk::ThunkType::CONCAT: {
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
        case Thunk::ThunkType::DEDUP: {
            // TODO: enforce maximum node count on deduplication
            assert(ref->deps.size() == 1);
            if (!ref->deps[0]->done) {
                break;
            }
            ref->done = true;
            if (!ref->deps[0]->result) {
                break;
            }
            auto inl = Inline(ref->deps[0]->result);
            auto sub = expgraph->NewDict(std::move(inl));
            if (sub->count == ref->deps[0]->result->count) {
                // Optimization: replace argument with expanded dictionary if there are no duplicates.
                ref->deps[0]->result = sub;
            }
            ref->result = std::move(sub);
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
    if (ref->todo) {
        return;
    }
    ref->todo = true;
    if (priority) {
        todo.push_front(ref);
    } else {
        todo.push_back(ref);
    }
}

std::pair<ExpGraph::Ref, std::string> Expander::Expand(const Graph::Ref& ref, size_t len) {
    Key key(len, ref);

    ThunkRef dummy;
    AddDep(key, dummy);

    while (!thunkmap[key]->done && expgraph->nodes.size() <= max_nodes && thunks.size() <= max_thunks) {
        if (todo.empty()) {
            return std::make_pair(ExpGraph::Ref(), "infinite recursion");
        }
        ThunkRef now = std::move(todo.front());
        todo.pop_front();
        now->todo = false;

        ProcessThunk(std::move(now));
    }

    if (expgraph->nodes.size() > max_nodes) {
        return std::make_pair(ExpGraph::Ref(), "maximum node count exceeded");
    }

    if (thunks.size() > max_thunks) {
        return std::make_pair(ExpGraph::Ref(), "maximum thunk count exceeded");
    }

    return std::make_pair(thunkmap[key]->result, "");
}

Expander::~Expander() {
    todo.clear();
    thunkmap.clear();
}
