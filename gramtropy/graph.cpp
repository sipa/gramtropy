#include "graph.h"

#include "tinyformat.h"
#include <map>

namespace {
    static std::string DescribeNode(Graph* graph, const Graph::Ref& node, std::map<const Graph::Node*, uint32_t>& m, std::vector<std::string>& rret) {
        if (m.count(&*node)) return tfm::format("n%u", m[&*node]);
        std::string inl;
        uint32_t pos = m.size();
        m[&*node] = pos;
        switch (node->nodetype) {
        case Graph::Node::UNDEF:
            inl = "???";
            break;
        case Graph::Node::NONE:
            inl = "none";
            break;
        case Graph::Node::EMPTY:
            inl = "\"\"";
            break;
        case Graph::Node::DICT: {
            inl = "(";
            bool first = true;
            for (const std::string& str : node->dict) {
                if (!first) inl += " | ";
                inl += "\"" + str + "\"";
                first = false;
            }
            inl += ")";
            break;
        }
        case Graph::Node::DISJUNCT: {
            inl = "(";
            bool first = true;
            for (const Graph::Ref& ref : node->refs) {
                if (!first) inl += " | ";
                inl += DescribeNode(graph, ref, m, rret);
                first = false;
            }
            inl += ")";
            break;
        }
        case Graph::Node::CONCAT: {
            inl = "(";
            bool first = true;
            for (const Graph::Ref& ref : node->refs) {
                if (!first) inl += " ";
                inl += DescribeNode(graph, ref, m, rret);
                first = false;
            }
            inl += ")";
            break;
        }
        default:
            assert(false);
            return "";
        }
        if (node.unique() && inl.size() <= 80) {
            return inl;
        }
        rret.push_back(tfm::format("n%u = %s;\n", pos, inl));
        return tfm::format("n%u", pos);
    }

    static std::string Describe(Graph* graph, const Graph::Ref& node) {
        std::map<const Graph::Node*, uint32_t> m;
        std::vector<std::string> res;
        std::string r = DescribeNode(graph, node, m, res);
        std::string ret;
        for (const std::string& r : res) {
            ret += r;
        }
        ret += "main = " + r + ";\n";
        return ret;
    }

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

    static void CollapseDisjunct(Graph* graph, const Graph::Ref& node, std::set<std::string>& dict, std::vector<Graph::Ref>& refs) {
        assert(node->nodetype == Graph::Node::DISJUNCT);
        for (Graph::Ref& ref : node->refs) {
            if (ref->nodetype == Graph::Node::NONE) continue;
            if (ref->nodetype == Graph::Node::DISJUNCT && ref.unique()) {
                CollapseDisjunct(graph, ref, dict, refs);
            } else if (ref->nodetype == Graph::Node::DICT && ref.unique()) {
                if (dict.size() < ref->dict.size()) {
                    dict.swap(ref->dict);
                }
                dict.insert(ref->dict.begin(), ref->dict.end());
            } else if (ref->nodetype == Graph::Node::CONCAT && ref->refs.size() == 1) {
                refs.emplace_back(ref->refs[0]);
            } else {
                refs.emplace_back(std::move(ref));
            }
        }
    }

    static void CollapseConcat(Graph* graph, const Graph::Ref& node, std::vector<Graph::Ref>& refs) {
        assert(node->nodetype == Graph::Node::CONCAT);
        for (Graph::Ref& ref : node->refs) {
            if (ref->nodetype == Graph::Node::EMPTY) continue;
            if (ref->nodetype == Graph::Node::CONCAT && ref.unique()) {
                CollapseConcat(graph, ref, refs);
            } else if (ref->nodetype == Graph::Node::DISJUNCT && ref->refs.size() == 1) {
                refs.emplace_back(ref->refs[0]);
            } else {
                refs.emplace_back(std::move(ref));
            }
        }
    }

    static bool OptimizeDisjunct(Graph* graph, const Graph::Ref& node) {
        assert(node->nodetype == Graph::Node::DISJUNCT);
        int nones = 0;
        int dicts = 0;
        int inlines = 0;
        int others = 0;
        for (const Graph::Ref& ref : node->refs) {
            if (ref->nodetype == Graph::Node::NONE) {
                nones++;
            } else if (ref->nodetype == Graph::Node::DISJUNCT && ref.unique()) {
                inlines++;
            } else if (ref->nodetype == Graph::Node::DICT && ref.unique()) {
                dicts++;
            } else {
                others++;
            }
        }
        if (dicts == 0 && inlines == 0 && others == 0) {
            node->nodetype = Graph::Node::NONE;
            node->refs.clear();
            return true;
        }
        if (nones == 0 && inlines == 0 && !(dicts == 0 && others == 1) && !(others == 0)) {
            return false;
        }
        std::vector<Graph::Ref> refs;
        std::set<std::string> dict;
        CollapseDisjunct(graph, node, dict, refs);
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
            return true;
        }
        return inlines != 0;
    }

    static bool OptimizeConcat(Graph* graph, const Graph::Ref& node) {
        assert(node->nodetype == Graph::Node::CONCAT);
        int empties = 0;
        int inlines = 0;
        int others = 0;
        for (const Graph::Ref& ref : node->refs) {
            if (ref->nodetype == Graph::Node::NONE) {
                node->nodetype = Graph::Node::NONE;
                node->refs.clear();
                return true;
            } else if (ref->nodetype == Graph::Node::EMPTY) {
                empties++;
            } else if (ref->nodetype == Graph::Node::CONCAT && ref.unique()) {
                inlines++;
            } else {
                others++;
            }
        }
        if (inlines == 0 && others == 0) {
            node->nodetype = Graph::Node::EMPTY;
            node->refs.clear();
            return true;
        }
        std::vector<Graph::Ref> refs;
        CollapseConcat(graph, node, refs);
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
        return inlines != 0;
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

std::string Describe(Graph& graph, const Graph::Ref& ref) {
    return Describe(&graph, ref);
}

Graph::Ref Graph::NewDict(std::set<std::string>&& dict) {
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
