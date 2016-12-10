#include "import.h"
#include <assert.h>

namespace {

uint64_t readnum(FILE* f) {
    uint64_t ret = 0;
    uint8_t c;
    do {
        c = getc(f);
        ret = (ret << 7) | (c & 0x7F);
    } while (c & 0x80);
    return ret;
}

}

/* c1 * s1 + c2 * (f1 + s2) + c3 * (f1 + f2 + s3) + c4 * (f1 + f2 + f3 + s4)
c1s1 + c2f1 + c2s2 + c3f1 + c3f2 + c3s3 + c4f1 + c4f2 + c4f3 + c4s4
c2f1 + c3f1 + c3f2 + c4f1 + c4f2 + c4f3
f1 * (c2 + c3 + c4) + f2 * (c3 + c4) + f3 * (c4)
- (f1 * c1 + f2 * (c1 + c2) + f3 * (c1 + c2 + c3)) */

void Import(FlatGraph& graph, FILE* file) {
    while (!feof(file)) {
        uint64_t typ = readnum(file);
        switch (typ & 3) {
        case 0: {
            size_t count = typ >> 2;
            size_t len = readnum(file);
            std::vector<char> data;
            data.resize(count * len);
            for (size_t i = 0; i < count; i++) {
                size_t offset = 0;
                if (i > 0) {
                    offset = readnum(file);
                    memcpy(&data[i * len], &data[(i - 1) * len], offset);
                }
                fread(&data[i * len + offset], len - offset, 1, file);
            }
            std::vector<FlatNode>::iterator node = graph.nodes.emplace(graph.nodes.end(), FlatNode::NodeType::DICT, len);
            node->dict = graph.dicts.size();
            graph.dicts.emplace_back(std::move(data), len);
            node->count = count;
            break;
        }
        case 1: {
            size_t num = 2 + (typ >> 2);
            BigNum count = 1;
            int len = 0;
            std::vector<std::pair<size_t, size_t>> refs;
            refs.reserve(num);
            for (size_t i = 0; i < num; i++) {
                size_t pos = readnum(file);
                size_t idx = graph.nodes.size() - readnum(file);
                assert(idx < graph.nodes.size());
                refs.emplace_back(pos, idx);
                count *= graph.nodes[idx].count;
                len += graph.nodes[idx].len;
            }
            std::vector<FlatNode>::iterator node = graph.nodes.emplace(graph.nodes.end(), FlatNode::NodeType::CONCAT, len);
            node->refs = std::move(refs);
            node->count = std::move(count);
            break;
        }
        case 2: {
            size_t num = 2 + (typ >> 2);
            BigNum count = 0;
            int len;
            std::vector<std::pair<size_t, size_t>> refs;
            refs.reserve(num);
            for (size_t i = 0; i < num; i++) {
                size_t idx = graph.nodes.size() - readnum(file);
                assert(idx < graph.nodes.size());
                refs.emplace_back(0, idx);
                count += graph.nodes[idx].count;
                if (i == 0) {
                    len = graph.nodes[idx].len;
                } else if (len != graph.nodes[idx].len) {
                    len = -1;
                }
            }
            std::vector<FlatNode>::iterator node = graph.nodes.emplace(graph.nodes.end(), FlatNode::NodeType::DISJUNCT, len);
            node->refs = std::move(refs);
            node->count = std::move(count);
            break;
        }
        }
    }
}
