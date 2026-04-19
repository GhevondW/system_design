#include "lang/parser.h"

#include <stdexcept>

namespace lang {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

std::expected<StmtList, ParseError> Parser::Parse() {
    try {
        StmtList statements;
        while (!IsAtEnd()) {
            statements.push_back(ParseStatement());
        }
        return statements;
    } catch (const std::runtime_error& e) {
        const auto& tok = pos_ < tokens_.size() ? tokens_[pos_] : tokens_.back();
        return std::unexpected(ParseError{e.what(), tok.line, tok.column});
    }
}

// --- Statement parsing ---

StmtPtr Parser::ParseStatement() {
    if (Match(TokenType::kLet)) return ParseLetDecl();
    if (Match(TokenType::kFn)) return ParseFnDecl();
    if (Match(TokenType::kIf)) return ParseIfStmt();
    if (Match(TokenType::kFor)) return ParseForStmt();
    if (Match(TokenType::kWhile)) return ParseWhileStmt();
    if (Match(TokenType::kReturn)) return ParseReturnStmt();

    // Expression statement
    auto expr = ParseExpression();
    Consume(TokenType::kSemicolon, "Expected ';' after expression");
    return MakeStmt(ExprStmt{std::move(expr)});
}

StmtPtr Parser::ParseLetDecl() {
    Token name = Consume(TokenType::kIdentifier, "Expected variable name after 'let'");
    Consume(TokenType::kAssign, "Expected '=' after variable name");
    auto initializer = ParseExpression();
    Consume(TokenType::kSemicolon, "Expected ';' after variable declaration");
    return MakeStmt(LetDecl{std::move(name), std::move(initializer)});
}

StmtPtr Parser::ParseFnDecl() {
    Token name = Consume(TokenType::kIdentifier, "Expected function name after 'fn'");
    Consume(TokenType::kLeftParen, "Expected '(' after function name");

    std::vector<Token> params;
    if (!Check(TokenType::kRightParen)) {
        do {
            params.push_back(Consume(TokenType::kIdentifier, "Expected parameter name"));
        } while (Match(TokenType::kComma));
    }
    Consume(TokenType::kRightParen, "Expected ')' after parameters");

    Consume(TokenType::kLeftBrace, "Expected '{' before function body");
    auto body = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}' after function body");

    return MakeStmt(FnDecl{std::move(name), std::move(params), std::move(body)});
}

StmtPtr Parser::ParseIfStmt() {
    Consume(TokenType::kLeftParen, "Expected '(' after 'if'");
    auto condition = ParseExpression();
    Consume(TokenType::kRightParen, "Expected ')' after if condition");

    Consume(TokenType::kLeftBrace, "Expected '{' after if condition");
    auto then_branch = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}' after if body");

    StmtList else_branch;
    if (Match(TokenType::kElse)) {
        if (Check(TokenType::kIf)) {
            // else if — wrap in a block with a single if statement
            Advance();  // consume 'if'
            auto else_if = ParseIfStmt();
            else_branch.push_back(std::move(else_if));
        } else {
            Consume(TokenType::kLeftBrace, "Expected '{' after 'else'");
            else_branch = ParseBlockBody();
            Consume(TokenType::kRightBrace, "Expected '}' after else body");
        }
    }

    return MakeStmt(IfStmt{std::move(condition), std::move(then_branch), std::move(else_branch)});
}

StmtPtr Parser::ParseForStmt() {
    Consume(TokenType::kLeftParen, "Expected '(' after 'for'");
    Consume(TokenType::kLet, "Expected 'let' in for loop");
    Token variable = Consume(TokenType::kIdentifier, "Expected variable name in for loop");
    Consume(TokenType::kColon, "Expected ':' in for loop");
    auto iterable = ParseExpression();
    Consume(TokenType::kRightParen, "Expected ')' after for loop header");

    Consume(TokenType::kLeftBrace, "Expected '{' before for loop body");
    auto body = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}' after for loop body");

    return MakeStmt(ForStmt{std::move(variable), std::move(iterable), std::move(body)});
}

StmtPtr Parser::ParseWhileStmt() {
    Consume(TokenType::kLeftParen, "Expected '(' after 'while'");
    auto condition = ParseExpression();
    Consume(TokenType::kRightParen, "Expected ')' after while condition");

    Consume(TokenType::kLeftBrace, "Expected '{' before while body");
    auto body = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}' after while body");

    return MakeStmt(WhileStmt{std::move(condition), std::move(body)});
}

StmtPtr Parser::ParseReturnStmt() {
    std::optional<ExprPtr> value;
    if (!Check(TokenType::kSemicolon)) {
        value = ParseExpression();
    }
    Consume(TokenType::kSemicolon, "Expected ';' after return");
    return MakeStmt(ReturnStmt{std::move(value)});
}

StmtPtr Parser::ParseBlock() {
    Consume(TokenType::kLeftBrace, "Expected '{'");
    auto stmts = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}'");
    return MakeStmt(BlockStmt{std::move(stmts)});
}

