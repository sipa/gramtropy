#ifndef _GRAMTROPY_PARSER_H_
#define _GRAMTROPY_PARSER_H_

#include "graph.h"

std::string Parse(Graph& graph, Graph::Ref& mainout, const char* str, size_t len);

#endif
