#include <assert.h>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <memory>
#include <math.h>

#include "gramtropy/bignum.h"
#include "gramtropy/stringvector.h"

struct ExpansionInfo {
    BigNum combinations;
};

struct FlattenedExpansionInfo : public ExpansionInfo {
    StringVector dict;
};

class NodeBase;

struct ExpansionRef {
    const NodeBase* node;
    const uint32_t cost;

    ExpansionRef(const NodeBase* nod, uint32_t cos) : node(nod), cost(cos) {}

    bool friend operator<(const ExpansionRef& r1, const ExpansionRef& r2) {
        if (r1.cost < r2.cost) return true;
        if (r1.cost > r2.cost) return false;
        return (r1.node < r2.node);
    }
};

struct ExpansionState {
    std::map<ExpansionRef, ExpansionInfo> disjunctions;
    std::map<ExpansionRef, ExpansionInfo> concatenations;
    std::map<ExpansionRef, FlattenedExpansionInfo> deduplications;
    std::set<ExpansionRef> unique_verified;
    std::set<ExpansionRef> printed;
    std::set<ExpansionRef> in_flight;
};

bool Deduplicate(ExpansionState& state, const NodeBase* node, uint32_t cost, const BigNum* combinations, std::set<std::vector<char> >& s, std::vector<char>* duplicate);

static const BigNum zero;

class NodeBase {
private:
    const std::string description;

protected:
    virtual const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const =0;
    virtual void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const =0;

public:
    std::string Describe(const ExpansionRef& ref) const {
        std::stringstream ss;
        ss << description << " (cost " << ref.cost << ")";
        return ss.str();
    }

    std::string Describe() const {
        return description;
    }

    const BigNum* Combinations(ExpansionState& state, uint32_t cost) const {
        ExpansionRef ref(this, cost);
        if (state.in_flight.count(ref)) {
            return NULL;
        }
        state.in_flight.insert(ref);
        const BigNum* ret = ComputeCombinations(state, ref);
        state.in_flight.erase(ref);
        return ret;
    }

    void Expand(int level, ExpansionState& state, uint32_t cost, const BigNum& num, std::vector<char>& ret) const {
        ExpansionRef ref(this, cost);
        const BigNum* comb = ComputeCombinations(state, ref);
        if (comb == NULL || num >= *comb) {
            throw std::runtime_error("Impossible combination requested for " + Describe(ref));
        }
        if (level >= 0) {
            for (int i = 0; i < level; i++) {
                std::cerr << "  ";
            };
            std::cerr << "* " << Describe(ref) << "[" << num.hex() << "]\n";
        }
        ComputeExpansion(level >= 0 ? level + 1 : level, state, ref, num, ret);
    }

    NodeBase(const std::string& str) : description(str) {}
    virtual ~NodeBase() {}
};

bool Deduplicate(int level, ExpansionState& state, const NodeBase* node, uint32_t cost, const BigNum& combinations, std::set<std::vector<char> >& s, std::vector<char>* duplicate) {
    BigNum num;
    bool ret = false;
    while (num < combinations) {
        std::vector<char> accum;
        BigNum num_mutable = num;
        node->Expand(level, state, cost, num_mutable, accum);
        if (s.count(accum)) {
            if (duplicate) {
                *duplicate = std::move(accum);
            }
            ret = true;
        } else {
            s.insert(std::move(accum));
        }
        num++;
    }
    return ret;
}

class NodeDictionary : public NodeBase {
private:
    std::map<uint32_t, StringVector> dicts;
    std::map<uint32_t, BigNum> counts;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        if (counts.count(ref.cost)) {
            return &counts.at(ref.cost);
        } else {
            return &zero;
        }
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        ret.insert(ret.end(), dicts.at(ref.cost).StringBegin(num.get_ui()), dicts.at(ref.cost).StringEnd(num.get_ui()));
    }

public:
    NodeDictionary(const std::string& desc, const std::vector<std::string>& strings) : NodeBase(desc) {
        for (const std::string& str : strings) {
            dicts[str.size()].Append(str);
            counts[str.size()]++;
        }
    }
};

