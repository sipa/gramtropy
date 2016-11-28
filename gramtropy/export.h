#ifndef _GRAMTROPY_EXPORT_H_
#define _GRAMTROPY_EXPORT_H_

#include <stdio.h>

#include "expgraph.h"

void Export(ExpGraph& expgraph, const ExpGraph::Ref& ref, FILE* file);

#endif
