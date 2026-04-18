#include "lang/lexer.h"

#include <gtest/gtest.h>

namespace lang {
namespace {

// Helper: tokenize and return all tokens (including EOF)
std::vector<Token> Lex(const std::string& source) {
    Lexer lexer(source);
    return lexer.Tokenize();
}

// Helper: tokenize and return types only (excluding EOF)
std::vector<TokenType> LexTypes(const std::string& source) {
    auto tokens = Lex(source);
    std::vector<TokenType> types;
    for (const auto& t : tokens) {
        if (t.type != TokenType::kEof) types.push_back(t.type);
    }
    return types;
}

// --- Basic Tokens ---

TEST(Lexer, EmptySource_ReturnsEof) {
    auto tokens = Lex("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::kEof);
}

TEST(Lexer, WhitespaceOnly_ReturnsEof) {
    auto tokens = Lex("   \t\n\r  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::kEof);
}

// --- Integer Literals ---

TEST(Lexer, IntegerLiteral) {
    auto tokens = Lex("42");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::kInteger);
    EXPECT_EQ(tokens[0].lexeme, "42");
}

TEST(Lexer, IntegerZero) {
    auto tokens = Lex("0");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::kInteger);
    EXPECT_EQ(tokens[0].lexeme, "0");
}

TEST(Lexer, MultipleIntegers) {
    auto types = LexTypes("1 22 333");
    ASSERT_EQ(types.size(), 3);
    EXPECT_EQ(types[0], TokenType::kInteger);
    EXPECT_EQ(types[1], TokenType::kInteger);
    EXPECT_EQ(types[2], TokenType::kInteger);
}

// --- Float Literals ---

TEST(Lexer, FloatLiteral) {
    auto tokens = Lex("3.14");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::kFloat);
    EXPECT_EQ(tokens[0].lexeme, "3.14");
}

TEST(Lexer, FloatWithLeadingDigits) {
    auto tokens = Lex("123.456");
    EXPECT_EQ(tokens[0].type, TokenType::kFloat);
    EXPECT_EQ(tokens[0].lexeme, "123.456");
}

// --- String Literals ---

TEST(Lexer, StringLiteral) {
    auto tokens = Lex("\"hello world\"");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::kString);
    EXPECT_EQ(tokens[0].lexeme, "hello world");
}

TEST(Lexer, EmptyString) {
    auto tokens = Lex("\"\"");
    EXPECT_EQ(tokens[0].type, TokenType::kString);
    EXPECT_EQ(tokens[0].lexeme, "");
}

TEST(Lexer, StringWithEscapes) {
    auto tokens = Lex("\"hello\\nworld\\t!\"");
    EXPECT_EQ(tokens[0].type, TokenType::kString);
    EXPECT_EQ(tokens[0].lexeme, "hello\nworld\t!");
}

TEST(Lexer, StringWithEscapedQuote) {
    auto tokens = Lex("\"say \\\"hi\\\"\"");
    EXPECT_EQ(tokens[0].type, TokenType::kString);
    EXPECT_EQ(tokens[0].lexeme, "say \"hi\"");
}

TEST(Lexer, StringWithBackslash) {
    auto tokens = Lex("\"path\\\\file\"");
    EXPECT_EQ(tokens[0].type, TokenType::kString);
    EXPECT_EQ(tokens[0].lexeme, "path\\file");
}

TEST(Lexer, UnterminatedString) {
    auto tokens = Lex("\"hello");
    EXPECT_EQ(tokens.back().type, TokenType::kError);
}

TEST(Lexer, StringWithNewline) {
    auto tokens = Lex("\"hello\nworld\"");
    EXPECT_EQ(tokens[0].type, TokenType::kError);
}

// --- Keywords ---

