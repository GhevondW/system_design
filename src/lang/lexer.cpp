#include "lang/lexer.h"

#include <unordered_map>

namespace lang {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;
    while (true) {
        auto token = NextToken();
        tokens.push_back(token);
        if (token.type == TokenType::kEof || token.type == TokenType::kError) {
            break;
        }
    }
    return tokens;
}

Token Lexer::NextToken() {
    SkipWhitespace();

    if (IsAtEnd()) {
        return MakeToken(TokenType::kEof, "");
    }

    token_start_line_ = line_;
    token_start_column_ = column_;

    char c = Advance();

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return ScanIdentifierOrKeyword();
    }

    // Numbers
    if (std::isdigit(c)) {
        return ScanNumber();
    }

    switch (c) {
        // Strings
        case '"': return ScanString();

        // Single-character tokens
        case '(': return MakeToken(TokenType::kLeftParen);
        case ')': return MakeToken(TokenType::kRightParen);
        case '{': return MakeToken(TokenType::kLeftBrace);
        case '}': return MakeToken(TokenType::kRightBrace);
        case '[': return MakeToken(TokenType::kLeftBracket);
        case ']': return MakeToken(TokenType::kRightBracket);
        case ';': return MakeToken(TokenType::kSemicolon);
        case ':': return MakeToken(TokenType::kColon);
        case ',': return MakeToken(TokenType::kComma);
        case '.': return MakeToken(TokenType::kDot);
        case '+': return MakeToken(TokenType::kPlus);
        case '-': return MakeToken(TokenType::kMinus);
        case '*': return MakeToken(TokenType::kStar);
        case '%': return MakeToken(TokenType::kPercent);

        // Slash or comment
        case '/':
            if (Match('/')) {
                SkipLineComment();
                return NextToken();
            }
            if (Match('*')) {
                SkipBlockComment();
                return NextToken();
            }
            return MakeToken(TokenType::kSlash);

        // Two-character tokens
        case '=':
            if (Match('=')) return MakeToken(TokenType::kEqual, "==");
            if (Match('>')) return MakeToken(TokenType::kArrow, "=>");
            return MakeToken(TokenType::kAssign);

        case '!':
            if (Match('=')) return MakeToken(TokenType::kNotEqual, "!=");
            return MakeToken(TokenType::kNot);

        case '<':
            if (Match('=')) return MakeToken(TokenType::kLessEqual, "<=");
            return MakeToken(TokenType::kLess);

        case '>':
            if (Match('=')) return MakeToken(TokenType::kGreaterEqual, ">=");
            return MakeToken(TokenType::kGreater);

        case '&':
            if (Match('&')) return MakeToken(TokenType::kAnd, "&&");
            return ErrorToken("Unexpected character '&'. Did you mean '&&'?");

        case '|':
            if (Match('|')) return MakeToken(TokenType::kOr, "||");
            return ErrorToken("Unexpected character '|'. Did you mean '||'?");

        default:
            return ErrorToken(std::string("Unexpected character '") + c + "'");
    }
}

// --- Private helpers ---

bool Lexer::IsAtEnd() const {
    return pos_ >= source_.size();
}

char Lexer::Peek() const {
    if (IsAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::PeekNext() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::Advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::Match(char expected) {
    if (IsAtEnd() || source_[pos_] != expected) return false;
    Advance();
    return true;
}

void Lexer::SkipWhitespace() {
    while (!IsAtEnd()) {
        char c = Peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            Advance();
        } else {
            break;
        }
    }
}

void Lexer::SkipLineComment() {
    while (!IsAtEnd() && Peek() != '\n') {
        Advance();
    }
}

void Lexer::SkipBlockComment() {
    int depth = 1;
    while (!IsAtEnd() && depth > 0) {
        if (Peek() == '/' && PeekNext() == '*') {
            Advance();
            Advance();
            depth++;
        } else if (Peek() == '*' && PeekNext() == '/') {
            Advance();
            Advance();
            depth--;
        } else {
            Advance();
        }
    }
}

Token Lexer::MakeToken(TokenType type, std::string lexeme) const {
    return Token{type, std::move(lexeme), token_start_line_, token_start_column_};
}

Token Lexer::MakeToken(TokenType type) const {
    return Token{type, std::string(1, source_[pos_ - 1]), token_start_line_, token_start_column_};
}

Token Lexer::ErrorToken(std::string message) const {
    return Token{TokenType::kError, std::move(message), token_start_line_, token_start_column_};
}

Token Lexer::ScanString() {
    std::string value;
    while (!IsAtEnd() && Peek() != '"') {
        if (Peek() == '\n') {
            return ErrorToken("Unterminated string (newline in string literal)");
        }
        if (Peek() == '\\') {
            Advance();
            if (IsAtEnd()) return ErrorToken("Unterminated escape sequence");
            char escaped = Advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                default:
                    return ErrorToken(std::string("Invalid escape sequence '\\") + escaped + "'");
            }
        } else {
            value += Advance();
        }
    }

    if (IsAtEnd()) {
        return ErrorToken("Unterminated string");
    }

    Advance();  // closing "
    return MakeToken(TokenType::kString, std::move(value));
}

Token Lexer::ScanNumber() {
    size_t start = pos_ - 1;  // we already consumed the first digit
    bool is_float = false;

    while (!IsAtEnd() && std::isdigit(Peek())) {
        Advance();
    }

    if (Peek() == '.' && std::isdigit(PeekNext())) {
        is_float = true;
        Advance();  // consume '.'
        while (!IsAtEnd() && std::isdigit(Peek())) {
            Advance();
        }
    }

    std::string lexeme = source_.substr(start, pos_ - start);
    return MakeToken(is_float ? TokenType::kFloat : TokenType::kInteger, std::move(lexeme));
}

Token Lexer::ScanIdentifierOrKeyword() {
    size_t start = pos_ - 1;  // we already consumed the first char

    while (!IsAtEnd() && (std::isalnum(Peek()) || Peek() == '_')) {
        Advance();
    }

    std::string lexeme = source_.substr(start, pos_ - start);
    TokenType type = LookupKeyword(lexeme);
    return MakeToken(type, std::move(lexeme));
}

TokenType Lexer::LookupKeyword(std::string_view word) {
    static const std::unordered_map<std::string_view, TokenType> keywords = {
        {"let", TokenType::kLet},       {"fn", TokenType::kFn},
        {"return", TokenType::kReturn}, {"if", TokenType::kIf},
        {"else", TokenType::kElse},     {"for", TokenType::kFor},
        {"while", TokenType::kWhile},   {"match", TokenType::kMatch},
        {"true", TokenType::kTrue},     {"false", TokenType::kFalse},
        {"null", TokenType::kNull},
    };

    auto it = keywords.find(word);
    if (it != keywords.end()) return it->second;

    // Lone underscore is a wildcard
    if (word == "_") return TokenType::kUnderscore;

    return TokenType::kIdentifier;
}

}  // namespace lang