class NodeDisjunction : public NodeBase {
private:
    const NodeBase* a;
    const NodeBase* b;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        if (state.disjunctions.count(ref)) {
            return &state.disjunctions[ref].combinations;
        }

        const BigNum* combinations_a = a->Combinations(state, ref.cost);
        const BigNum* combinations_b = b->Combinations(state, ref.cost);
        if (combinations_a == NULL || combinations_b == NULL) {
            return NULL;
        }
        BigNum& combinations = state.disjunctions[ref].combinations;
        combinations = *combinations_a;
        combinations += *combinations_b;
        return &combinations;
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        const BigNum* combinations_a = a->Combinations(state, ref.cost);
        if (num < *combinations_a) {
            a->Expand(level, state, ref.cost, num, ret);
            return;
        }
        BigNum n = num;
        n -= *combinations_a;
        b->Expand(level, state, ref.cost, n, ret);
    }

public:
    NodeDisjunction(const std::string& desc, const NodeBase* node_a, const NodeBase* node_b) : NodeBase(desc), a(node_a), b(node_b) {}
};

class NodeConcatenation : public NodeBase {
private:
    const NodeBase* left;
    const NodeBase* right;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        if (state.concatenations.count(ref)) {
            return &state.concatenations[ref].combinations;
        }

        BigNum combinations;

        for (uint32_t left_cost = 0; left_cost <= ref.cost; left_cost++) {
            uint32_t right_cost = ref.cost - left_cost;
            const BigNum* left_combinations = left->Combinations(state, left_cost);
            const BigNum* right_combinations = right->Combinations(state, right_cost);
            if (left_combinations != NULL && left_combinations->is_zero()) {
                continue;
            }
            if (right_combinations != NULL && right_combinations->is_zero()) {
                continue;
            }
            if (left_combinations == NULL || right_combinations == NULL) {
                return NULL;
            }
            combinations += (*left_combinations) * (*right_combinations);
        }

        return &(state.concatenations[ref].combinations = combinations);
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        BigNum num_copy = num;
        BigNum mult_combinations;
        for (uint32_t left_cost = 0; left_cost <= ref.cost; left_cost++) {
            uint32_t right_cost = ref.cost - left_cost;
            const BigNum* left_combinations = left->Combinations(state, left_cost);
            const BigNum* right_combinations = right->Combinations(state, right_cost);
            if (left_combinations != NULL && left_combinations->is_zero()) {
                continue;
            }
            if (right_combinations != NULL && right_combinations->is_zero()) {
                continue;
            }
            mult_combinations = (*left_combinations) * (*right_combinations);
            if (num_copy < mult_combinations) {
                BigNum other = num_copy.divmod(*left_combinations);
                left->Expand(level, state, left_cost, num_copy, ret);
                right->Expand(level, state, right_cost, other, ret);
                return;
            } else {
                num_copy -= mult_combinations;
            }
        }
        throw std::runtime_error("Expansion beyond concatenation combinations for " + Describe(ref));
    }

public:
    NodeConcatenation(const std::string& str, const NodeBase* node_left, const NodeBase* node_right) : NodeBase(str), left(node_left), right(node_right) {}
};


class NodeReference : public NodeBase {
private:
    const NodeBase* reference;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        if (reference == NULL) {
            throw std::runtime_error("Evaluating undefined reference for" + Describe(ref));
        }
        return reference->Combinations(state, ref.cost);
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        reference->Expand(level, state, ref.cost, num, ret);
    }

public:
    void Update(const NodeBase* ref) {
        reference = ref;
    }
    const NodeBase* Dereference() const {
        return reference;
    }

    NodeReference(const std::string& desc) : NodeBase(desc), reference(NULL) {}
};

class NodeDeduplication : public NodeBase {
private:
    const NodeBase* reference;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        if (state.deduplications.count(ref)) {
            return &state.deduplications[ref].combinations;
        }

        std::set<std::vector<char > > s;
        const BigNum* comb = reference->Combinations(state, ref.cost);
        if (comb == NULL) {
            return NULL;
        }
        if (*comb > 1000000) {
            throw std::runtime_error("Deduplication of " + comb->hex() + " combinations requested for " + Describe(ref));
        }
        Deduplicate(-1, state, reference, ref.cost, *comb, s, NULL);
        StringVector& strv = state.deduplications[ref].dict;

