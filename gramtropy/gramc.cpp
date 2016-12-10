#include <stdio.h>
#include "parser.h"
#include "expgraph.h"
#include "expander.h"
#include "export.h"

namespace {

ExpGraph::Ref ExpandForBits(const Graph& graph, const Graph::Ref& main, ExpGraph& expgraph, double bits, double overshoot, size_t maxlen) {
    Expander exp(&graph, &expgraph);
    double goalbits = bits + log(1.0 + overshoot) / log(2.0);

    std::vector<ExpGraph::Ref> refs;
    BigNum total;
    for (size_t len = 0; len <= maxlen; len++) {
        auto r = exp.Expand(main, len);
        if (!r.first) {
            return ExpGraph::Ref();
        }
        if (!r.second.defined()) {
            continue;
        }
        total += r.second->count;
        refs.emplace_back(std::move(r.second));
        if (total.log2() >= goalbits) {
            size_t start = 0;
            while (start < refs.size()) {
                BigNum next = total;
                next -= refs[start]->count;
                if (next.log2() >= bits) {
                    total = std::move(next);
                    ++start;
                } else {
                    refs.erase(refs.begin(), refs.begin() + start);
                    fprintf(stderr, "Usings lengths %lu..%lu\n", (unsigned long)refs.front()->len, (unsigned long)refs.back()->len);
                    return expgraph.NewDisjunct(std::move(refs));
                }
            }
        }
    }

    return ExpGraph::Ref();
}

}

int main(int argc, char** argv) {
    char *buf = (char*)malloc(1048576);
    ssize_t len = fread(buf, 1, 1048576, stdin);
    int reslen = argc > 1 ? strtoul(argv[1], NULL, 10) : 16;

    if (len < 0) {
        fprintf(stderr, "Failed to read\n");
        return 1;
    }

    Graph graph;
    Graph::Ref main;
    std::string parse_error = Parse(graph, main, buf, len);
    if (!main.defined()) {
        fprintf(stderr, "Parse error: %s\n", parse_error.c_str());
        return 2;
    }

    free(buf);

    ExpGraph expgraph;
    ExpGraph::Ref emain = ExpandForBits(graph, main, expgraph, 500, 0.2, 1024);
    if (!emain.defined()) {
        fprintf(stderr, "Unable to expand graph\n");
        return 3;
    }
    Optimize(expgraph);

    fprintf(stderr, "%lu node model, %s combinations (%g bits)\n", (unsigned long)expgraph.nodes.size(), emain->count.hex().c_str(), emain->count.log2());

    Export(expgraph, emain, stdout);

    emain = ExpGraph::Ref();
    main = Graph::Ref();
    return 0;
}
