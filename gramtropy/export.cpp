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
    do {
        putc((n & 0x7F) | ((n > 0x7F) << 7), f);
        n >>= 7;
    } while (n);
}

}

/* c1 * s1 + c2 * (f1 + s2) + c3 * (f1 + f2 + s3) + c4 * (f1 + f2 + f3 + s4)
c1s1 + c2f1 + c2s2 + c3f1 + c3f2 + c3s3 + c4f1 + c4f2 + c4f3 + c4s4
c2f1 + c3f1 + c3f2 + c4f1 + c4f2 + c4f3
f1 * (c2 + c3 + c4) + f2 * (c3 + c4) + f3 * (c4)
- (f1 * c1 + f2 * (c1 + c2) + f3 * (c1 + c2 + c3)) */


void Export(ExpGraph& expgraph, const ExpGraph::Ref& ref, FILE* file) {
    int cnt = 0;
    std::map<const ExpGraph::Node*, NodeData> dump;
    for (const auto& node : expgraph.nodes) {
        auto it = dump.emplace(&node, cnt);
        NodeData& data = it.first->second;
//        fprintf(stderr, "Export node %i\n", cnt);
        if (node.nodetype == ExpGraph::Node::NodeType::DICT) {
            double cost = log2(node.dict.size());
//            fprintf(stderr, "* Dict size %u\n", (unsigned)node.dict.size());
            writenum(4 * node.dict.size(), stdout);
            writenum(node.dict[0].size(), stdout);
            for (size_t s = 0; s < node.dict.size(); s++) {
                std::string str = node.dict[s];
                fwrite(str.data(), str.size(), 1, stdout);
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
            writenum(4 * node.refs.size() + 1, stdout);
            std::sort(subs.begin(), subs.end());
            for (const auto& sub : subs) {
                auto it2 = dump.find(std::get<1>(sub));
                const NodeData& subdata = it2->second;
                fail += (success + subdata.fail) * fact;
                success += subdata.success;
                fact *= 0.1;
                writenum(std::get<2>(sub), stdout);
                writenum(cnt - subdata.number - 1, stdout);
//                fprintf(stderr, "  * node %i at pos %i (%g suc, %g fail)\n", subdata.number, std::get<2>(sub), subdata.success, subdata.fail);
            }
            data.success = 1.0 + success;
            data.fail = 1.0 + fail;
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
            writenum(4 * node.refs.size() + 2, stdout);
            std::sort(subs.begin(), subs.end());
            for (const auto& sub : subs) {
                auto it2 = dump.find(sub.second);
                const NodeData& subdata = it2->second;
                success += (fail + subdata.success) * (sub.second->count.get_d() / node.count.get_d());
                fail += subdata.fail;
                writenum(cnt - subdata.number - 1, stdout);
//                fprintf(stderr, "  * node %i (%g suc, %g fail)\n", subdata.number, subdata.success, subdata.fail);
            }
            data.success = 1.0 + success;
            data.fail = 1.0 + fail;
        }
//        fprintf(stderr, "* cost (%g suc, %g fail)\n", data.success, data.fail);
        if (ref && &*ref == &node) {
            break;
        }
        cnt++;
    }
    
}

