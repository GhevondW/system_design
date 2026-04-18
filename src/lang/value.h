#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lang {

// Forward declaration for recursive types
struct ScriptValue;

// Type aliases for composite types
using ScriptList = std::vector<ScriptValue>;
using ScriptMap = std::map<std::string, ScriptValue>;
using NativeFunction = std::function<ScriptValue(std::vector<ScriptValue>)>;

// Null type
struct NullValue {
    bool operator==(const NullValue&) const = default;
};

// Error type
struct ErrorValue {
    std::string message;
    bool operator==(const ErrorValue&) const = default;
};

// Function type (user-defined — will be extended in interpreter step)
struct FunctionValue {
    std::string name;
    std::vector<std::string> params;
    // Body will be an AST node pointer, added in Step 3
    // For now, just metadata
    bool operator==(const FunctionValue&) const = default;
};

// The core dynamic value type
struct ScriptValue {
    using Variant = std::variant<NullValue, bool, int64_t, double, std::string,
                                 std::shared_ptr<ScriptList>, std::shared_ptr<ScriptMap>,
                                 FunctionValue, NativeFunction, ErrorValue>;

    Variant data;

    // Default: null
    ScriptValue() : data(NullValue{}) {}

    // Constructors for each type
    ScriptValue(NullValue) : data(NullValue{}) {}
    ScriptValue(bool val) : data(val) {}
    ScriptValue(int val) : data(static_cast<int64_t>(val)) {}
    ScriptValue(int64_t val) : data(val) {}
    ScriptValue(double val) : data(val) {}
    ScriptValue(const char* val) : data(std::string(val)) {}
    ScriptValue(std::string val) : data(std::move(val)) {}
    ScriptValue(ScriptList val) : data(std::make_shared<ScriptList>(std::move(val))) {}
    ScriptValue(ScriptMap val) : data(std::make_shared<ScriptMap>(std::move(val))) {}
    ScriptValue(FunctionValue val) : data(std::move(val)) {}
    ScriptValue(NativeFunction val) : data(std::move(val)) {}
    ScriptValue(ErrorValue val) : data(std::move(val)) {}

    // Type queries
    [[nodiscard]] bool IsNull() const;
    [[nodiscard]] bool IsBool() const;
    [[nodiscard]] bool IsInt() const;
    [[nodiscard]] bool IsFloat() const;
    [[nodiscard]] bool IsNumber() const;
    [[nodiscard]] bool IsString() const;
    [[nodiscard]] bool IsList() const;
    [[nodiscard]] bool IsMap() const;
    [[nodiscard]] bool IsFunction() const;
    [[nodiscard]] bool IsNativeFunction() const;
    [[nodiscard]] bool IsCallable() const;
    [[nodiscard]] bool IsError() const;

    // Accessors (undefined behavior if wrong type — check first)
    [[nodiscard]] bool AsBool() const;
    [[nodiscard]] int64_t AsInt() const;
    [[nodiscard]] double AsFloat() const;
    [[nodiscard]] double AsNumber() const;
    [[nodiscard]] const std::string& AsString() const;
    [[nodiscard]] ScriptList& AsList() const;
    [[nodiscard]] ScriptMap& AsMap() const;
    [[nodiscard]] const FunctionValue& AsFunction() const;
    [[nodiscard]] const NativeFunction& AsNativeFunction() const;
    [[nodiscard]] const ErrorValue& AsError() const;

    // Truthiness: null and false are falsy, everything else is truthy
    [[nodiscard]] bool IsTruthy() const;

    // Type name for error messages
    [[nodiscard]] std::string_view TypeName() const;

    // String representation
    [[nodiscard]] std::string ToString() const;

    // Comparison
    [[nodiscard]] bool Equals(const ScriptValue& other) const;

    // Arithmetic — returns error value on type mismatch
    [[nodiscard]] static ScriptValue Add(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue Subtract(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue Multiply(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue Divide(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue Modulo(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue Negate(const ScriptValue& val);

    // Comparison operators — return bool values, error on incompatible types
    [[nodiscard]] static ScriptValue LessThan(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue LessEqual(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue GreaterThan(const ScriptValue& lhs, const ScriptValue& rhs);
    [[nodiscard]] static ScriptValue GreaterEqual(const ScriptValue& lhs, const ScriptValue& rhs);

    // Factory helpers
    [[nodiscard]] static ScriptValue Null();
    [[nodiscard]] static ScriptValue Error(std::string message);
    [[nodiscard]] static ScriptValue List(ScriptList elements = {});
    [[nodiscard]] static ScriptValue Map(ScriptMap entries = {});
};

}  // namespace lang
