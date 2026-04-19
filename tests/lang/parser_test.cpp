#include "lang/ast.h"
#include "lang/lexer.h"
#include "lang/parser.h"

#include <gtest/gtest.h>

namespace lang {
namespace {

// Helper: parse source and return statement list
StmtList ParseSource(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.Tokenize();
    Parser parser(std::move(tokens));
    auto result = parser.Parse();
    EXPECT_TRUE(result.has_value()) << result.error().message;
    return std::move(result.value());
}

// Helper: parse source, expect failure
ParseError ParseSourceError(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.Tokenize();
    Parser parser(std::move(tokens));
    auto result = parser.Parse();
    EXPECT_FALSE(result.has_value());
    return result.error();
}

// --- Let declarations ---

TEST(Parser, LetDecl_Integer) {
    auto stmts = ParseSource("let x = 42;");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    EXPECT_EQ(let_decl->name.lexeme, "x");
    auto* lit = std::get_if<LiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->token.type, TokenType::kInteger);
    EXPECT_EQ(lit->token.lexeme, "42");
}

TEST(Parser, LetDecl_String) {
    auto stmts = ParseSource("let name = \"hello\";");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* lit = std::get_if<LiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->token.lexeme, "hello");
}

TEST(Parser, LetDecl_Bool) {
    auto stmts = ParseSource("let active = true;");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* lit = std::get_if<LiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->token.type, TokenType::kTrue);
}

TEST(Parser, LetDecl_List) {
    auto stmts = ParseSource("let items = [1, 2, 3];");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* list = std::get_if<ListLiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->elements.size(), 3);
}

TEST(Parser, LetDecl_Map) {
    auto stmts = ParseSource("let user = { name: \"Alice\", age: 30 };");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* map = std::get_if<MapLiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->entries.size(), 2);
    EXPECT_EQ(map->entries[0].key.lexeme, "name");
    EXPECT_EQ(map->entries[1].key.lexeme, "age");
}

TEST(Parser, LetDecl_EmptyMap) {
    auto stmts = ParseSource("let m = {};");
    ASSERT_EQ(stmts.size(), 1);
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* map = std::get_if<MapLiteralExpr>(&let_decl->initializer->data);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->entries.size(), 0);
}

// --- Function declarations ---

TEST(Parser, FnDecl_NoParams) {
    auto stmts = ParseSource("fn greet() { return \"hello\"; }");
    ASSERT_EQ(stmts.size(), 1);
    auto* fn = std::get_if<FnDecl>(&stmts[0]->data);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name.lexeme, "greet");
    EXPECT_EQ(fn->params.size(), 0);
    EXPECT_EQ(fn->body.size(), 1);
}

TEST(Parser, FnDecl_WithParams) {
    auto stmts = ParseSource("fn add(a, b) { return a + b; }");
    ASSERT_EQ(stmts.size(), 1);
    auto* fn = std::get_if<FnDecl>(&stmts[0]->data);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name.lexeme, "add");
    ASSERT_EQ(fn->params.size(), 2);
    EXPECT_EQ(fn->params[0].lexeme, "a");
    EXPECT_EQ(fn->params[1].lexeme, "b");
}

// --- Expressions ---

TEST(Parser, BinaryExpr_Addition) {
    auto stmts = ParseSource("1 + 2;");
    ASSERT_EQ(stmts.size(), 1);
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    ASSERT_NE(expr_stmt, nullptr);
    auto* binary = std::get_if<BinaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(binary, nullptr);
    EXPECT_EQ(binary->op.type, TokenType::kPlus);
}

TEST(Parser, BinaryExpr_Precedence) {
    // 1 + 2 * 3 should be 1 + (2 * 3)
    auto stmts = ParseSource("1 + 2 * 3;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* add = std::get_if<BinaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op.type, TokenType::kPlus);
    // Right side should be multiplication
    auto* mul = std::get_if<BinaryExpr>(&add->right->data);
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->op.type, TokenType::kStar);
}

TEST(Parser, UnaryExpr_Negation) {
    auto stmts = ParseSource("-x;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* unary = std::get_if<UnaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op.type, TokenType::kMinus);
}

TEST(Parser, UnaryExpr_Not) {
    auto stmts = ParseSource("!flag;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* unary = std::get_if<UnaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op.type, TokenType::kNot);
}

TEST(Parser, CallExpr_NoArgs) {
    auto stmts = ParseSource("foo();");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* call = std::get_if<CallExpr>(&expr_stmt->expression->data);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->arguments.size(), 0);
}

TEST(Parser, CallExpr_WithArgs) {
    auto stmts = ParseSource("foo(1, \"bar\");");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* call = std::get_if<CallExpr>(&expr_stmt->expression->data);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->arguments.size(), 2);
}

