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

enum RunMode {
    MODE_GENERATE,
    MODE_ENCODE,
    MODE_DECODE,
    MODE_ENCODE_STREAM,
    MODE_DECODE_STREAM,
    MODE_INFO,
    MODE_ITERATE,
    MODE_HELP,
};

}

int main(int argc, char** argv) {
    RunMode mode = MODE_GENERATE;
    int generate = 1;
    int opt;
    const char* str = nullptr;
    while ((opt = getopt(argc, argv, "iaDEd:e:g:h")) != -1) {
        switch (opt) {
        case 'i':
            mode = MODE_INFO;
            break;
        case 'a':
            mode = MODE_ITERATE;
            break;
        case 'D':
            mode = MODE_DECODE_STREAM;
            break;
        case 'E':
            mode = MODE_ENCODE_STREAM;
            break;
        case 'd':
            mode = MODE_DECODE;
            str = optarg;
            break;
        case 'e':
            mode = MODE_ENCODE;
            str = optarg;
            break;
        case 'g':
            mode = MODE_GENERATE;
            generate = strtoul(optarg, NULL, 10);
            break;
        case 'h':
            mode = MODE_HELP;
            break;
        }
    }

    if (mode == MODE_HELP || optind + 1 > argc) {
        fprintf(stderr, "Usage: %s [-g n] file      Generate n random phrases (default 1)\n", *argv);
        fprintf(stderr, "       %s -e hexnum file   Encode hexadecimal into phrase\n", *argv);
        fprintf(stderr, "       %s -d str file      Decode phrase into hexadecimal \n", *argv);
        fprintf(stderr, "       %s -E file          Encode hexadecimals read from stdin\n", *argv);
        fprintf(stderr, "       %s -D file          Decode phrases read from stdin\n", *argv);
        fprintf(stderr, "       %s -i file          Show information about file\n", *argv);
        fprintf(stderr, "       %s -a file          Generate all phrases from file, in order\n", *argv);
        return mode != MODE_HELP;
    }

    FlatGraph graph;
    if (!ParseFile(argv[optind], graph)) {
        return 2;
    }
    const FlatNode* main = &graph.nodes.back();

    switch (mode) {
    case MODE_GENERATE:
        while (generate--) {
            if (!Generate(graph, main)) {
                return 3;
            }
        }
        break;
    case MODE_ITERATE:
    {
        BigNum num;
        do {
            printf("%s\n", Generate(graph, main, BigNum(num)).c_str());
            num += 1;
        } while(true);
    }
    case MODE_ENCODE:
    {
        BigNum num;
        if (!num.set_hex(str)) {
            fprintf(stderr, "Cannot parse hex number '%s'\n", str);
            return 4;
        }
        if (num >= main->count) {
            fprintf(stderr, "Number out of range (max %s)\n", main->count.hex().c_str());
            return 5;
        }
        printf("%s\n", Generate(graph, main, std::move(num)).c_str());
        break;
    }
    case MODE_DECODE:
    {
        BigNum num;
        if (!Parse(graph, main, str, num)) {
            printf("-1\n");
        } else {
            printf("%s\n", num.hex().c_str());
        }
        break;
    }
    case MODE_INFO:
    {
        printf("Combinations: %s\n", main->count.hex().c_str());
        printf("Bits: %g\n", main->count.log2());
        printf("Nodes: %lu\n", (unsigned long)graph.nodes.size());
        break;
    }
    case MODE_ENCODE_STREAM:
    {
        char buf[8192];
        while (fgets(buf, sizeof(buf), stdin) != nullptr) {
            char* ptr = strchr(buf, '\n');
            if (ptr) {
                *ptr = 0;
            }
            BigNum num;
            if (!num.set_hex(buf)) {
                fprintf(stderr, "Cannot parse hex number '%s'\n", buf);
                return 4;
            }
            if (num >= main->count) {
                fprintf(stderr, "Number %s out of range (max %s)\n", num.hex().c_str(), main->count.hex().c_str());
                return 5;
            }
            printf("%s\n", Generate(graph, main, std::move(num)).c_str());
        }
        break;
    }
    case MODE_DECODE_STREAM:
    {
        char buf[8192];
        while (fgets(buf, sizeof(buf), stdin) != nullptr) {
            char* ptr = strchr(buf, '\n');
            if (ptr) {
                *ptr = 0;
            }
            BigNum num;
            if (!Parse(graph, main, buf, num)) {
                printf("-1\n");
            } else {
                printf("%s\n", num.hex().c_str());
            }
        }
        break;
    }
    default:
        break;
    }

    return 0;
}
