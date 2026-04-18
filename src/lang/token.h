#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lang {

enum class TokenType : uint8_t {
    // Literals
    kInteger,
    kFloat,
    kString,
    kTrue,
    kFalse,
    kNull,

    // Identifiers
    kIdentifier,

    // Keywords
    kLet,
    kFn,
    kReturn,
    kIf,
    kElse,
    kFor,
    kWhile,
    kMatch,

    // Operators
    kPlus,         // +
    kMinus,        // -
    kStar,         // *
    kSlash,        // /
    kPercent,      // %
    kAssign,       // =
    kEqual,        // ==
    kNotEqual,     // !=
    kLess,         // <
    kLessEqual,    // <=
    kGreater,      // >
    kGreaterEqual, // >=
    kAnd,          // &&
    kOr,           // ||
    kNot,          // !

    // Punctuation
    kLeftParen,    // (
    kRightParen,   // )
    kLeftBrace,    // {
    kRightBrace,   // }
    kLeftBracket,  // [
    kRightBracket, // ]
    kSemicolon,    // ;
    kColon,        // :
    kComma,        // ,
    kDot,          // .
    kArrow,        // =>
    kUnderscore,   // _ (wildcard in match)

    // Special
    kEof,
    kError,
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

[[nodiscard]] std::string_view TokenTypeName(TokenType type);

}  // namespace lang
