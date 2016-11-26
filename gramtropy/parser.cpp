#include "parser.h"
#include "expgraph.h"
#include <map>

namespace {

class Lexer {
public:
    struct Token {
        enum TokenType {
            NONE,
            ERROR,
            SYMBOL,
            STRING,
            OPEN_BRACE,
            CLOSE_BRACE,
            ASTERISK,
            PLUS,
            QUESTION,
            EQUALS,
            PIPE,
            SEMICOLON,
            END
        };
        TokenType tokentype;
        std::string text;

        Token() : tokentype(NONE) {}
        Token(TokenType typ) : tokentype(typ) {}
        Token(TokenType typ, std::string&& str) : tokentype(typ), text(std::move(str)) {}
    };

private:
    const char* it;
    const char* const itend;
    Token next;
    int line = 0;
    const char* line_begin;

public:
    int GetLine() const { return line; }
    int GetCol() const { return std::distance(line_begin, it); }

    Lexer(const char* ptr, size_t len) : it(ptr), itend(ptr + len), line_begin(it) {}

    Token Lex() {
        while (it != itend && (*it == ' ' || *it == '\n' || *it == '\r' || *it == '\t' || *it == '#')) {
            if (*it == '\n') {
                line++;
                line_begin = std::next(it);
            }
            if (*it == '#') {
                while (it != itend && *it != '\n') {
                    ++it;
                }
            } else {
                ++it;
            }
        }
        if (it == itend) {
            return Token(Token::END);
        }
        if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z')) {
            auto bit = it;
            ++it;
            do {
                if (it == itend) {
                    break;
                }
                if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z') || (*it >= '0' && *it <= '9')) {
                    ++it;
                    continue;
                }
                break;
            } while(true);
            return Token(Token::SYMBOL, std::string(bit, it));
        }
        switch (*it) {
        case '(':
            ++it;
            return Token(Token::OPEN_BRACE);
        case ')':
            ++it;
            return Token(Token::CLOSE_BRACE);
        case '|':
            ++it;
            return Token(Token::PIPE);
        case '+':
            ++it;
            return Token(Token::PLUS);
        case '*':
            ++it;
            return Token(Token::ASTERISK);
        case '?':
            ++it;
            return Token(Token::QUESTION);
        case '=':
            ++it;
            return Token(Token::EQUALS);
        case ';':
            ++it;
            return Token(Token::SEMICOLON);
        case '"': {
            ++it;
            std::string str;
            do {
                if (it == itend) {
                    return Token(Token::ERROR);
                }
                if (*it == '"') {
                    ++it;
                    break;
                }
                if (*it == '\\') {
                    ++it;
                    if (it == itend) {
                        return Token(Token::ERROR);
                    }
                    if (*it == '"') {
                        ++it;
                        str += '"';
                        continue;
                    }
                    if (*it == '\\') {
                        ++it;
                        str += '\\';
                        continue;
                    }
                    if (*it == 'n') {
                        ++it;
                        str += '\n';
                        continue;
                    }
                    return Token(Token::ERROR);
                }
                if (*it == '\n') {
                    ++line;
                    line_begin = std::next(it);
                }
                str += *it;
                ++it;
            } while(true);
            return Token(Token::STRING, std::move(str));
        }
        default:
            break;
        }
        return Token(Token::ERROR);
    }

    const Token::TokenType PeekType() {
        if (next.tokentype == Token::NONE) {
            next = Lex();
        }
        return next.tokentype;
    }

    Token Get() {
        Token ret = std::move(next);
        next = Lex();
        return ret;
    }

    void Skip() {
        next = Lex();
    }
};

class Parser {
private:
    enum NodeType {
        EXPR,
        PIPE,
    };

    Lexer* lexer;
    Graph* graph;
    std::map<std::string, Graph::Ref> symbols;

public:
    Parser(Lexer* lex, Graph* gra) : lexer(lex), graph(gra) {
        symbols["empty"] = gra->NewEmpty();
        symbols["none"] = gra->NewNone();
    }