StmtList Parser::ParseBlockBody() {
    StmtList stmts;
    while (!Check(TokenType::kRightBrace) && !IsAtEnd()) {
        stmts.push_back(ParseStatement());
    }
    return stmts;
}

// --- Expression parsing (precedence climbing) ---

ExprPtr Parser::ParseExpression() {
    return ParseAssignment();
}

ExprPtr Parser::ParseAssignment() {
    auto expr = ParseOr();

    if (Match(TokenType::kAssign)) {
        // Right-associative: `a = b = c` parses as `a = (b = c)`, unlike the
        // left-associative while-loops used for the other binary operators below.
        auto value = ParseAssignment();
        return MakeExpr(AssignExpr{std::move(expr), std::move(value)});
    }

    return expr;
}

ExprPtr Parser::ParseOr() {
    auto left = ParseAnd();
    while (Match(TokenType::kOr)) {
        auto op = Previous();
        auto right = ParseAnd();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseAnd() {
    auto left = ParseEquality();
    while (Match(TokenType::kAnd)) {
        auto op = Previous();
        auto right = ParseEquality();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseEquality() {
    auto left = ParseComparison();
    while (Match(TokenType::kEqual) || Match(TokenType::kNotEqual)) {
        auto op = Previous();
        auto right = ParseComparison();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseComparison() {
    auto left = ParseAddition();
    while (Match(TokenType::kLess) || Match(TokenType::kLessEqual) ||
           Match(TokenType::kGreater) || Match(TokenType::kGreaterEqual)) {
        auto op = Previous();
        auto right = ParseAddition();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseAddition() {
    auto left = ParseMultiplication();
    while (Match(TokenType::kPlus) || Match(TokenType::kMinus)) {
        auto op = Previous();
        auto right = ParseMultiplication();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseMultiplication() {
    auto left = ParseUnary();
    while (Match(TokenType::kStar) || Match(TokenType::kSlash) || Match(TokenType::kPercent)) {
        auto op = Previous();
        auto right = ParseUnary();
        left = MakeExpr(BinaryExpr{std::move(left), std::move(op), std::move(right)});
    }
    return left;
}

ExprPtr Parser::ParseUnary() {
    if (Match(TokenType::kMinus) || Match(TokenType::kNot)) {
        auto op = Previous();
        auto operand = ParseUnary();
        return MakeExpr(UnaryExpr{std::move(op), std::move(operand)});
    }
    return ParsePostfix();
}

ExprPtr Parser::ParsePostfix() {
    auto expr = ParsePrimary();

    while (true) {
        if (Match(TokenType::kDot)) {
            Token member = Consume(TokenType::kIdentifier, "Expected property name after '.'");
            expr = MakeExpr(MemberAccessExpr{std::move(expr), std::move(member)});
        } else if (Match(TokenType::kLeftParen)) {
            // Function call
            std::vector<ExprPtr> args;
            if (!Check(TokenType::kRightParen)) {
                do {
                    args.push_back(ParseExpression());
                } while (Match(TokenType::kComma));
            }
            Consume(TokenType::kRightParen, "Expected ')' after arguments");
            expr = MakeExpr(CallExpr{std::move(expr), std::move(args)});
        } else if (Match(TokenType::kLeftBracket)) {
            // Index access
            auto index = ParseExpression();
            Consume(TokenType::kRightBracket, "Expected ']' after index");
            expr = MakeExpr(IndexAccessExpr{std::move(expr), std::move(index)});
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::ParsePrimary() {
    // Literals
    if (Match(TokenType::kInteger) || Match(TokenType::kFloat) || Match(TokenType::kString) ||
        Match(TokenType::kTrue) || Match(TokenType::kFalse) || Match(TokenType::kNull)) {
        return MakeExpr(LiteralExpr{Previous()});
    }

    // Identifier
    if (Match(TokenType::kIdentifier)) {
        return MakeExpr(IdentifierExpr{Previous()});
    }

    // Parenthesized expression
    if (Match(TokenType::kLeftParen)) {
        auto expr = ParseExpression();
        Consume(TokenType::kRightParen, "Expected ')' after expression");
        return expr;
    }

    // List literal or lambda: [
    if (Check(TokenType::kLeftBracket)) {
        return ParseListLiteral();
    }

    // Map literal or block expression: {
    if (Check(TokenType::kLeftBrace)) {
        return ParseMapOrBlock();
    }

    // Match expression
    if (Match(TokenType::kMatch)) {
        return ParseMatchExpr();
    }

    ThrowError("Unexpected token");
}

ExprPtr Parser::ParseListLiteral() {
    Advance();  // consume [

    // `[` is ambiguous: both list literals `[a, b, c]` and lambdas
    // `[captures](params) { body }` start with it. We peek forward assuming a
    // capture list; if we don't find the `](` that confirms a lambda, we rewind
    // pos_ to saved_pos and fall through to the list-literal branch.
    size_t saved_pos = pos_;

    std::vector<Token> captures;
    if (Check(TokenType::kIdentifier)) {
        // Tentatively parse capture list
        captures.push_back(tokens_[pos_]);
        pos_++;
        while (pos_ < tokens_.size() && tokens_[pos_].type == TokenType::kComma) {
            pos_++;  // skip comma
            if (pos_ < tokens_.size() && tokens_[pos_].type == TokenType::kIdentifier) {
                captures.push_back(tokens_[pos_]);
                pos_++;
            } else {
                break;
            }
        }
    }

    if (pos_ < tokens_.size() && tokens_[pos_].type == TokenType::kRightBracket) {
        size_t after_bracket = pos_ + 1;
        if (after_bracket < tokens_.size() && tokens_[after_bracket].type == TokenType::kLeftParen) {
            pos_++;  // consume ]
            return ParseLambda(std::move(captures));
        }
    }

    // Not a lambda — backtrack and parse as list
    pos_ = saved_pos;
    captures.clear();

    std::vector<ExprPtr> elements;
    if (!Check(TokenType::kRightBracket)) {
        do {
            elements.push_back(ParseExpression());
        } while (Match(TokenType::kComma));
    }
    Consume(TokenType::kRightBracket, "Expected ']' after list");
    return MakeExpr(ListLiteralExpr{std::move(elements)});
}

ExprPtr Parser::ParseMapOrBlock() {
    Advance();  // consume {

    // Empty map: {}
    if (Check(TokenType::kRightBrace)) {
        Advance();
        return MakeExpr(MapLiteralExpr{});
    }

    // Check if this is a map: { identifier: expr, ... }
    if (Check(TokenType::kIdentifier) && pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::kColon) {
        // Map literal
        std::vector<MapEntry> entries;
        do {
            Token key = Consume(TokenType::kIdentifier, "Expected map key");
            Consume(TokenType::kColon, "Expected ':' after map key");
            auto value = ParseExpression();
            entries.push_back(MapEntry{std::move(key), std::move(value)});
        } while (Match(TokenType::kComma));
        Consume(TokenType::kRightBrace, "Expected '}' after map");
        return MakeExpr(MapLiteralExpr{std::move(entries)});
    }

    // Otherwise treat as a map with string keys (could also be block — for now, error)
    ThrowError("Expected map literal with 'key: value' entries");
}

ExprPtr Parser::ParseLambda(std::vector<Token> captures) {
    // [ and captures and ] already consumed. Now at (
    Consume(TokenType::kLeftParen, "Expected '(' after lambda capture list");
    std::vector<Token> params;
    if (!Check(TokenType::kRightParen)) {
        do {
            params.push_back(Consume(TokenType::kIdentifier, "Expected parameter name"));
        } while (Match(TokenType::kComma));
    }
    Consume(TokenType::kRightParen, "Expected ')' after lambda parameters");
    Consume(TokenType::kLeftBrace, "Expected '{' before lambda body");
    auto body = ParseBlockBody();
    Consume(TokenType::kRightBrace, "Expected '}' after lambda body");

    return MakeExpr(LambdaExpr{std::move(captures), std::move(params), std::move(body)});
}

ExprPtr Parser::ParseMatchExpr() {
    // 'match' already consumed
    Consume(TokenType::kLeftParen, "Expected '(' after 'match'");
    auto subject = ParseExpression();
    Consume(TokenType::kRightParen, "Expected ')' after match subject");
    Consume(TokenType::kLeftBrace, "Expected '{' after match subject");

    std::vector<MatchArm> arms;
    while (!Check(TokenType::kRightBrace) && !IsAtEnd()) {
        ExprPtr pattern;
        if (Match(TokenType::kUnderscore)) {
            pattern = nullptr;  // wildcard
        } else {
            pattern = ParseExpression();
        }
        Consume(TokenType::kArrow, "Expected '=>' in match arm");
        auto value = ParseExpression();
        arms.push_back(MatchArm{std::move(pattern), std::move(value)});

        // Optional comma between arms
        Match(TokenType::kComma);
    }
    Consume(TokenType::kRightBrace, "Expected '}' after match arms");

    return MakeExpr(MatchExpr{std::move(subject), std::move(arms)});
}

// --- Token helpers ---

const Token& Parser::Current() const {
    return tokens_[pos_];
}

const Token& Parser::Previous() const {
    return tokens_[pos_ - 1];
}

bool Parser::IsAtEnd() const {
    return pos_ >= tokens_.size() || tokens_[pos_].type == TokenType::kEof;
}

bool Parser::Check(TokenType type) const {
    if (IsAtEnd()) return false;
    return tokens_[pos_].type == type;
}

bool Parser::Match(TokenType type) {
    if (Check(type)) {
        Advance();
        return true;
    }
    return false;
}

Token Parser::Consume(TokenType type, std::string_view message) {
    if (Check(type)) return Advance();
    ThrowError(message);
}

Token Parser::Advance() {
    if (!IsAtEnd()) pos_++;
    return tokens_[pos_ - 1];
}

void Parser::ThrowError(std::string_view message) {
    const auto& tok = Current();
    throw std::runtime_error(std::string(message) + " at line " + std::to_string(tok.line) +
                             ", column " + std::to_string(tok.column) + " (got '" + tok.lexeme +
                             "')");
}

}  // namespace lang
