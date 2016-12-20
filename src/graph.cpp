#include "graph.h"

#include <map>

namespace {
    static bool OptimizeRefInternal(Graph* graph, Graph::Ref& node) {
        if ((node->nodetype == Graph::Node::DISJUNCT || node->nodetype == Graph::Node::CONCAT) && node->refs.size() == 1) {
            node = node->refs[0];
            return true;
        }
        return false;
    }

    static bool OptimizeDict(Graph* graph, const Graph::Ref& node) {
        assert(node->nodetype == Graph::Node::DICT);
        if (node->dict.empty()) {
            node->nodetype = Graph::Node::NONE;
            return true;
        }
        if (node->dict.size() == 1 && node->dict.begin()->size() == 0) {
            node->nodetype = Graph::Node::EMPTY;
            node->dict.clear();
            return true;
        }
        return false;
    }

    static bool CollapseDisjunct(Graph* graph, const Graph::Ref& node, std::vector<std::string>& dict, std::vector<Graph::Ref>& refs) {
        assert(node->nodetype == Graph::Node::DISJUNCT);
        bool modified = false;
        for (Graph::Ref& ref : node->refs) {
            bool modify = true;
            if (ref->nodetype == Graph::Node::NONE) {
                // Nothing to do.
            } else if (ref->nodetype == Graph::Node::DISJUNCT && ref.unique()) {
                CollapseDisjunct(graph, ref, dict, refs);
            } else if (ref->nodetype == Graph::Node::DICT && ref.unique()) {
                if (dict.empty()) {
                    // The first dict being included - not necessarily a modification.
                    modify = false;
                }
                if (dict.size() < ref->dict.size()) {
                    dict.swap(ref->dict);
                }
                for (auto& str : ref->dict) {
                    dict.emplace_back(std::move(str));
                }
            } else if (ref->nodetype == Graph::Node::CONCAT && ref->refs.size() == 1) {
                refs.emplace_back(ref->refs[0]);
            } else {
                modify = false;
                refs.emplace_back(std::move(ref));
            }
            modified |= modify;
        }
        return modified;
    }

    static bool CollapseConcat(Graph* graph, const Graph::Ref& node, std::vector<Graph::Ref>& refs) {
        assert(node->nodetype == Graph::Node::CONCAT);
        bool modified = false;
        for (Graph::Ref& ref : node->refs) {
            bool modify = true;
            if (ref->nodetype == Graph::Node::EMPTY) {
                // Nothing to do.
            } else if (ref->nodetype == Graph::Node::CONCAT && ref.unique()) {
                CollapseConcat(graph, ref, refs);
            } else if (ref->nodetype == Graph::Node::DISJUNCT && ref->refs.size() == 1) {
                refs.emplace_back(ref->refs[0]);
            } else if (!refs.empty() && ref->nodetype == Graph::Node::DICT && refs.back()->nodetype == Graph::Node::DICT && ref.unique() && refs.back().unique() && (ref->dict.size() == 1 || refs.back()->dict.size() == 1)) {
                std::vector<std::string> strs;
                for (const auto& str1 : refs.back()->dict) {
                    for (const auto& str2 : ref->dict) {
                        strs.emplace_back(str1 + str2);
                    }
                }
                refs.back()->dict = std::move(strs);
                ref = Graph::Ref();
            } else {
                modify = false;
                refs.emplace_back(std::move(ref));
            }
            modified |= modify;
        }
        return modified;
    }

    static bool OptimizeDisjunct(Graph* graph, const Graph::Ref& node) {
        assert(node->nodetype == Graph::Node::DISJUNCT);
        std::vector<Graph::Ref> refs;
        std::vector<std::string> dict;
        bool modified = CollapseDisjunct(graph, node, dict, refs);
        if (dict.size() == 0 && refs.size() == 0) {
            node->refs.clear();
            node->nodetype = Graph::Node::EMPTY;
            return true;
        }
        if (dict.size() == 0 && refs.size() == 1 && refs[0].unique()) {
            node->refs = std::move(refs[0]->refs);
            node->dict = std::move(refs[0]->dict);
            node->nodetype = refs[0]->nodetype;
            return true;
        }
        node->refs = std::move(refs);
        if (node->refs.size() == 0) {
            node->nodetype = Graph::Node::DICT;
            node->dict = std::move(dict);
            OptimizeDict(graph, node);
            return true;
        }
        if (dict.size() != 0) {
            Graph::Ref newdict = graph->NewNode(Graph::Node::DICT);
            newdict->dict = std::move(dict);
            OptimizeDict(graph, newdict);
            node->refs.emplace_back(std::move(newdict));
        }
        return modified;
    }

