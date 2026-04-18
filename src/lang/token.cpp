#include "lang/token.h"

namespace lang {

std::string_view TokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::kInteger: return "Integer";
        case TokenType::kFloat: return "Float";
        case TokenType::kString: return "String";
        case TokenType::kTrue: return "True";
        case TokenType::kFalse: return "False";
        case TokenType::kNull: return "Null";
        case TokenType::kIdentifier: return "Identifier";
        case TokenType::kLet: return "Let";
        case TokenType::kFn: return "Fn";
        case TokenType::kReturn: return "Return";
        case TokenType::kIf: return "If";
        case TokenType::kElse: return "Else";
        case TokenType::kFor: return "For";
        case TokenType::kWhile: return "While";
        case TokenType::kMatch: return "Match";
        case TokenType::kPlus: return "Plus";
        case TokenType::kMinus: return "Minus";
        case TokenType::kStar: return "Star";
        case TokenType::kSlash: return "Slash";
        case TokenType::kPercent: return "Percent";
        case TokenType::kAssign: return "Assign";
        case TokenType::kEqual: return "Equal";
        case TokenType::kNotEqual: return "NotEqual";
        case TokenType::kLess: return "Less";
        case TokenType::kLessEqual: return "LessEqual";
        case TokenType::kGreater: return "Greater";
        case TokenType::kGreaterEqual: return "GreaterEqual";
        case TokenType::kAnd: return "And";
        case TokenType::kOr: return "Or";
        case TokenType::kNot: return "Not";
        case TokenType::kLeftParen: return "LeftParen";
        case TokenType::kRightParen: return "RightParen";
        case TokenType::kLeftBrace: return "LeftBrace";
        case TokenType::kRightBrace: return "RightBrace";
        case TokenType::kLeftBracket: return "LeftBracket";
        case TokenType::kRightBracket: return "RightBracket";
        case TokenType::kSemicolon: return "Semicolon";
        case TokenType::kColon: return "Colon";
        case TokenType::kComma: return "Comma";
        case TokenType::kDot: return "Dot";
        case TokenType::kArrow: return "Arrow";
        case TokenType::kUnderscore: return "Underscore";
        case TokenType::kEof: return "Eof";
        case TokenType::kError: return "Error";
    }
    return "Unknown";
}

}  // namespace lang
