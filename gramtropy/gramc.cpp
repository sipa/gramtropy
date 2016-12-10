#include <stdio.h>
#include "parser.h"
#include "expgraph.h"
#include "expander.h"
#include "export.h"

int main(int argc, char** argv) {
    char *buf = (char*)malloc(1048576);
    ssize_t len = fread(buf, 1, 1048576, stdin);
    int reslen = argc > 1 ? strtoul(argv[1], NULL, 10) : 16;

    if (len < 0) {
        fprintf(stderr, "Failed to read\n");
        return -1;
    }

    Graph graph;
    Graph::Ref main;
    if (!Parse(graph, main, buf, len)) {
        return -1;
    }

    free(buf);

    // std::string desc = Describe(graph, main);
    // printf("After optimize:\n");
    // printf("%s\n", desc.c_str());


    ExpGraph::Ref emain;
    ExpGraph expgraph;
    {
        Expander exp(&graph, &expgraph);
        emain = exp.Expand(main, reslen);
        if (!emain.defined()) {
            fprintf(stderr, "Error: infinite recursion\n");
            return -1;
        }
    }

    Optimize(expgraph);

    fprintf(stderr, "%lu node model, %s combinations (%g bits)\n", (unsigned long)expgraph.nodes.size(), emain->count.hex().c_str(), emain->count.log2());

    Export(expgraph, emain, stdout);

    emain = ExpGraph::Ref();
    main = Graph::Ref();
    return 0;
}