TEST(Lexer, Keywords) {
    auto tokens = Lex("let fn return if else for while match true false null");
    auto types = LexTypes("let fn return if else for while match true false null");
    ASSERT_EQ(types.size(), 11);
    EXPECT_EQ(types[0], TokenType::kLet);
    EXPECT_EQ(types[1], TokenType::kFn);
    EXPECT_EQ(types[2], TokenType::kReturn);
    EXPECT_EQ(types[3], TokenType::kIf);
    EXPECT_EQ(types[4], TokenType::kElse);
    EXPECT_EQ(types[5], TokenType::kFor);
    EXPECT_EQ(types[6], TokenType::kWhile);
    EXPECT_EQ(types[7], TokenType::kMatch);
    EXPECT_EQ(types[8], TokenType::kTrue);
    EXPECT_EQ(types[9], TokenType::kFalse);
    EXPECT_EQ(types[10], TokenType::kNull);
}

// --- Identifiers ---

TEST(Lexer, Identifier) {
    auto tokens = Lex("myVar");
    EXPECT_EQ(tokens[0].type, TokenType::kIdentifier);
    EXPECT_EQ(tokens[0].lexeme, "myVar");
}

TEST(Lexer, IdentifierWithUnderscores) {
    auto tokens = Lex("my_var_123");
    EXPECT_EQ(tokens[0].type, TokenType::kIdentifier);
    EXPECT_EQ(tokens[0].lexeme, "my_var_123");
}

TEST(Lexer, IdentifierStartingWithUnderscore) {
    auto tokens = Lex("_private");
    EXPECT_EQ(tokens[0].type, TokenType::kIdentifier);
    EXPECT_EQ(tokens[0].lexeme, "_private");
}

TEST(Lexer, LoneUnderscoreIsWildcard) {
    auto tokens = Lex("_");
    EXPECT_EQ(tokens[0].type, TokenType::kUnderscore);
}

TEST(Lexer, KeywordPrefixIsIdentifier) {
    auto tokens = Lex("letter");
    EXPECT_EQ(tokens[0].type, TokenType::kIdentifier);
    EXPECT_EQ(tokens[0].lexeme, "letter");
}

// --- Operators ---

TEST(Lexer, SingleCharOperators) {
    auto types = LexTypes("+ - * / %");
    ASSERT_EQ(types.size(), 5);
    EXPECT_EQ(types[0], TokenType::kPlus);
    EXPECT_EQ(types[1], TokenType::kMinus);
    EXPECT_EQ(types[2], TokenType::kStar);
    EXPECT_EQ(types[3], TokenType::kSlash);
    EXPECT_EQ(types[4], TokenType::kPercent);
}

TEST(Lexer, ComparisonOperators) {
    auto types = LexTypes("== != < <= > >=");
    ASSERT_EQ(types.size(), 6);
    EXPECT_EQ(types[0], TokenType::kEqual);
    EXPECT_EQ(types[1], TokenType::kNotEqual);
    EXPECT_EQ(types[2], TokenType::kLess);
    EXPECT_EQ(types[3], TokenType::kLessEqual);
    EXPECT_EQ(types[4], TokenType::kGreater);
    EXPECT_EQ(types[5], TokenType::kGreaterEqual);
}

TEST(Lexer, LogicalOperators) {
    auto types = LexTypes("&& || !");
    ASSERT_EQ(types.size(), 3);
    EXPECT_EQ(types[0], TokenType::kAnd);
    EXPECT_EQ(types[1], TokenType::kOr);
    EXPECT_EQ(types[2], TokenType::kNot);
}

TEST(Lexer, AssignAndArrow) {
    auto types = LexTypes("= =>");
    ASSERT_EQ(types.size(), 2);
    EXPECT_EQ(types[0], TokenType::kAssign);
    EXPECT_EQ(types[1], TokenType::kArrow);
}

// --- Punctuation ---

TEST(Lexer, Punctuation) {
    auto types = LexTypes("( ) { } [ ] ; : , .");
    ASSERT_EQ(types.size(), 10);
    EXPECT_EQ(types[0], TokenType::kLeftParen);
    EXPECT_EQ(types[1], TokenType::kRightParen);
    EXPECT_EQ(types[2], TokenType::kLeftBrace);
    EXPECT_EQ(types[3], TokenType::kRightBrace);
    EXPECT_EQ(types[4], TokenType::kLeftBracket);
    EXPECT_EQ(types[5], TokenType::kRightBracket);
    EXPECT_EQ(types[6], TokenType::kSemicolon);
    EXPECT_EQ(types[7], TokenType::kColon);
    EXPECT_EQ(types[8], TokenType::kComma);
    EXPECT_EQ(types[9], TokenType::kDot);
}