TEST(Parser, MemberAccess) {
    auto stmts = ParseSource("obj.name;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* member = std::get_if<MemberAccessExpr>(&expr_stmt->expression->data);
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member.lexeme, "name");
}

TEST(Parser, MemberAccessChain) {
    auto stmts = ParseSource("req.body.plate;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* outer = std::get_if<MemberAccessExpr>(&expr_stmt->expression->data);
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(outer->member.lexeme, "plate");
    auto* inner = std::get_if<MemberAccessExpr>(&outer->object->data);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->member.lexeme, "body");
}

TEST(Parser, MethodCall) {
    auto stmts = ParseSource("items.push(4);");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* call = std::get_if<CallExpr>(&expr_stmt->expression->data);
    ASSERT_NE(call, nullptr);
    auto* member = std::get_if<MemberAccessExpr>(&call->callee->data);
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member.lexeme, "push");
}

TEST(Parser, IndexAccess) {
    auto stmts = ParseSource("items[0];");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* index = std::get_if<IndexAccessExpr>(&expr_stmt->expression->data);
    ASSERT_NE(index, nullptr);
}

TEST(Parser, Assignment) {
    auto stmts = ParseSource("x = 42;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* assign = std::get_if<AssignExpr>(&expr_stmt->expression->data);
    ASSERT_NE(assign, nullptr);
}

TEST(Parser, MemberAssignment) {
    auto stmts = ParseSource("user.email = \"alice@example.com\";");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* assign = std::get_if<AssignExpr>(&expr_stmt->expression->data);
    ASSERT_NE(assign, nullptr);
    auto* member = std::get_if<MemberAccessExpr>(&assign->target->data);
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member.lexeme, "email");
}

// --- Control flow ---

TEST(Parser, IfStmt_Simple) {
    auto stmts = ParseSource("if (x > 0) { log(x); }");
    ASSERT_EQ(stmts.size(), 1);
    auto* if_stmt = std::get_if<IfStmt>(&stmts[0]->data);
    ASSERT_NE(if_stmt, nullptr);
    EXPECT_EQ(if_stmt->then_branch.size(), 1);
    EXPECT_EQ(if_stmt->else_branch.size(), 0);
}

TEST(Parser, IfElseStmt) {
    auto stmts = ParseSource("if (x > 0) { log(\"pos\"); } else { log(\"neg\"); }");
    auto* if_stmt = std::get_if<IfStmt>(&stmts[0]->data);
    ASSERT_NE(if_stmt, nullptr);
    EXPECT_EQ(if_stmt->then_branch.size(), 1);
    EXPECT_EQ(if_stmt->else_branch.size(), 1);
}

TEST(Parser, IfElseIfStmt) {
    auto stmts = ParseSource(
        "if (x > 10) { log(\"big\"); } else if (x > 0) { log(\"small\"); } else { log(\"neg\"); "
        "}");
    auto* if_stmt = std::get_if<IfStmt>(&stmts[0]->data);
    ASSERT_NE(if_stmt, nullptr);
    EXPECT_EQ(if_stmt->else_branch.size(), 1);
    // The else branch contains another if statement
    auto* else_if = std::get_if<IfStmt>(&if_stmt->else_branch[0]->data);
    ASSERT_NE(else_if, nullptr);
}

TEST(Parser, ForLoop) {
    auto stmts = ParseSource("for (let item : items) { log(item); }");
    ASSERT_EQ(stmts.size(), 1);
    auto* for_stmt = std::get_if<ForStmt>(&stmts[0]->data);
    ASSERT_NE(for_stmt, nullptr);
    EXPECT_EQ(for_stmt->variable.lexeme, "item");
    EXPECT_EQ(for_stmt->body.size(), 1);
}

TEST(Parser, WhileLoop) {
    auto stmts = ParseSource("while (count > 0) { count = count - 1; }");
    ASSERT_EQ(stmts.size(), 1);
    auto* while_stmt = std::get_if<WhileStmt>(&stmts[0]->data);
    ASSERT_NE(while_stmt, nullptr);
    EXPECT_EQ(while_stmt->body.size(), 1);
}

TEST(Parser, ReturnStmt_WithValue) {
    auto stmts = ParseSource("return 42;");
    auto* ret = std::get_if<ReturnStmt>(&stmts[0]->data);
    ASSERT_NE(ret, nullptr);
    EXPECT_TRUE(ret->value.has_value());
}

