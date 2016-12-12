#include "interpreter.h"
#include "import.h"
#include <stdio.h>
#include <unistd.h>

namespace {

bool RandomInteger(const BigNum& range, BigNum& out) {
    int bits = range.bits();
    std::vector<uint8_t> data;
    data.resize((bits + 7)/8);
    FILE* rng = fopen("/dev/urandom", "r");
    do {
        size_t r = fread(&data[0], data.size(), 1, rng);
        if (r != 1) {
            fprintf(stderr, "Unable to read from RNG");
            return false;
        }
        if (bits % 8) {
            data[0] >>= (8 - (bits % 8));
        }
        out = BigNum(&data[0], data.size());
    } while (out >= range);
    fclose(rng);
    return true;
}

bool Generate(const FlatGraph& graph, const FlatNode* ref) {
    BigNum num;
    if (!RandomInteger(ref->count, num)) {
        return false;
    }
    printf("%s\n", Generate(graph, ref, std::move(num)).c_str());
    return true;
}

bool ParseFile(const char *file, FlatGraph& graph) {
    FILE* fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", file);
        return false;
    }
    Import(graph, fp);
    fclose(fp);
    return true;
}

}

int main(int argc, char** argv) {
    bool help = false;
    bool encode = false;
    int generate = 1;
    int opt;
    const char* str = nullptr;
    while ((opt = getopt(argc, argv, "d:e:g:h")) != -1) {
        switch (opt) {
        case 'd':
            str = optarg;
            encode = false;
            generate = 0;
            break;
        case 'e':
            str = optarg;
            encode = true;
            generate = 0;
            break;
        case 'g':
            encode = false;
            generate = strtoul(optarg, NULL, 10);
            break;
        case 'h':
            help = true;
            break;
        }
    }

    if (help || optind + 1 > argc || (generate == 0 && str == nullptr)) {
        fprintf(stderr, "Usage: %s [-g n] file      generate n phrases (default 1)\n", *argv);
        fprintf(stderr, "       %s -e hexnum file   generate phrase number hexnum\n", *argv);
        fprintf(stderr, "       %s -d str file:     decode phrase string str to hex\n", *argv);
        return 1;
    }

    FlatGraph graph;
    if (!ParseFile(argv[optind], graph)) {
        return 2;
    }

    if (generate) {
        while (generate--) {
            if (!Generate(graph, &graph.nodes.back())) {
                return 3;
            }
        }
    } else if (encode) {
        BigNum num;
        if (!num.set_hex(str)) {
            fprintf(stderr, "Cannot parse hex number '%s'\n", str);
            return 4;
        }
        fprintf(stderr, "Encoding number %s\n", num.hex().c_str());
        if (num >= graph.nodes.back().count) {
            fprintf(stderr, "Number out of range (max %s)\n", graph.nodes.back().count.hex().c_str());
            return 5;
        }
        printf("%s\n", Generate(graph, &graph.nodes.back(), std::move(num)).c_str());
    } else {
        BigNum num;
        if (!Parse(graph, &graph.nodes.back(), str, num)) {
            printf("-1\n");
        } else {
            printf("%s\n", num.hex().c_str());
        }
    }

    return 0;
}
