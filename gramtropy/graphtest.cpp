#include <stdio.h>

#include "graph.h"

int main(void) {
    Graph graph;
    Graph::Ref e = graph.NewEmpty();
    Graph::Ref s1 = graph.NewDisjunct(graph.NewString("a"), std::move(e));
    Graph::Ref s2 = graph.NewDisjunct(graph.NewString("b"), std::move(s1));
    Graph::Ref s3 = graph.NewDisjunct(graph.NewString("c"), std::move(s2));
    printf("%s", graph.Describe(s3).c_str());
    graph.Optimize();
    printf("%s", graph.Describe(s3).c_str());
    return 0;
}
