#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "lang/token.h"

namespace lang {

class Lexer {
public:
    explicit Lexer(std::string source);

    [[nodiscard]] std::vector<Token> Tokenize();
    [[nodiscard]] Token NextToken();

private:
    [[nodiscard]] bool IsAtEnd() const;
    [[nodiscard]] char Peek() const;
    [[nodiscard]] char PeekNext() const;
    char Advance();
    bool Match(char expected);

    void SkipWhitespace();
    void SkipLineComment();
    void SkipBlockComment();

    Token MakeToken(TokenType type, std::string lexeme) const;
    Token MakeToken(TokenType type) const;
    Token ErrorToken(std::string message) const;

    Token ScanString();
    Token ScanNumber();
    Token ScanIdentifierOrKeyword();

    [[nodiscard]] static TokenType LookupKeyword(std::string_view word);

    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    int token_start_line_ = 1;
    int token_start_column_ = 1;
};

}  // namespace lang
