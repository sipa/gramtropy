#include "export.h"
#include <algorithm>
#include <math.h>

namespace {

struct NodeData {
    int number;
    double success;
    double fail;

    NodeData(int num_) : number(num_), success(0), fail(0) {}
};

void writenum(uint64_t n, FILE* f) {
//    fprintf(stderr, "Write num %lx\n", (unsigned long)n);
    int exts = 0;
    uint64_t nc = n;
    while (nc >> 7) {
        ++exts;
        nc >>= 7;
    }
    while (exts) {
        putc(0x80 | ((n >> (7 * exts)) & 0x7F), f);
        --exts;
    }
    putc(n & 0x7F, f);
}

}

/* c1 * s1 + c2 * (f1 + s2) + c3 * (f1 + f2 + s3) + c4 * (f1 + f2 + f3 + s4)
c1s1 + c2f1 + c2s2 + c3f1 + c3f2 + c3s3 + c4f1 + c4f2 + c4f3 + c4s4
c2f1 + c3f1 + c3f2 + c4f1 + c4f2 + c4f3
f1 * (c2 + c3 + c4) + f2 * (c3 + c4) + f3 * (c4)
- (f1 * c1 + f2 * (c1 + c2) + f3 * (c1 + c2 + c3)) */


void Export(ExpGraph& expgraph, const ExpGraph::Ref& ref, FILE* file) {
    int cnt = 0;
    BigNum big = 1;
    double small = 1.0;
    for (int i = 0; i < 3; i++) {
        big *= 1000000000;
        small *= 0.000000001;
    }
    std::map<const ExpGraph::Node*, NodeData> dump;
    for (const auto& node : expgraph.nodes) {
        auto it = dump.emplace(&node, cnt);
        NodeData& data = it.first->second;
//        fprintf(stderr, "Export node %i (%s combinations)\n", cnt, node.count.hex().str_c());
        if (node.nodetype == ExpGraph::Node::NodeType::DICT) {
            double cost = log2(node.dict.size());
//            fprintf(stderr, "* Dict size %u\n", (unsigned)node.dict.size());
            writenum(4 * node.dict.size() - 3, file);
            writenum(node.len, file);
            const std::string* prev = nullptr;
            for (const auto& str : node.dict) {
                int offset = 0;
                if (prev != nullptr) {
                    while (offset < node.len && str[offset] == (*prev)[offset]) {
                        ++offset;
                    }
                    writenum(offset, file);
                }
                fwrite(str.data() + offset, str.size() - offset, 1, file);
                prev = &str;
            }
            data.success = cost + 1.0;
            data.fail = cost + 2.0;
        } else if (node.nodetype == ExpGraph::Node::NodeType::CONCAT) {
//            fprintf(stderr, "* Cat of %i\n", (int)node.refs.size());
            size_t pos = 0;
            std::vector<std::tuple<double, const ExpGraph::Node*, int>> subs;
            for (size_t s = 0; s < node.refs.size(); s++) {
                auto it2 = dump.find(&*node.refs[s]);
                const NodeData& subdata = it2->second;
                subs.emplace_back(subdata.fail, &*node.refs[s], pos);
                assert(node.refs[s]->len >= 0);
                pos += node.refs[s]->len;
            }
            double success = 0;
            double fail = 0;
            double fact = 1.0;
            writenum(4 * node.refs.size() - 6, file);
            std::sort(subs.begin(), subs.end());
            for (const auto& sub : subs) {
                auto it2 = dump.find(std::get<1>(sub));
                const NodeData& subdata = it2->second;
                fail += (success + subdata.fail) * fact;
                success += subdata.success;
                fact *= 0.1;
                writenum(std::get<2>(sub), file);
                writenum(cnt - subdata.number - 1, file);
//                fprintf(stderr, "  * node %i at pos %i\n", subdata.number, std::get<2>(sub));
            }
            data.success = 1.0 + success;
            data.fail = 1.0 + fail;
//            fprintf(stderr, "  * Total: %s combinations\n", node.count.hex().c_str());
        } else {
//            fprintf(stderr, "* Disjunct of %i\n", (int)node.refs.size());
            std::vector<std::pair<double, const ExpGraph::Node*>> subs;
            for (size_t s = 0; s < node.refs.size(); s++) {
                auto it2 = dump.find(&*node.refs[s]);
                const NodeData& subdata = it2->second;
                subs.emplace_back(subdata.fail / node.refs[s]->count.get_d(), &*node.refs[s]);
            }
            double success = 0;
            double fail = 0;
            writenum(4 * node.refs.size() - 5, file);
            if (ref->len != -1) { // Don't reorder multilength disjunctions (no need, as they're fast regardless).
                std::sort(subs.begin(), subs.end());
            }
            for (const auto& sub : subs) {
                auto it2 = dump.find(sub.second);
                const NodeData& subdata = it2->second;
                BigNum x = sub.second->count * big;
                BigNum ratio = x.divmod(node.count);
                success += (fail + subdata.success) * (ratio.get_d() * small);
                fail += subdata.fail;
                writenum(cnt - subdata.number - 1, file);
//                fprintf(stderr, "  * node %i (%g suc, %g fail)\n", subdata.number, subdata.success, subdata.fail);
            }
            data.success = 1.0 + success;
            data.fail = 1.0 + fail;
//            fprintf(stderr, "  * Total: %s combinations\n", node.count.hex().c_str());
        }
//        fprintf(stderr, "* cost (%g suc, %g fail)\n", data.success, data.fail);
        if (ref && &*ref == &node) {
            break;
        }
        cnt++;
    }
    writenum(0, file);
}