// --- Comments ---

TEST(Lexer, LineComment) {
    auto types = LexTypes("42 // this is a comment\n43");
    ASSERT_EQ(types.size(), 2);
    EXPECT_EQ(types[0], TokenType::kInteger);
    EXPECT_EQ(types[1], TokenType::kInteger);
}

TEST(Lexer, BlockComment) {
    auto types = LexTypes("42 /* block comment */ 43");
    ASSERT_EQ(types.size(), 2);
    EXPECT_EQ(types[0], TokenType::kInteger);
    EXPECT_EQ(types[1], TokenType::kInteger);
}

TEST(Lexer, NestedBlockComment) {
    auto types = LexTypes("42 /* outer /* inner */ still comment */ 43");
    ASSERT_EQ(types.size(), 2);
    EXPECT_EQ(types[0], TokenType::kInteger);
    EXPECT_EQ(types[1], TokenType::kInteger);
}

// --- Line/Column Tracking ---

TEST(Lexer, LineTracking) {
    auto tokens = Lex("let\nx\n=\n42");
    EXPECT_EQ(tokens[0].line, 1);  // let
    EXPECT_EQ(tokens[1].line, 2);  // x
    EXPECT_EQ(tokens[2].line, 3);  // =
    EXPECT_EQ(tokens[3].line, 4);  // 42
}

TEST(Lexer, ColumnTracking) {
    auto tokens = Lex("let x = 42");
    EXPECT_EQ(tokens[0].column, 1);  // let
    EXPECT_EQ(tokens[1].column, 5);  // x
    EXPECT_EQ(tokens[2].column, 7);  // =
    EXPECT_EQ(tokens[3].column, 9);  // 42
}

// --- Error Cases ---

TEST(Lexer, SingleAmpersandIsError) {
    auto tokens = Lex("&");
    EXPECT_EQ(tokens[0].type, TokenType::kError);
}

TEST(Lexer, SinglePipeIsError) {
    auto tokens = Lex("|");
    EXPECT_EQ(tokens[0].type, TokenType::kError);
}

TEST(Lexer, UnexpectedCharacter) {
    auto tokens = Lex("@");
    EXPECT_EQ(tokens[0].type, TokenType::kError);
}

// --- Full SysLang Expressions ---

TEST(Lexer, LetDeclaration) {
    auto types = LexTypes("let name = \"car_reg\";");
    ASSERT_EQ(types.size(), 5);
    EXPECT_EQ(types[0], TokenType::kLet);
    EXPECT_EQ(types[1], TokenType::kIdentifier);
    EXPECT_EQ(types[2], TokenType::kAssign);
    EXPECT_EQ(types[3], TokenType::kString);
    EXPECT_EQ(types[4], TokenType::kSemicolon);
}

TEST(Lexer, FunctionDeclaration) {
    auto types = LexTypes("fn validate_plate(plate) { return true; }");
    ASSERT_EQ(types.size(), 10);
    EXPECT_EQ(types[0], TokenType::kFn);
    EXPECT_EQ(types[1], TokenType::kIdentifier);
    EXPECT_EQ(types[2], TokenType::kLeftParen);
    EXPECT_EQ(types[3], TokenType::kIdentifier);
    EXPECT_EQ(types[4], TokenType::kRightParen);
    EXPECT_EQ(types[5], TokenType::kLeftBrace);
    EXPECT_EQ(types[6], TokenType::kReturn);
    EXPECT_EQ(types[7], TokenType::kTrue);
    EXPECT_EQ(types[8], TokenType::kSemicolon);
    EXPECT_EQ(types[9], TokenType::kRightBrace);
}

