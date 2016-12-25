#include "expander.h"
#include <algorithm>

ExpGraph::Ref Expander::MakeNonDict(std::vector<ExpGraph::Ref>&& refs, ExpGraph::Node::NodeType nodetype, bool sort) {
    if (sort) {
        std::sort(refs.begin(), refs.end());
    }
    std::pair<ExpGraph::Node::NodeType, ComparablePointer<std::vector<ExpGraph::Ref>>> key(nodetype, MakeComparable(&refs));
    auto fnd = nodemap.find(key);
    if (fnd != nodemap.end()) {
        return fnd->second;
    }
    ExpGraph::Ref ret;
    switch (nodetype) {
    case ExpGraph::Node::NodeType::CONCAT:
        ret = expgraph->NewConcat(std::move(refs));
        break;
    case ExpGraph::Node::NodeType::DISJUNCT:
        ret = expgraph->NewDisjunct(std::move(refs));
        break;
    default:
        assert(!"Unknown type");
    }
    if (ret->nodetype == nodetype) {
        key.second = &ret->refs;
        nodemap.emplace(std::move(key), ret);
    }
    return ret;
}

ExpGraph::Ref Expander::MakeDict(std::set<std::string>&& dict) {
    if (dict.size() == 0) {
        return ExpGraph::Ref();
    }
    auto fnd = dictmap.find(MakeComparable(&dict));
    if (fnd != dictmap.end()) {
        return fnd->second;
    }
    auto ret = expgraph->NewDict(std::move(dict));
    dictmap.emplace(MakeComparable(&ret->dict), ret);
    return ret;
}

ExpGraph::Ref Expander::MakeDisjunct(std::vector<ExpGraph::Ref>&& refs) {
    if (refs.size() == 0) {
        return ExpGraph::Ref();
    }
    auto ret = MakeNonDict(std::move(refs), ExpGraph::Node::NodeType::DISJUNCT, true);
    if (ret->count.bits() <= 6) {
        auto inl = Inline(ret);
        return MakeDict(std::move(inl));
    }
    return ret;
}

ExpGraph::Ref Expander::MakeConcat(std::vector<ExpGraph::Ref>&& refs) {
    if (refs.size() == 0) {
        return MakeDict({""});
    }
    return MakeNonDict(std::move(refs), ExpGraph::Node::NodeType::CONCAT, false);
}

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

bool Expander::ProcessThunk(ThunkRef ref, std::string& error) {
//    fprintf(stderr, "Processing thunk %p\n", &*ref);
    if (ref->done) {
//        fprintf(stderr, "  done\n");
        return true;
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
        case Graph::Node::NodeType::DICT: {
            std::set<std::string> vec;
            if (ref->key.ref->nodetype == Graph::Node::NodeType::EMPTY && ref->key.len == 0) {
                vec.insert("");
            } else {
                for (const auto& str : ref->key.ref->dict) {
                    if (str.size() == ref->key.len) {
                        if (vec.count(str)) {
                            error = "duplicate string '" + str + "'";
                            return false;
                        }
                        vec.insert(str);
                    }
                }
            }
            ref->done = true;
            ref->result = MakeDict(std::move(vec));
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
                // Bisect the list of concatenation elements.
                size_t total = ref->key.ref->refs.size();
                size_t mid = (ref->key.offset + total - ref->key.cutoff + 1) / 2;
                Key key1(s, ref->key.ref, ref->key.offset, total - mid);
                Key key2(ref->key.len - s, ref->key.ref, mid, ref->key.cutoff);
                // If either of the two halves result in a single node, descend into it instead.
                if (total == key1.offset + key1.cutoff + 1) {
                    key1 = Key(s, ref->key.ref->refs[key1.offset]);
                }
                if (total == key2.offset + key2.cutoff + 1) {
                    key2 = Key(ref->key.len - s, ref->key.ref->refs[key2.offset]);
                }
                // Check if we already know to have no solutions for any of the two sides.
                auto fnd1 = thunkmap.find(key1), fnd2 = thunkmap.find(key2);
                if (fnd1 != thunkmap.end() && fnd1->second->done && !fnd1->second->result) {
                    continue;
                }
                if (fnd2 != thunkmap.end() && fnd2->second->done && !fnd2->second->result) {
                    continue;
                }
                // Create thunk for the concatenation of the two halves.
                auto sub = thunks.emplace_back();
                ref->deps.push_back(sub);
                sub->forward.insert(ref);
                sub->nodetype = Thunk::ThunkType::CONCAT;
                if (key1.len <= key2.len) {
                    AddDep(key1, sub);
                    AddDep(key2, sub);
                } else {
                    // Make sure the shorter length is expanded first.
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
        case Graph::Node::NodeType::LENLIMIT: {
            assert(ref->key.ref->refs.size() == 1);
            if (ref->key.len < ref->key.ref->par1 || ref->key.len > ref->key.ref->par2) {
                ref->done = true;
                break;
            }
            ref->nodetype = Thunk::ThunkType::COPY;
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
            ref->result = MakeDisjunct(std::move(refs));
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
            if (!none) {
                ref->result = MakeConcat(std::move(refs));
            }
            break;
        }
        case Thunk::ThunkType::DEDUP: {
            assert(ref->deps.size() == 1);
            if (!ref->deps[0]->done) {
                break;
            }
            ref->done = true;
            if (!ref->deps[0]->result) {
                break;
            }
            if (ref->deps[0]->result->count.bits() > 30) {
                error = "deduplication of very large set not possible";
                return false;
            }
            auto inl = Inline(ref->deps[0]->result);
            auto sub = MakeDict(std::move(inl));
            if (sub->count == ref->deps[0]->result->count) {
                // Optimization: replace argument with expanded dictionary if there are no duplicates.
                ref->deps[0]->result = sub;
            }
            ref->result = std::move(sub);
            break;
        }
        case Thunk::ThunkType::COPY: {
            assert(ref->deps.size() == 1);
            if (!ref->deps[0]->done) {
                break;
            }
            ref->done = true;
            if (!ref->deps[0]->result) {
                break;
            }
            ref->result = ref->deps[0]->result;
            break;
        }
        default:
            assert(!"Unknown thunk type");
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

    return true;
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

    std::string error;

    while (!thunkmap[key]->done && expgraph->nodes.size() <= max_nodes && thunks.size() <= max_thunks) {
        if (todo.empty()) {
            return std::make_pair(ExpGraph::Ref(), "infinite recursion");
        }
        ThunkRef now = std::move(todo.front());
        todo.pop_front();
        now->todo = false;

        if (!ProcessThunk(std::move(now), error)) {
            return std::make_pair(ExpGraph::Ref(), error);
        }
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