    Graph::Ref ParseSymbol(std::string&& name) {
        auto it = symbols.find(name);
        if (it == symbols.end()) {
            it = symbols.emplace(name, graph->NewUndefined()).first;
        }
        return it->second;
    }

    Graph::Ref ParseExpression() {
        std::vector<std::pair<NodeType, Graph::Ref>> nodes;

        bool cont = true;
        while (cont) {
            switch (lexer->PeekType()) {
            case Lexer::Token::OPEN_BRACE: {
                lexer->Skip();
                auto res = ParseExpression();
                if (lexer->PeekType() == Lexer::Token::CLOSE_BRACE) {
                    lexer->Skip();
                    nodes.emplace_back(EXPR, std::move(res));
                } else {
                    assert(!"Unbalanced braces");
                }
                break;
            }
            case Lexer::Token::STRING:
                nodes.emplace_back(EXPR, graph->NewString(lexer->Get().text));
                break;
            case Lexer::Token::SYMBOL: {
                Lexer::Token tok = lexer->Get();
                nodes.emplace_back(EXPR, ParseSymbol(std::move(tok.text)));
                break;
            }
            case Lexer::Token::PIPE:
                lexer->Skip();
                nodes.emplace_back(PIPE, symbols["none"]);
                break;
            case Lexer::Token::ASTERISK: {
                if (nodes.empty() || nodes.back().first != EXPR) {
                    cont = false;
                    break;
                }
                lexer->Skip();
                Graph::Ref n = graph->NewUndefined();
                graph->Define(n, graph->NewDisjunct(symbols["empty"], graph->NewConcat(nodes.back().second, n)));
                nodes.back().second = std::move(n);
                break;
            }
            case Lexer::Token::PLUS: {
                if (nodes.empty() || nodes.back().first != EXPR) {
                    cont = false;
                    break;
                }
                lexer->Skip();
                Graph::Ref n = graph->NewUndefined();
                graph->Define(n, graph->NewDisjunct(nodes.back().second, graph->NewConcat(nodes.back().second, n)));
                nodes.back().second = std::move(n);
                break;
            }
            case Lexer::Token::QUESTION: {
                if (nodes.empty() || nodes.back().first != EXPR) {
                    cont = false;
                    break;
                }
                lexer->Skip();
                nodes.back().second = graph->NewDisjunct(symbols["empty"], nodes.back().second);
                break;
            }
            default:
                cont = false;
            }
        }

        std::vector<Graph::Ref> disj;
        std::vector<Graph::Ref> cat;

        for (auto& node : nodes) {
            switch (node.first) {
            case EXPR:
                cat.emplace_back(std::move(node.second));
                break;
            case PIPE:
                disj.push_back(graph->NewConcat(std::move(cat)));
                cat.clear();
                break;
            }
        }

        disj.push_back(graph->NewConcat(std::move(cat)));
        return graph->NewDisjunct(std::move(disj));
    }

    bool ParseStatement() {
        if (lexer->PeekType() != Lexer::Token::SYMBOL) {
            fprintf(stderr, "fail 1\n");
            return false;
        }

        Lexer::Token sym = lexer->Get();
        Graph::Ref symbol = ParseSymbol(std::move(sym.text));
        if (graph->IsDefined(symbol)) {
            fprintf(stderr, "fail 2\n");
            return false;
        }

        if (lexer->PeekType() != Lexer::Token::EQUALS) {
            fprintf(stderr, "fail 3\n");
            return false;
        }
        lexer->Skip();

        Graph::Ref expr = ParseExpression();
//        if (!graph->IsDefined(expr)) {
//            fprintf(stderr, "fail 4\n");
//            return false;
//        }

        if (lexer->PeekType() != Lexer::Token::SEMICOLON) {
            fprintf(stderr, "fail 5\n");
            return false;
        }

        graph->Define(symbol, std::move(expr));

        lexer->Skip();
        return true;
    }

