#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "lang/token.h"

namespace lang {

// Forward declarations
struct Expression;
struct Statement;

using ExprPtr = std::unique_ptr<Expression>;
using StmtPtr = std::unique_ptr<Statement>;
using StmtList = std::vector<StmtPtr>;

// --- Expression nodes ---

struct LiteralExpr {
    Token token;  // carries the value in lexeme
};

struct IdentifierExpr {
    Token name;
};

struct UnaryExpr {
    Token op;
    ExprPtr operand;
};

struct BinaryExpr {
    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct AssignExpr {
    ExprPtr target;  // identifier or member access
    ExprPtr value;
};

struct CallExpr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
};

struct MemberAccessExpr {
    ExprPtr object;
    Token member;
};

struct IndexAccessExpr {
    ExprPtr object;
    ExprPtr index;
};

struct ListLiteralExpr {
    std::vector<ExprPtr> elements;
};

struct MapEntry {
    Token key;
    ExprPtr value;
};

struct MapLiteralExpr {
    std::vector<MapEntry> entries;
};

struct LambdaExpr {
    std::vector<Token> captures;
    std::vector<Token> params;
    StmtList body;
};

struct MatchArm {
    ExprPtr pattern;  // nullptr means wildcard (_)
    ExprPtr value;
};

struct MatchExpr {
    ExprPtr subject;
    std::vector<MatchArm> arms;
};

// The expression variant
struct Expression {
    using Variant =
        std::variant<LiteralExpr, IdentifierExpr, UnaryExpr, BinaryExpr, AssignExpr, CallExpr,
                     MemberAccessExpr, IndexAccessExpr, ListLiteralExpr, MapLiteralExpr,
                     LambdaExpr, MatchExpr>;
    Variant data;

    template <typename T>
    explicit Expression(T&& val) : data(std::forward<T>(val)) {}
};

// --- Statement nodes ---

struct LetDecl {
    Token name;
    ExprPtr initializer;
};

struct FnDecl {
    Token name;
    std::vector<Token> params;
    StmtList body;
};

struct IfStmt {
    ExprPtr condition;
    StmtList then_branch;
    StmtList else_branch;  // empty if no else
};

struct ForStmt {
    Token variable;
    ExprPtr iterable;
    StmtList body;
};

struct WhileStmt {
    ExprPtr condition;
    StmtList body;
};

struct ReturnStmt {
    std::optional<ExprPtr> value;
};

struct ExprStmt {
    ExprPtr expression;
};

struct BlockStmt {
    StmtList statements;
};

// The statement variant
struct Statement {
    using Variant =
        std::variant<LetDecl, FnDecl, IfStmt, ForStmt, WhileStmt, ReturnStmt, ExprStmt, BlockStmt>;
    Variant data;

    template <typename T>
    explicit Statement(T&& val) : data(std::forward<T>(val)) {}
};

// --- Helpers to make AST nodes ---

template <typename T>
ExprPtr MakeExpr(T&& val) {
    return std::make_unique<Expression>(std::forward<T>(val));
}

template <typename T>
StmtPtr MakeStmt(T&& val) {
    return std::make_unique<Statement>(std::forward<T>(val));
}

}  // namespace lang