        for (const std::vector<char>& str : s) {
             strv.Append(str);
        }
        return &(state.deduplications[ref].combinations = strv.size());
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        const StringVector& strv = state.deduplications[ref].dict;
        ret.insert(ret.end(), strv.StringBegin(num.get_ui()), strv.StringEnd(num.get_ui()));
    }

public:
    NodeDeduplication(const std::string& str, const NodeBase* ref) : NodeBase(str), reference(ref) {}
};

class NodeVerifyUnique : public NodeBase {
private:
    const NodeBase* reference;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        const BigNum* comb = reference->Combinations(state, ref.cost);
        if (comb == NULL) {
            return NULL;
        }
        if (state.unique_verified.count(ref) == 0) {
            std::set<std::vector<char> > s;
            std::vector<char> ret;
            if (*comb < 1000 && Deduplicate(-1, state, reference, ref.cost, *comb, s, &ret)) {
                throw std::runtime_error("Ambiguous expansion for " + Describe(ref) + ": " + std::string(ret.begin(), ret.end()));
            }
            state.unique_verified.insert(ref);
        }
        return comb;
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        reference->Expand(level, state, ref.cost, num, ret);
    }

public:
    NodeVerifyUnique(const std::string& str, const NodeBase* ref) : NodeBase(str), reference(ref) {}
};

class NodePrint : public NodeBase {
private:
    const NodeBase* reference;

protected:
    const BigNum* ComputeCombinations(ExpansionState& state, const ExpansionRef& ref) const override {
        const BigNum* comb = reference->Combinations(state, ref.cost);
        if (comb == NULL) {
            return NULL;
        }
        if (state.printed.count(ref) == 0) {
            std::set<std::vector<char> > s;
            if (*comb < 10000) {
                Deduplicate(-1, state, reference, ref.cost, *comb, s, NULL);
                std::cout << "Dump for " << Describe(ref) << ": ";
                for (const std::vector<char>& ss : s) {
                    std::cout << std::string(ss.begin(), ss.end()) << " ";
                }
                std::cout << "\n";
            }
            state.printed.insert(ref);
        }
        return comb;
    }

    void ComputeExpansion(int level, ExpansionState& state, const ExpansionRef& ref, const BigNum& num, std::vector<char>& ret) const override {
        reference->Expand(level, state, ref.cost, num, ret);
    }

public:
    NodePrint(const std::string& str, const NodeBase* ref) : NodeBase(str), reference(ref) {}
};