TEST(Lexer, LambdaExpression) {
    auto types = LexTypes("[](a, b) { return a + b; }");
    ASSERT_EQ(types.size(), 14);
    EXPECT_EQ(types[0], TokenType::kLeftBracket);
    EXPECT_EQ(types[1], TokenType::kRightBracket);
    EXPECT_EQ(types[2], TokenType::kLeftParen);
    EXPECT_EQ(types[3], TokenType::kIdentifier);
    EXPECT_EQ(types[4], TokenType::kComma);
    EXPECT_EQ(types[5], TokenType::kIdentifier);
    EXPECT_EQ(types[6], TokenType::kRightParen);
    EXPECT_EQ(types[7], TokenType::kLeftBrace);
    EXPECT_EQ(types[8], TokenType::kReturn);
    EXPECT_EQ(types[9], TokenType::kIdentifier);
    EXPECT_EQ(types[10], TokenType::kPlus);
    EXPECT_EQ(types[11], TokenType::kIdentifier);
    EXPECT_EQ(types[12], TokenType::kSemicolon);
    EXPECT_EQ(types[13], TokenType::kRightBrace);
}

TEST(Lexer, MatchExpression) {
    auto types = LexTypes("match (req.method) { \"GET\" => x, _ => y }");
    ASSERT_EQ(types.size(), 15);
    EXPECT_EQ(types[0], TokenType::kMatch);
    EXPECT_EQ(types[1], TokenType::kLeftParen);
    EXPECT_EQ(types[2], TokenType::kIdentifier);
    EXPECT_EQ(types[3], TokenType::kDot);
    EXPECT_EQ(types[4], TokenType::kIdentifier);
    EXPECT_EQ(types[5], TokenType::kRightParen);
    EXPECT_EQ(types[6], TokenType::kLeftBrace);
    EXPECT_EQ(types[7], TokenType::kString);
    EXPECT_EQ(types[8], TokenType::kArrow);
    EXPECT_EQ(types[9], TokenType::kIdentifier);
    EXPECT_EQ(types[10], TokenType::kComma);
    EXPECT_EQ(types[11], TokenType::kUnderscore);
    EXPECT_EQ(types[12], TokenType::kArrow);
    EXPECT_EQ(types[13], TokenType::kIdentifier);
    EXPECT_EQ(types[14], TokenType::kRightBrace);
}

TEST(Lexer, ListLiteral) {
    auto types = LexTypes("[1, 2, 3]");
    ASSERT_EQ(types.size(), 7);
    EXPECT_EQ(types[0], TokenType::kLeftBracket);
    EXPECT_EQ(types[1], TokenType::kInteger);
    EXPECT_EQ(types[2], TokenType::kComma);
    EXPECT_EQ(types[3], TokenType::kInteger);
    EXPECT_EQ(types[4], TokenType::kComma);
    EXPECT_EQ(types[5], TokenType::kInteger);
    EXPECT_EQ(types[6], TokenType::kRightBracket);
}

TEST(Lexer, MapLiteral) {
    auto types = LexTypes("{ name: \"Alice\", age: 30 }");
    ASSERT_EQ(types.size(), 9);
    EXPECT_EQ(types[0], TokenType::kLeftBrace);
    EXPECT_EQ(types[1], TokenType::kIdentifier);
    EXPECT_EQ(types[2], TokenType::kColon);
    EXPECT_EQ(types[3], TokenType::kString);
    EXPECT_EQ(types[4], TokenType::kComma);
    EXPECT_EQ(types[5], TokenType::kIdentifier);
    EXPECT_EQ(types[6], TokenType::kColon);
    EXPECT_EQ(types[7], TokenType::kInteger);
    EXPECT_EQ(types[8], TokenType::kRightBrace);
}

