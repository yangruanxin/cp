#include "lexer.h"
#include <unordered_map>
#include <cctype>

Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1), column(1) {}

char Lexer::peek() const {
    return pos < source.size() ? source[pos] : '\0';
}

char Lexer::advance() {
    char c = source[pos++];
    if (c == '\n') { line++; column = 1; }
    else { column++; }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < source.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else if (c == '/' && pos + 1 < source.size()) {
            char next = source[pos + 1];
            if (next == '/') {
                while (pos < source.size() && advance() != '\n');
            } else if (next == '*') {
                advance(); advance();
                while (pos < source.size()) {
                    if (peek() == '*' && pos + 1 < source.size() && source[pos + 1] == '/') {
                        advance(); advance();
                        break;
                    }
                    advance();
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

Token Lexer::readIdentifier() {
    int startCol = column;
    std::string value;
    while (std::isalnum(peek()) || peek() == '_') {
        value += advance();
    }

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"int", TokenType::INT}, {"void", TokenType::VOID}, {"const", TokenType::CONST},
        {"if", TokenType::IF}, {"else", TokenType::ELSE}, {"while", TokenType::WHILE},
        {"break", TokenType::BREAK}, {"continue", TokenType::CONTINUE}, {"return", TokenType::RETURN}
    };

    auto it = keywords.find(value);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::ID;
    return {type, value, line, startCol};
}

Token Lexer::readNumber() {
    int startCol = column;
    std::string value;
    if (peek() == '-') {
        value += advance();
    }
    while (std::isdigit(peek())) {
        value += advance();
    }
    return {TokenType::NUMBER, value, line, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < source.size()) {
        skipWhitespaceAndComments();
        if (pos >= source.size()) break;

        char c = peek();
        int startCol = column;

        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentifier());
        } else if (std::isdigit(c)) {
            tokens.push_back(readNumber());
        } else if (c == '-') {
            // Check if negative number
            if (pos + 1 < source.size() && std::isdigit(source[pos + 1])) {
                tokens.push_back(readNumber());
            } else {
                advance();
                tokens.push_back({TokenType::MINUS, "-", line, startCol});
            }
        } else {
            advance();
            switch (c) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line, startCol}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line, startCol}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line, startCol}); break;
                case '%': tokens.push_back({TokenType::PERCENT, "%", line, startCol}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line, startCol}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line, startCol}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line, startCol}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line, startCol); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line, startCol}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line, startCol}); break;
                case '=':
                    if (peek() == '=') {
                        advance();
                        tokens.push_back({TokenType::EQ, "==", line, startCol});
                    } else {
                        tokens.push_back({TokenType::ASSIGN, "=", line, startCol});
                    }
                    break;
                case '!':
                    if (peek() == '=') {
                        advance();
                        tokens.push_back({TokenType::NE, "!=", line, startCol});
                    } else {
                        tokens.push_back({TokenType::NOT, "!", line, startCol});
                    }
                    break;
                case '<':
                    if (peek() == '=') {
                        advance();
                        tokens.push_back({TokenType::LE, "<=", line, startCol});
                    } else {
                        tokens.push_back({TokenType::LT, "<", line, startCol});
                    }
                    break;
                case '>':
                    if (peek() == '=') {
                        advance();
                        tokens.push_back({TokenType::GE, ">=", line, startCol});
                    } else {
                        tokens.push_back({TokenType::GT, ">", line, startCol});
                    }
                    break;
                case '&':
                    if (peek() == '&') {
                        advance();
                        tokens.push_back({TokenType::AND, "&&", line, startCol});
                    } else {
                        // error
                    }
                    break;
                case '|':
                    if (peek() == '|') {
                        advance();
                        tokens.push_back({TokenType::OR, "||", line, startCol});
                    } else {
                        // error
                    }
                    break;
                default: break;
            }
        }
    }
    tokens.push_back({TokenType::END, "", line, column});
    return tokens;
}
