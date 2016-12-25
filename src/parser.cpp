#include "parser.h"
#include "expgraph.h"
#include "export.h"
#include <map>
#include <math.h>
#include <limits>

namespace {

class Lexer {
public:
    struct Token {
        enum TokenType {
            NONE,
            ERROR,
            SYMBOL,
            STRING,
            INTEGER,
            REGEXP,
            OPEN_BRACE,
            CLOSE_BRACE,
            ASTERISK,
            PLUS,
            QUESTION,
            EQUALS,
            PIPE,
            SEMICOLON,
            COMMA,
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

    char Peek() const {
        return *it;
    }

    bool End() const {
        return it == itend;
    }

    void Advance() {
        if (*it == '\n') {
            ++it;
            line_begin = it;
            ++line;
        } else {
            ++it;
        }
    }

public:
    int GetLine() const { return line; }
    int GetCol() const { return std::distance(line_begin, it); }

    Lexer(const char* ptr, size_t len) : it(ptr), itend(ptr + len), line_begin(it) {}

    Token Lex() {
        while (!End()) {
            char ch = Peek();
            if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t' && ch != '#') {
                break;
            }
            Advance();
            if (ch == '#') {
                while (!End() && Peek() != '\n') {
                    Advance();
                }
            }
        }
        if (End()) {
            return Token(Token::END);
        }
        char ch = Peek();
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
            std::string str = {ch};
            Advance();
            while (!End()) {
                char ch2 = Peek();
                if ((ch2 < 'a' || ch2 > 'z') && (ch2 < 'A' || ch2 > 'Z') && ch2 != '_' && (ch2 < '0' || ch2 > '9')) {
                    break;
                }
                str += ch2;
                Advance();
            }
            return Token(Token::SYMBOL, std::move(str));
        }
        if (ch >= '0' && ch <= '9') {
            std::string str = {ch};
            Advance();
            while (!End()) {
                char ch2 = Peek();
                if (ch2 < '0' || ch2 > '9') {
                    break;
                }
                str += ch2;
                Advance();
            }
            return Token(Token::INTEGER, std::move(str));
        }
        switch (ch) {
        case '(':
            Advance();
            return Token(Token::OPEN_BRACE);
        case ')':
            Advance();
            return Token(Token::CLOSE_BRACE);
        case '|':
            Advance();
            return Token(Token::PIPE);
        case '+':
            Advance();
            return Token(Token::PLUS);
        case '*':
            Advance();
            return Token(Token::ASTERISK);
        case '?':
            Advance();
            return Token(Token::QUESTION);
        case '=':
            Advance();
            return Token(Token::EQUALS);
        case ';':
            Advance();
            return Token(Token::SEMICOLON);
        case ',':
            Advance();
            return Token(Token::COMMA);
        case '/': {
            bool escaped = false;
            Advance();
            std::string str;
            while (true) {
                bool wasescaped = escaped;
                escaped = false;
                if (End()) {
                    return Token(Token::ERROR);
                }
                char ch2 = Peek();
                Advance();
                if (ch2 == '/' && !wasescaped) {
                    break;
                }
                str += ch2;
                if (ch2 == '\\' && !wasescaped) {
                    escaped = true;
                }
            }
            return Token(Token::REGEXP, std::move(str));
        }
        case '"': {
            Advance();
            std::string str;
            bool cont = true;
            while (cont) {
                if (End()) {
                    return Token(Token::ERROR);
                }
                char ch2 = Peek();
                Advance();
                switch (ch2) {
                case '"':
                    cont = false;
                    break;
                case '\\': {
                    if (End()) {
                        return Token(Token::ERROR);
                    }
                    char ch3 = Peek();
                    switch (ch3) {
                    case '"':
                    case '\\':
                        Advance();
                        str += ch3;
                        break;
                    case 'n':
                        Advance();
                        str += '\n';
                        break;
                    default:
                        return Token(Token::ERROR);
                    }
                    continue;
                }
                default:
                    str += ch2;
                }
            }
            return Token(Token::STRING, std::move(str));
        }
        default:
            return Token(Token::ERROR);
        }
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

