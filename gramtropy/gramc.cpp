#include <stdio.h>
#include "parser.h"
#include "expgraph.h"
#include "expander.h"
#include "export.h"
#include <unistd.h>

namespace {

ExpGraph::Ref ExpandForBits(const Graph& graph, const Graph::Ref& main, ExpGraph& expgraph, double minbits, double overshoot, size_t minlen, size_t maxlen, size_t maxnodes, size_t maxthunks) {
    Expander exp(&graph, &expgraph, maxnodes, maxthunks);
    double goalbits = minbits + log1p(overshoot) / log(2.0);

    std::vector<ExpGraph::Ref> refs;
    BigNum total;
    for (size_t len = minlen; len <= maxlen; len++) {
        auto r = exp.Expand(main, len);
        if (r.second.size() > 0) {
            fprintf(stderr, "Expansion failure: %s\n", r.second.c_str());
            return ExpGraph::Ref();
        }
        if (!r.first) {
            continue;
        }
        total += r.first->count;
        refs.emplace_back(std::move(r.first));
        if (total.log2() >= goalbits) {
            size_t start = 0;
            while (start < refs.size()) {
                BigNum next = total;
                next -= refs[start]->count;
                if (next.log2() >= minbits) {
                    total = std::move(next);
                    ++start;
                } else {
                    refs.erase(refs.begin(), refs.begin() + start);
                    printf("Using length range %lu..%lu\n", (unsigned long)refs.front()->len, (unsigned long)refs.back()->len);
                    return expgraph.NewDisjunct(std::move(refs));
                }
            }
        }
    }

    fprintf(stderr, "No solution with enough entropy in range\n");
    return ExpGraph::Ref();
}

bool WriteFile(const char *file, ExpGraph& expgraph, const ExpGraph::Ref& emain) {
    FILE* fp = fopen(file, "w");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", file);
        return false;
    }
    Export(expgraph, emain, fp);
    fclose(fp);
    return true;
}

Graph::Ref ParseFile(const char *file, Graph& graph) {
    FILE* fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", file);
        return Graph::Ref();
    }
    std::vector<char> data;
    ssize_t tlen = 0;
    while (true) {
        data.resize(tlen + 65536);
        ssize_t len = fread(data.data() + tlen, 1, 65536, fp);
        if (len < 0) {
            fprintf(stderr, "Unable to read from file '%s'\n", file);
            return Graph::Ref();
        }
        if (len == 0) {
            data.resize(tlen);
            break;
        }
        tlen += len;
    }
    fclose(fp);

    Graph::Ref main;
    std::string parse_error = Parse(graph, main, data.data(), tlen);
    if (!main.defined()) {
        fprintf(stderr, "Parse error: %s\n", parse_error.c_str());
        return Graph::Ref();
    }
    return main;
}

}

int main(int argc, char** argv) {
    size_t minlen = 0;
    size_t maxlen = 1024;
    size_t maxnodes = 1000000;
    size_t maxthunks = 250000;
    double overshoot = 0.2;
    double bits = 64;
    const char* infile = nullptr;
    const char* outfile = nullptr;
    bool invalid_usage = false;
    bool help = false;

    int opt;
    while ((opt = getopt(argc, argv, "b:l:u:N:T:O:h")) != -1) {
        switch (opt) {
        case 'b':
            bits = strtod(optarg, nullptr);
            break;
        case 'l':
            minlen = strtoul(optarg, nullptr, 10);
            break;
        case 'u':
            maxlen = strtoul(optarg, nullptr, 10);
            break;
        case 'N':
            maxnodes = strtoul(optarg, nullptr, 10);
            break;
        case 'T':
            maxthunks = strtoul(optarg, nullptr, 10);
            break;
        case 'O':
            overshoot = strtod(optarg, nullptr);
            break;
        case 'h':
            help = true;
        }
    }

    if (bits <= 0 || bits > 65536) {
        fprintf(stderr, "Bits out of range (0.0-65536.0)\n");
        invalid_usage = true;
    }

    if (minlen > 65536) {
        fprintf(stderr, "Minimum length out of range (0.0-65536.0)\n");
        invalid_usage = true;
    }

    if (maxlen < minlen || maxlen > 65536) {
        fprintf(stderr, "Maximum length out of range (minimum length-65536)\n");
        invalid_usage = true;
    }

    if (maxnodes < 10 || maxnodes > 1000000000) {
        fprintf(stderr, "Maximum nodes out of range (10-1000000000)\n");
        invalid_usage = true;
    }

    if (maxthunks < 10 || maxthunks > 1000000000) {
        fprintf(stderr, "Maximum thunks out of range (10-1000000000)\n");
        invalid_usage = true;
    }

    if (overshoot < 0 || overshoot > 1) {
        fprintf(stderr, "Overshoot out of range (0.0-1.0)\n");
        invalid_usage = true;
    }

    if (optind + 1 > argc) {
        fprintf(stderr, "Expected input filename\n");
        invalid_usage = true;
    }

    if (optind + 2 > argc) {
        fprintf(stderr, "Expected output filename\n");
        invalid_usage = true;
    }

    infile = argv[optind];
    outfile = argv[optind + 1];

    if (strcmp(infile, outfile) == 0) {
        fprintf(stderr, "Refusing to overwrite input file\n");
        invalid_usage = true;
    }

    if (invalid_usage || help) {
        fprintf(stderr, "Usage: %s [options...] infile outfile\n", *argv);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -b bits: use a range with at least bits bits of entropy (default: 64.0)\n");
        fprintf(stderr, "  -l minlen: generate phrases of at least minlen characters (default: 0)\n");
        fprintf(stderr, "  -u maxlen: generate phrases of at most maxlen characters (default: 1024)\n");
        fprintf(stderr, "  -N maxnodes, -T maxthunks, -O overshoot: miscelleanous tweaks\n");
        if (invalid_usage) {
            return -1;
        }
        return 0;
    }


    Graph graph;
    Graph::Ref main = ParseFile(infile, graph);
    if (!main) {
        return 1;
    }

    ExpGraph expgraph;
    ExpGraph::Ref emain = ExpandForBits(graph, main, expgraph, bits, overshoot, minlen, maxlen, maxnodes, maxthunks);
    if (!emain.defined()) {
        return 2;
    }
    main = Graph::Ref();

    Optimize(expgraph);

    printf("Result: %s combinations (%g bits)\n", emain->count.hex().c_str(), emain->count.log2());

    WriteFile(outfile, expgraph, emain);

    emain = ExpGraph::Ref();
    return 0;
}