class Grammar {
    std::vector<std::unique_ptr<NodeBase> > nodes;
    const NodeBase* terminal;

public:
    NodeReference* NewReference(const std::string& str) {
        NodeReference* ret = new NodeReference(str);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodeDisjunction* NewDisjunction(const std::string& str, const NodeBase* a, const NodeBase* b) {
        NodeDisjunction* ret = new NodeDisjunction(str, a, b);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodeConcatenation* NewConcatenation(const std::string& str, const NodeBase* left, const NodeBase* right) {
        NodeConcatenation* ret = new NodeConcatenation(str, left, right);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodeDeduplication* NewDeduplication(const std::string& str, const NodeBase* ref) {
        NodeDeduplication* ret = new NodeDeduplication(str, ref);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodeVerifyUnique* NewVerifyUnique(const std::string& str, const NodeBase* ref) {
        NodeVerifyUnique* ret = new NodeVerifyUnique(str, ref);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodePrint* NewPrint(const std::string& str, const NodeBase* ref) {
        NodePrint* ret = new NodePrint(str, ref);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    NodeDictionary* NewDictionary(const std::string& str, const std::vector<std::string>& vstr) {
        NodeDictionary* ret = new NodeDictionary(str, vstr);
        nodes.push_back(std::unique_ptr<NodeBase>(ret));
        return ret;
    }

    void SetTerminal(const NodeBase* ref) {
        terminal = ref;
    }

    const NodeBase* GetTerminal() const {
        return terminal;
    }
};

class GrammarBuilder {
    Grammar* grammar;
    std::map<std::string, NodeReference*> symbols;

    NodeReference* Symbol(const std::string& name) {
        std::map<std::string, NodeReference*>::iterator it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        throw std::runtime_error("Undefined symbol: " + name);
    }

public:
    GrammarBuilder(Grammar* gram) : grammar(gram) {}

    void Define(const std::string& name) {
        std::map<std::string, NodeReference*>::iterator it = symbols.find(name);
        if (it != symbols.end()) {
            throw std::runtime_error("Duplicate symbol definition: " + name);
        }
        symbols[name] = grammar->NewReference("Symbol " + name);
    }

    void OrDictionary(const std::string& name, const std::vector<std::string>& dict) {
        NodeReference* symbol = Symbol(name);
        NodeDictionary* ndict = grammar->NewDictionary("Dictionary for " + name, dict);
        if (symbol->Dereference() == NULL) {
            symbol->Update(ndict);
        } else {
            symbol->Update(grammar->NewDisjunction("Dictionary expansion for " + name, symbol->Dereference(), ndict));
        }
    }

    void OrConcatenation(const std::string& name, const std::vector<std::string>& list) {
        NodeReference* symbol = Symbol(name);
        std::vector<NodeBase*> lists;
        lists.reserve(list.size());
        for (const std::string& entry : list) {
            lists.push_back(Symbol(entry));
        }
        while (lists.size() > 1) {
            std::vector<NodeBase*> lists2;
            for (size_t x = 0; x * 2 + 1 < lists.size(); x++) {
                lists2.push_back(grammar->NewConcatenation("Inner concatenation for " + name, lists[x * 2], lists[x * 2 + 1]));
            }
            if (lists.size() & 1) {
                lists2.push_back(lists.back());
            }
            lists.swap(lists2);
        }
        if (symbol->Dereference() == NULL) {
            symbol->Update(lists[0]);
        } else {
            symbol->Update(grammar->NewDisjunction("Expansion for " + name, symbol->Dereference(), lists[0]));
        }
    }

    void Deduplicate(const std::string& name) {
        NodeReference* symbol = Symbol(name);
        if (symbol->Dereference() == NULL) {
            throw std::runtime_error("Deduplicating empty symbol: " + name);
        }
        symbol->Update(grammar->NewDeduplication("Deduplication for " + name, symbol->Dereference()));
    }

    void VerifyUnique(const std::string& name) {
        NodeReference* symbol = Symbol(name);
        if (symbol->Dereference() == NULL) {
            throw std::runtime_error("Verifying empty symbol: " + name);
        }
        symbol->Update(grammar->NewVerifyUnique("Verification for " + name, symbol->Dereference()));
    }

    void Print(const std::string& name) {
        NodeReference* symbol = Symbol(name);
        if (symbol->Dereference() == NULL) {
            throw std::runtime_error("Printing empty symbol: " + name);
        }
        symbol->Update(grammar->NewPrint("Print of " + name, symbol->Dereference()));
    }

    void Terminal(const std::string& name) {
        grammar->SetTerminal(grammar->NewVerifyUnique("Verification for " + name, Symbol(name)));
    }
};

bool ParseLine(const std::string& in, std::vector<std::string>& out) {
    std::string::const_iterator it = in.begin();
    do {
        while (it != in.end() && isspace(*it)) {
            it++;
        }
        if (it == in.end()) {
            return true;
        }
        if (*it == '\"') {
            std::string::const_iterator it2 = it + 1;
            while (it2 != in.end() && *it2 != '\"') {
                it2++;
            }
            if (it2 == in.end()) {
                return false;
            }
            out.push_back(std::string(it + 1, it2));
            it = it2 + 1;
            continue;
        }
        std::string::const_iterator it3 = it + 1;
        while (it3 != in.end() && !isspace(*it3)) {
            it3++;
        }
        out.push_back(std::string(it, it3));
        it = it3;
    } while (it != in.end());
    return true;
}

bool Parse(std::istream& in, Grammar* outset) {
    GrammarBuilder builder(outset);
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() == 0 || line[0] == '#') {
            continue;
        }
        std::vector<std::string> pline;
        if (!ParseLine(line, pline)) {
            return false;
        }
        if (pline.size() == 0) {
            continue;
        }
        if (pline[0] == "symbol" && pline.size() > 1) {
            for (size_t x = 1; x < pline.size(); x++) {
                builder.Define(pline[x]);
            }
            continue;
        }
        if (pline[0] == "dict" && pline.size() > 2) {
            builder.OrDictionary(pline[1], std::vector<std::string>(pline.begin() + 2, pline.end()));
            continue;
        }
        if (pline[0] == "expand" && pline.size() > 2) {
            builder.OrConcatenation(pline[1], std::vector<std::string>(pline.begin() + 2, pline.end()));
            continue;
        }
        if (pline[0] == "terminal" && pline.size() == 2) {
            builder.Terminal(pline[1]);
            continue;
        }
        if (pline[0] == "deduplicate" && pline.size() == 2) {
            builder.Deduplicate(pline[1]);
            continue;
        }
        if (pline[0] == "unique" && pline.size() == 2) {
            builder.VerifyUnique(pline[1]);
            continue;
        }
        if (pline[0] == "print" && pline.size() == 2) {
            builder.Print(pline[1]);
            continue;
        }
        throw std::runtime_error("Unparsable line: " + line);
    }
    return true;
}

void RandomInteger(const BigNum& range, BigNum& out) {
    int bits = range.bits();
    std::vector<uint8_t> data;
    data.resize((bits + 7)/8);
    std::ifstream rng("/dev/urandom", std::ios::binary);
    do {
        rng.read((char*)&data[0], data.size());
        if (bits % 8) {
            data[0] >>= (8 - (bits % 8));
        }
        out = BigNum(&data[0], data.size());
    } while (out >= range);
}

std::string GenerateRandom(ExpansionState& state, const NodeBase* terminal, double bits) {
    std::vector<BigNum> cumulative;
    cumulative.push_back(BigNum());
    double minrange = pow(2.0, bits);
    for (int cost = 1; cost < 1000; cost++) {
        const BigNum* comb = terminal->Combinations(state, cost);
        if (comb == NULL) {
            throw std::runtime_error("Recursion detected");
        }
        BigNum ccomb = *comb;
        ccomb += cumulative.back();
        cumulative.push_back(std::move(ccomb));
        std::cerr << "Length " << cost << ": " << comb->hex() << " [" << log(comb->get_d())/log(2) << "] (cumulative " << cumulative.back().hex() << " [" << log(cumulative.back().get_d())/log(2.0) << ", " << log(cumulative.back().get_d())/log(2.0)/cost << "])" << std::endl;
        if (cumulative[cost].get_d() * 0.75 >= minrange) {
            for (int count = 1; count <= cost; count++) {
                BigNum range = cumulative[cost];
                range -= cumulative[cost - count];
                if (range >= minrange && range * 4 >= cumulative[cost] * 3) {
                    std::cerr << "Using length range [" << (cost - count + 1) << ".." << cost << "]: " << (log(range.get_d()) / log(2.0)) << " bits of entropy\n";
                    std::vector<std::string> vret;
                    BigNum rand;
                    RandomInteger(range, rand);
                    rand += cumulative[cost - count];
                    for (int realcost = cost - count + 1; realcost <= cost; realcost++) {
                        if (rand < cumulative[realcost]) {
                            std::vector<char> ret;
                            rand -= cumulative[realcost - 1];
                            terminal->Expand(-1, state, realcost, rand, ret);
                            return std::string(ret.begin(), ret.end());
                        }
                    }
                }
            }
        }
    }
    throw std::runtime_error("No solutions found\n");
}

int main(int argc, char** argv) {
    Grammar grammar;
    Parse(std::cin, &grammar);

    const NodeBase* terminal = grammar.GetTerminal();
    if (terminal == NULL) {
        throw std::runtime_error("No terminal symbol selected");
    }

    unsigned long num = argc > 2 ? strtoul(argv[2], NULL, 10) : 1;
    double bits = argc > 1 ? strtod(argv[1], NULL) : 64;

    if (num > 1) {
        bits += (1.0 / num + log(num) - 1.0) / log(2.0);
    }

    ExpansionState state;
    for (unsigned int i = 0; i < num; i++) {
        std::cout << GenerateRandom(state, terminal, bits) << "\n";
    }
    return 0;
}