    static bool OptimizeConcat(Graph* graph, const Graph::Ref& node) {
        assert(node->nodetype == Graph::Node::CONCAT);
        for (const Graph::Ref& ref : node->refs) {
            if (ref->nodetype == Graph::Node::NONE) {
                node->nodetype = Graph::Node::NONE;
                node->refs.clear();
                return true;
            }
        }
        std::vector<Graph::Ref> refs;
        bool modified = CollapseConcat(graph, node, refs);
        node->refs.clear();
        if (refs.size() == 1 && refs[0].unique()) {
            node->refs = std::move(refs[0]->refs);
            node->dict = std::move(refs[0]->dict);
            node->nodetype = refs[0]->nodetype;
            return true;
        }
        node->refs = std::move(refs);
        if (node->refs.size() == 0) {
            node->nodetype = Graph::Node::EMPTY;
            return true;
        }
        return modified;
    }

    static bool Optimize(Graph* graph, const Graph::Ref& node) {
        bool ret = false;
        if (node->nodetype == Graph::Node::DISJUNCT) {
            ret |= OptimizeDisjunct(graph, node);
        }
        if (node->nodetype == Graph::Node::CONCAT) {
            ret |= OptimizeConcat(graph, node);
        }
        if (node->nodetype == Graph::Node::DICT) {
            ret |= OptimizeDict(graph, node);
        }
        return ret;
    };
}

void Optimize(Graph& graph) {
    bool any;
    do {
        any = false;
        for (auto it = graph.begin(); it != graph.end(); it++) {
            bool now = Optimize(&graph, it);
            any |= now;
        }
    } while(any);
}

void OptimizeRef(Graph& graph, Graph::Ref& ref) {
    OptimizeRefInternal(&graph, ref);
}

Graph::Ref Graph::NewNode(Graph::Node::NodeType typ) {
    return emplace_back(typ);
}

Graph::Ref Graph::NewUndefined() {
    return NewNode(Graph::Node::UNDEF);
}

void Graph::Define(const Graph::Ref& undef, Graph::Ref&& definition) {
    assert(undef->nodetype == Graph::Node::UNDEF);
    if (definition.unique()) {
        undef->nodetype = definition->nodetype;
        undef->refs = std::move(definition->refs);
        undef->dict = std::move(definition->dict);
        definition = Graph::Ref();
    } else {
        undef->nodetype = Graph::Node::DISJUNCT;
        undef->refs.resize(1);
        undef->refs[0] = std::move(definition);
        undef->dict.clear();
    }
}

Graph::Ref Graph::NewEmpty() {
    return NewNode(Graph::Node::EMPTY);
}

Graph::Ref Graph::NewNone() {
    return NewNode(Graph::Node::NONE);
}

bool Graph::FullyDefined() {
    for (const Graph::Node& node : *this) {
        if (node.nodetype == Graph::Node::UNDEF) {
            return false;
        }
    }
    return true;
}

bool Graph::IsDefined(const Graph::Ref& ref) {
    return ref->nodetype != Graph::Node::UNDEF;
}

Graph::Ref Graph::NewDedup(Graph::Ref&& ref) {
    Graph::Ref ret;
    if (ref->nodetype == Graph::Node::DEDUP || ref->nodetype == Graph::Node::DICT) {
        ret = std::move(ref);
    } else {
        ret = NewNode(Graph::Node::DEDUP);
        ret->refs = {std::move(ref)};
    }
    return ret;
}

Graph::Ref Graph::NewDict(std::vector<std::string>&& dict) {
    Graph::Ref ret = NewNode(Graph::Node::DICT);
    ret->dict = std::move(dict);
    Optimize(this, ret);
    return ret;
}

Graph::Ref Graph::NewConcat(std::vector<Graph::Ref>&& refs) {
    Graph::Ref ret;
    if (refs.size() == 1) {
        ret = std::move(refs[0]);
        refs.clear();
        return ret;
    }
    if (refs.size() == 0) {
        ret = NewNode(Graph::Node::EMPTY);
    } else {
        ret = NewNode(Graph::Node::CONCAT);
        ret->refs = std::move(refs);
        Optimize(this, ret);
    }
    return ret;
}

Graph::Ref Graph::NewDisjunct(std::vector<Graph::Ref>&& refs) {
    Graph::Ref ret;
    if (refs.size() == 1) {
        ret = std::move(refs[0]);
        refs.clear();
        return ret;
    }
    if (refs.size() == 0) {
        ret = NewNode(Graph::Node::NONE);
    } else {
        ret = NewNode(Graph::Node::DISJUNCT);
        ret->refs = std::move(refs);
        Optimize(this, ret);
    }
    return ret;
}
