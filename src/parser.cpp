#include "parser.h"
#include "expgraph.h"
#include "export.h"
#include <map>
#include <math.h>

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
        if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z') || *it == '_') {
            auto bit = it;
            ++it;
            do {
                if (it == itend) {
                    break;
                }
                if ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z') || *it == '_' || (*it >= '0' && *it <= '9')) {
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
    std::string error;

public:
    std::map<std::string, Graph::Ref> symbols;

    Parser(Lexer* lex, Graph* gra) : lexer(lex), graph(gra) {
        symbols["empty"] = gra->NewEmpty();
        symbols["none"] = gra->NewNone();
    }

    Graph::Ref ParseDict() {
        std::vector<std::string> dict;
        while (lexer->PeekType() == Lexer::Token::SYMBOL || lexer->PeekType() == Lexer::Token::STRING) {
            auto l = lexer->Get();
            dict.emplace_back(std::move(l.text));
        }
        return graph->NewDict(std::move(dict));
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
                if (!res.defined()) {
                    return Graph::Ref();
                }
                if (lexer->PeekType() == Lexer::Token::CLOSE_BRACE) {
                    lexer->Skip();
                    nodes.emplace_back(EXPR, std::move(res));
                } else {
                    error = "Unbalanced braces";
                    return Graph::Ref();
                }
                break;
            }
            case Lexer::Token::STRING:
                nodes.emplace_back(EXPR, graph->NewString(lexer->Get().text));
                break;
            case Lexer::Token::SYMBOL: {
                Lexer::Token tok = lexer->Get();
                if (tok.text == "dedup" && lexer->PeekType() == Lexer::Token::OPEN_BRACE) {
                    auto res = ParseExpression();
                    if (!res.defined()) {
                        return Graph::Ref();
                    }
                    nodes.emplace_back(EXPR, graph->NewDedup(std::move(res)));
                } else if (tok.text == "dict" && lexer->PeekType() == Lexer::Token::OPEN_BRACE) {
                    lexer->Skip();
                    auto res = ParseDict();
                    if (lexer->PeekType() != Lexer::Token::CLOSE_BRACE) {
                        error = "closing brace expected";
                        return Graph::Ref();
                    }
                    lexer->Skip();
                    nodes.emplace_back(EXPR, std::move(res));
                } else {
                    nodes.emplace_back(EXPR, ParseSymbol(std::move(tok.text)));
                }
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
            case Lexer::Token::QUESTION:
                if (nodes.empty() || nodes.back().first != EXPR) {
                    cont = false;
                    break;
                }
                lexer->Skip();
                nodes.back().second = graph->NewDisjunct(symbols["empty"], nodes.back().second);
                break;
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
            error = "symbol expected";
            return false;
        }

        Lexer::Token sym = lexer->Get();
        Graph::Ref symbol = ParseSymbol(std::move(sym.text));
        if (graph->IsDefined(symbol)) {
            error = "duplicate definition for symbol '" + sym.text + "'";
            return false;
        }

        if (lexer->PeekType() != Lexer::Token::EQUALS) {
            error = "equals sign expected";
            return false;
        }
        lexer->Skip();

        Graph::Ref expr = ParseExpression();
        if (!expr.defined()) {
            // Error already set by ParseExpression
            return false;
        }

        if (lexer->PeekType() != Lexer::Token::SEMICOLON) {
            error = "semicolon expected";
            return false;
        }

        graph->Define(symbol, std::move(expr));

        lexer->Skip();
        return true;
    }

    std::pair<Graph::Ref, std::string> ParseProgram() {
        while (lexer->PeekType() != Lexer::Token::END) {
            if (!ParseStatement()) {
                return std::make_pair(Graph::Ref(), error);
            }
        }
        return std::make_pair(ParseSymbol("main"), "");
    }
};

}

std::string Parse(Graph& graph, Graph::Ref& mainout, const char* str, size_t len) {
    Graph::Ref main;
    Lexer lex(str, len);

    {
        Parser parser(&lex, &graph);
        auto ret = parser.ParseProgram();
        if (!ret.first.defined()) {
            return ret.second + " on line " + std::to_string(lex.GetLine()) + ", column " + std::to_string(lex.GetCol());
        }
        main = std::move(ret.first);

        for (auto const &x : parser.symbols) {
            if (!graph.IsDefined(x.second)) {
                return "undefined symbol '" + x.first + "'";
            }
        }
    }

    if (!graph.IsDefined(main)) {
        return "main is not defined";
    }
    if (!graph.FullyDefined()) {
        return "undefined symbol";
    }

    Optimize(graph);
    OptimizeRef(graph, main);
    mainout = std::move(main);
    return "";
}
