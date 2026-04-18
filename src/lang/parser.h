#pragma once

#include <expected>
#include <string>
#include <vector>

#include "lang/ast.h"
#include "lang/token.h"

namespace lang {

struct ParseError {
    std::string message;
    int line;
    int column;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    [[nodiscard]] std::expected<StmtList, ParseError> Parse();

private:
    // Statement parsing
    StmtPtr ParseStatement();
    StmtPtr ParseLetDecl();
    StmtPtr ParseFnDecl();
    StmtPtr ParseIfStmt();
    StmtPtr ParseForStmt();
    StmtPtr ParseWhileStmt();
    StmtPtr ParseReturnStmt();
    StmtPtr ParseBlock();
    StmtList ParseBlockBody();

    // Expression parsing (Pratt-style precedence climbing)
    ExprPtr ParseExpression();
    ExprPtr ParseAssignment();
    ExprPtr ParseOr();
    ExprPtr ParseAnd();
    ExprPtr ParseEquality();
    ExprPtr ParseComparison();
    ExprPtr ParseAddition();
    ExprPtr ParseMultiplication();
    ExprPtr ParseUnary();
    ExprPtr ParsePostfix();
    ExprPtr ParsePrimary();

    // Specific primary parsers
    ExprPtr ParseListLiteral();
    ExprPtr ParseMapOrBlock();
    ExprPtr ParseLambda(std::vector<Token> captures);
    ExprPtr ParseMatchExpr();

    // Token helpers
    [[nodiscard]] const Token& Current() const;
    [[nodiscard]] const Token& Previous() const;
    [[nodiscard]] bool IsAtEnd() const;
    [[nodiscard]] bool Check(TokenType type) const;
    bool Match(TokenType type);
    Token Consume(TokenType type, std::string_view message);
    Token Advance();

    [[noreturn]] void ThrowError(std::string_view message);

    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

}  // namespace lang