TEST(Parser, ReturnStmt_Void) {
    auto stmts = ParseSource("return;");
    auto* ret = std::get_if<ReturnStmt>(&stmts[0]->data);
    ASSERT_NE(ret, nullptr);
    EXPECT_FALSE(ret->value.has_value());
}

TEST(Parser, ReturnStmt_Map) {
    auto stmts = ParseSource("return { status: 200, body: \"ok\" };");
    auto* ret = std::get_if<ReturnStmt>(&stmts[0]->data);
    ASSERT_NE(ret, nullptr);
    EXPECT_TRUE(ret->value.has_value());
    auto* map = std::get_if<MapLiteralExpr>(&ret->value.value()->data);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->entries.size(), 2);
}

// --- Lambda ---

TEST(Parser, Lambda_NoCapture) {
    auto stmts = ParseSource("let add = [](a, b) { return a + b; };");
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* lambda = std::get_if<LambdaExpr>(&let_decl->initializer->data);
    ASSERT_NE(lambda, nullptr);
    EXPECT_EQ(lambda->captures.size(), 0);
    EXPECT_EQ(lambda->params.size(), 2);
    EXPECT_EQ(lambda->params[0].lexeme, "a");
    EXPECT_EQ(lambda->params[1].lexeme, "b");
}

TEST(Parser, Lambda_WithCapture) {
    auto stmts = ParseSource("let check = [threshold](val) { return val > threshold; };");
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* lambda = std::get_if<LambdaExpr>(&let_decl->initializer->data);
    ASSERT_NE(lambda, nullptr);
    ASSERT_EQ(lambda->captures.size(), 1);
    EXPECT_EQ(lambda->captures[0].lexeme, "threshold");
    ASSERT_EQ(lambda->params.size(), 1);
    EXPECT_EQ(lambda->params[0].lexeme, "val");
}

// --- Match expression ---

TEST(Parser, MatchExpr) {
    auto stmts = ParseSource(
        "let r = match (x) { \"a\" => 1, \"b\" => 2, _ => 0 };");
    auto* let_decl = std::get_if<LetDecl>(&stmts[0]->data);
    ASSERT_NE(let_decl, nullptr);
    auto* match_expr = std::get_if<MatchExpr>(&let_decl->initializer->data);
    ASSERT_NE(match_expr, nullptr);
    ASSERT_EQ(match_expr->arms.size(), 3);
    EXPECT_NE(match_expr->arms[0].pattern, nullptr);   // "a"
    EXPECT_NE(match_expr->arms[1].pattern, nullptr);   // "b"
    EXPECT_EQ(match_expr->arms[2].pattern, nullptr);    // _ (wildcard)
}

// --- Logical operators ---

TEST(Parser, LogicalAnd) {
    auto stmts = ParseSource("a && b;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* binary = std::get_if<BinaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(binary, nullptr);
    EXPECT_EQ(binary->op.type, TokenType::kAnd);
}

TEST(Parser, LogicalOr) {
    auto stmts = ParseSource("a || b;");
    auto* expr_stmt = std::get_if<ExprStmt>(&stmts[0]->data);
    auto* binary = std::get_if<BinaryExpr>(&expr_stmt->expression->data);
    ASSERT_NE(binary, nullptr);
    EXPECT_EQ(binary->op.type, TokenType::kOr);
}

// --- Complex expressions from spec ---

TEST(Parser, FullFunctionWithDbQuery) {
    auto stmts = ParseSource(R"(
        fn handle_register(req) {
            let existing = db.FindOne("cars", { plate: req.body.plate });
            if (existing != null) {
                return { status: 409, body: "Already registered" };
            }
            return { status: 201, body: { plate: req.body.plate } };
        }
    )");
    ASSERT_EQ(stmts.size(), 1);
    auto* fn = std::get_if<FnDecl>(&stmts[0]->data);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name.lexeme, "handle_register");
    EXPECT_EQ(fn->params.size(), 1);
    EXPECT_GE(fn->body.size(), 2);
}

TEST(Parser, MultipleStatements) {
    auto stmts = ParseSource("let x = 1; let y = 2; let z = x + y;");
    EXPECT_EQ(stmts.size(), 3);
}

// --- Error cases ---

TEST(Parser, Error_MissingSemicolon) {
    auto err = ParseSourceError("let x = 42");
    EXPECT_FALSE(err.message.empty());
}

TEST(Parser, Error_MissingClosingBrace) {
    auto err = ParseSourceError("fn foo() { return 1;");
    EXPECT_FALSE(err.message.empty());
}

TEST(Parser, Error_MissingClosingParen) {
    auto err = ParseSourceError("foo(1, 2;");
    EXPECT_FALSE(err.message.empty());
}

}  // namespace
}  // namespace lang