    std::pair<bool, Graph::Ref> ParseProgram() {
        while (lexer->PeekType() != Lexer::Token::END) {
            if (!ParseStatement()) {
                return std::make_pair(false, graph->NewUndefined());
            }
        }
        return std::make_pair(true, ParseSymbol("main"));
    }
};

}

#ifdef MAIN
#include <stdio.h>
#include "expgraph.h"
#include "expander.h"


int main(int argc, char** argv) {
    char *buf = (char*)malloc(1048576);
    ssize_t len = fread(buf, 1, 1048576, stdin);
    int reslen = argc > 1 ? strtoul(argv[1], NULL, 10) : 16;

    if (len < 0) {
        fprintf(stderr, "Failed to read\n");
        return -1;
    }

    Lexer lex(buf, len);

    Graph graph;
    Graph::Ref main;

    {
        Parser parser(&lex, &graph);
        auto ret = parser.ParseProgram();
        if (!ret.first) {
            fprintf(stderr, "Parse error: line %i, column %i\n", lex.GetLine(), lex.GetCol());
            return -1;
        }
        main = std::move(ret.second);
        // std::string desc = Describe(graph, main);
        // printf("Before optimize:\n");
        // printf("%s\n\n", desc.c_str());
    }

    free(buf);

    if (!graph.IsDefined(main)) {
        fprintf(stderr, "Error: main is not defined\n");
        return -4;
    }
    if (!graph.FullyDefined()) {
        fprintf(stderr, "Error: undefined symbols remain\n");
        return -3;
    }

    Optimize(graph);
    OptimizeRef(graph, main);
    // std::string desc = Describe(graph, main);
    // printf("After optimize:\n");
    // printf("%s\n", desc.c_str());


    ExpGraph::Ref emain;
    ExpGraph expgraph;
    {
        Expander exp(&graph, &expgraph);
        emain = exp.Expand(main, reslen);
        if (!emain.defined()) {
            fprintf(stderr, "Error: infinite recursion\n");
            return -4;
        }
    }

    Optimize(expgraph);

/*
    printf("\nExpansion:\n");
    int cnt = 0;
    std::map<const ExpGraph::Node*, int> dump;
    for (const auto& node : expgraph.nodes) {
        dump[&node] = cnt;
        printf("%s ", node.count.hex().c_str());
        if (node.nodetype == ExpGraph::Node::NodeType::DICT) {
            printf("0 %X", (unsigned int)node.dict.size());
            for (size_t s = 0; s < node.dict.size(); s++) {
                std::string str = node.dict[s];
                printf(" %X %.*s", (unsigned int)str.size(), (int)str.size(), str.c_str());
            }
        } else if (node.nodetype == ExpGraph::Node::NodeType::CONCAT) {
            printf("1 %X", (unsigned int)node.refs.size());
            size_t pos = 0;
            for (size_t s = 0; s < node.refs.size(); s++) {
                printf(" %X %X", (unsigned int)pos, cnt - dump[&*node.refs[s]]);
                assert(node.refs[s]->len >= 0);
                pos += node.refs[s]->len;
            }
        } else {
            printf("2 %X", (unsigned int)node.refs.size());
            for (size_t s = 0; s < node.refs.size(); s++) {
                printf(" %X", cnt - dump[&*node.refs[s]]);
            }
        }
        printf(" ");
        cnt++;
    }
    printf("- main = node%i\n", dump[&*emain]);
*/

    printf("%lu node model, %s combinations\n", (unsigned long)expgraph.nodes.size(), emain->count.hex().c_str());

    for (int i = 0; i < 100; i++) {
        std::string str = Generate(emain);
        printf("Res: %s\n", str.c_str());
    }

    emain = ExpGraph::Ref();
    main = Graph::Ref();
    return 0;
}

#endif
