#include "interpreter.h"
#include "import.h"
#include <stdio.h>

int main(int argc, char** argv) {
    FlatGraph graph;
    Import(graph, stdin);

    for (int i = 0; i < 100; i++) {
        std::string str = Generate(graph, &graph.nodes.back());
        printf("%s\n", str.c_str());
    }

    return 0;
}
