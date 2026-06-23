#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    ID, NUMBER,
    INT, VOID, CONST, IF, ELSE, WHILE, BREAK, CONTINUE, RETURN,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    AND, OR, NOT,
    EQ, NE, LT, GT, LE, GE,
    ASSIGN,
    LPAREN, RPAREN, LBRACE, RBRACE, SEMICOLON, COMMA,
    END
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos;
    int line;
    int column;

    char peek() const;
    char advance();
    void skipWhitespaceAndComments();
    Token readIdentifier();
    Token readNumber();
};

#endif