TEST(Lexer, ForLoop) {
    // for (let plate : plates) { log(plate); }
    // for ( let plate : plates ) { log ( plate ) ; }
    auto types = LexTypes("for (let plate : plates) { log(plate); }");
    ASSERT_EQ(types.size(), 14);
    EXPECT_EQ(types[0], TokenType::kFor);
    EXPECT_EQ(types[1], TokenType::kLeftParen);
    EXPECT_EQ(types[2], TokenType::kLet);
    EXPECT_EQ(types[3], TokenType::kIdentifier);   // plate
    EXPECT_EQ(types[4], TokenType::kColon);
    EXPECT_EQ(types[5], TokenType::kIdentifier);   // plates
    EXPECT_EQ(types[6], TokenType::kRightParen);
    EXPECT_EQ(types[7], TokenType::kLeftBrace);
    EXPECT_EQ(types[8], TokenType::kIdentifier);   // log
    EXPECT_EQ(types[9], TokenType::kLeftParen);
    EXPECT_EQ(types[10], TokenType::kIdentifier);  // plate
    EXPECT_EQ(types[11], TokenType::kRightParen);
    EXPECT_EQ(types[12], TokenType::kSemicolon);
    EXPECT_EQ(types[13], TokenType::kRightBrace);
}

TEST(Lexer, MemberAccessChain) {
    auto types = LexTypes("req.body.plate");
    ASSERT_EQ(types.size(), 5);
    EXPECT_EQ(types[0], TokenType::kIdentifier);
    EXPECT_EQ(types[1], TokenType::kDot);
    EXPECT_EQ(types[2], TokenType::kIdentifier);
    EXPECT_EQ(types[3], TokenType::kDot);
    EXPECT_EQ(types[4], TokenType::kIdentifier);
}

TEST(Lexer, LambdaWithCapture) {
    // [threshold](val) { return val > threshold; }
    auto types = LexTypes("[threshold](val) { return val > threshold; }");
    ASSERT_EQ(types.size(), 13);
    EXPECT_EQ(types[0], TokenType::kLeftBracket);
    EXPECT_EQ(types[1], TokenType::kIdentifier);  // threshold
    EXPECT_EQ(types[2], TokenType::kRightBracket);
    EXPECT_EQ(types[3], TokenType::kLeftParen);
    EXPECT_EQ(types[4], TokenType::kIdentifier);   // val
    EXPECT_EQ(types[5], TokenType::kRightParen);
    EXPECT_EQ(types[6], TokenType::kLeftBrace);
    EXPECT_EQ(types[7], TokenType::kReturn);
    EXPECT_EQ(types[8], TokenType::kIdentifier);   // val
    EXPECT_EQ(types[9], TokenType::kGreater);
    EXPECT_EQ(types[10], TokenType::kIdentifier);  // threshold
    EXPECT_EQ(types[11], TokenType::kSemicolon);
    EXPECT_EQ(types[12], TokenType::kRightBrace);
}

// --- TokenTypeName ---

TEST(Token, TypeNameCoversAllTypes) {
    EXPECT_EQ(TokenTypeName(TokenType::kInteger), "Integer");
    EXPECT_EQ(TokenTypeName(TokenType::kFloat), "Float");
    EXPECT_EQ(TokenTypeName(TokenType::kString), "String");
    EXPECT_EQ(TokenTypeName(TokenType::kTrue), "True");
    EXPECT_EQ(TokenTypeName(TokenType::kFalse), "False");
    EXPECT_EQ(TokenTypeName(TokenType::kNull), "Null");
    EXPECT_EQ(TokenTypeName(TokenType::kIdentifier), "Identifier");
    EXPECT_EQ(TokenTypeName(TokenType::kLet), "Let");
    EXPECT_EQ(TokenTypeName(TokenType::kFn), "Fn");
    EXPECT_EQ(TokenTypeName(TokenType::kReturn), "Return");
    EXPECT_EQ(TokenTypeName(TokenType::kIf), "If");
    EXPECT_EQ(TokenTypeName(TokenType::kElse), "Else");
    EXPECT_EQ(TokenTypeName(TokenType::kFor), "For");
    EXPECT_EQ(TokenTypeName(TokenType::kWhile), "While");
    EXPECT_EQ(TokenTypeName(TokenType::kMatch), "Match");
    EXPECT_EQ(TokenTypeName(TokenType::kArrow), "Arrow");
    EXPECT_EQ(TokenTypeName(TokenType::kUnderscore), "Underscore");
    EXPECT_EQ(TokenTypeName(TokenType::kEof), "Eof");
    EXPECT_EQ(TokenTypeName(TokenType::kError), "Error");
}

}  // namespace
}  // namespace lang