    Graph::Ref regexp_d;

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
        if (dict.size() == 0) {
            return symbols["none"];
        }
        if (dict.size() == 1 && dict[0].size() == 0) {
            return symbols["empty"];
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

    Graph::Ref ParseRegexpSection(std::string::const_iterator& it, std::string::const_iterator itend) {
        std::vector<Graph::Ref> disj;
        std::vector<Graph::Ref> cat;
        while (it != itend) {
            char ch = *it;
            if (ch == ')' || ch == ']') {
                break;
            }
            ++it;
            switch (ch) {
            case '|':
                disj.emplace_back(graph->NewConcat(std::move(cat)));
                cat.clear();
                break;
            case '\\': {
                char ch2 = *it;
                ++it;
                switch (ch2) {
                case 'n':
                    cat.emplace_back(graph->NewDict({"\n"}));
                    break;
                case 'd':
                    if (!regexp_d) {
                        std::vector<std::string> digits;
                        for (char c = '0'; c <= '9'; ++c) {
                            digits.emplace_back(1, c);
                        }
                        regexp_d = graph->NewDict(std::move(digits));
                    }
                    cat.emplace_back(regexp_d);
                    break;
                default:
                    cat.emplace_back(graph->NewDict({{ch}}));
                    break;
                }
                break;
            }
            case '(': {
                auto ret = ParseRegexpSection(it, itend);
                if (!ret) {
                    return ret;
                }
                if (it == itend || *it != ')') {
                    error = "')' expected in regexp";
                    return Graph::Ref();
                }
                ++it;
                cat.emplace_back(std::move(ret));
                break;
            }
            case '[': {
                std::vector<std::string> opts;
                char lastchar = 0;
                bool havelast = false;
                do {
                    if (it == itend) {
                        error = "']' expected in regexp";
                        return Graph::Ref();
                    }
                    char ch2 = *(it++);
                    if (ch2 == ']') {
                        break;
                    }
                    if (ch2 == '-' && havelast && it != itend && *it != ']') {
                        char ch3 = *(it++);
                        while (lastchar != ch3) {
                            opts.emplace_back(1, ++lastchar);
                        }
                        havelast = false;
                    } else if (ch2 == '\\' && it != itend) {
                        opts.emplace_back(1, *(it++));
                        lastchar = '\\';
                        havelast = true;
                    } else {
                        opts.emplace_back(1, ch2);
                        lastchar = ch2;
                        havelast = true;
                    }
                } while(true);
                cat.emplace_back(graph->NewDict(std::move(opts)));
                break;
            }
            case '+': {
                if (cat.empty()) {
                    error = "'+' unexpected in regexp";
                    return Graph::Ref();
                }
                Graph::Ref n = graph->NewUndefined();
                graph->Define(n, graph->NewDisjunct(cat.back(), graph->NewConcat(cat.back(), n)));
                cat.back() = std::move(n);
                break;
            }
            case '*': {
                if (cat.empty()) {
                    error = "'*' unexpected in regexp";
                    return Graph::Ref();
                }
                Graph::Ref n = graph->NewUndefined();
                graph->Define(n, graph->NewDisjunct(symbols["empty"], graph->NewConcat(cat.back(), n)));
                cat.back() = std::move(n);
                break;
            }
            case '?': {
                if (cat.empty()) {
                    error = "'?' unexpected in regexp";
                    return Graph::Ref();
                }
                Graph::Ref n = graph->NewDisjunct(symbols["empty"], std::move(cat.back()));
                cat.back() = std::move(n);
                break;
            }
            default:
                cat.emplace_back(graph->NewString(std::string(1, ch)));
                break;
            }
        }
        disj.emplace_back(graph->NewConcat(std::move(cat)));
        return graph->NewDisjunct(std::move(disj));
    }

    Graph::Ref ParseRegexp(const std::string& str) {
        auto it = str.begin();
        Graph::Ref ref = ParseRegexpSection(it, str.end());
        if (!ref) {
            return ref;
        }
        if (it != str.end()) {
            error = "unbalanced braces in regexp";
            return Graph::Ref();
        }
        return ref;
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
                    error = "unbalanced braces";
                    return Graph::Ref();
                }
                break;
            }
            case Lexer::Token::STRING:
                nodes.emplace_back(EXPR, graph->NewString(lexer->Get().text));
                break;
            case Lexer::Token::REGEXP: {
                Lexer::Token regexp = lexer->Get();
                auto res = ParseRegexp(regexp.text);
                if (!res) {
                    return Graph::Ref();
                }
                nodes.emplace_back(EXPR, std::move(res));
                break;
            }
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
                } else if ((tok.text == "min_length" || tok.text == "max_length") && lexer->PeekType() == Lexer::Token::OPEN_BRACE) {
                    lexer->Skip();
                    if (lexer->PeekType() != Lexer::Token::INTEGER) {
                        error = "integer expected";
                        return Graph::Ref();
                    }
                    auto num = lexer->Get();
                    if (lexer->PeekType() != Lexer::Token::COMMA) {
                        error = "comma expected";
                        return Graph::Ref();
                    }
                    lexer->Skip();
                    auto expr = ParseExpression();
                    if (!expr.defined()) {
                        return Graph::Ref();
                    }
                    if (lexer->PeekType() != Lexer::Token::CLOSE_BRACE) {
                        error = "closing brace expected";
                        return Graph::Ref();
                    }
                    lexer->Skip();
                    size_t val = strtoul(num.text.c_str(), NULL, 10);
                    if (tok.text == "min_length") {
                        nodes.emplace_back(EXPR, graph->NewLengthLimit(std::move(expr), val, 1000000));
                    } else {
                        nodes.emplace_back(EXPR, graph->NewLengthLimit(std::move(expr), 0, val));
                    }
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
            case Lexer::Token::ERROR:
                error = "invalid token";
                return Graph::Ref();
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
