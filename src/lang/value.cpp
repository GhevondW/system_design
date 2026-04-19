#include "lang/value.h"

#include <sstream>

namespace lang {

// --- Type queries ---

bool ScriptValue::IsNull() const {
    return std::holds_alternative<NullValue>(data);
}

bool ScriptValue::IsBool() const {
    return std::holds_alternative<bool>(data);
}

bool ScriptValue::IsInt() const {
    return std::holds_alternative<int64_t>(data);
}

bool ScriptValue::IsFloat() const {
    return std::holds_alternative<double>(data);
}

bool ScriptValue::IsNumber() const {
    return IsInt() || IsFloat();
}

bool ScriptValue::IsString() const {
    return std::holds_alternative<std::string>(data);
}

bool ScriptValue::IsList() const {
    return std::holds_alternative<std::shared_ptr<ScriptList>>(data);
}

bool ScriptValue::IsMap() const {
    return std::holds_alternative<std::shared_ptr<ScriptMap>>(data);
}

bool ScriptValue::IsFunction() const {
    return std::holds_alternative<FunctionValue>(data);
}

bool ScriptValue::IsNativeFunction() const {
    return std::holds_alternative<NativeFunction>(data);
}

bool ScriptValue::IsCallable() const {
    return IsFunction() || IsNativeFunction();
}

bool ScriptValue::IsError() const {
    return std::holds_alternative<ErrorValue>(data);
}

// --- Accessors ---

bool ScriptValue::AsBool() const {
    return std::get<bool>(data);
}

int64_t ScriptValue::AsInt() const {
    return std::get<int64_t>(data);
}

double ScriptValue::AsFloat() const {
    return std::get<double>(data);
}

double ScriptValue::AsNumber() const {
    if (IsInt()) return static_cast<double>(AsInt());
    return AsFloat();
}

const std::string& ScriptValue::AsString() const {
    return std::get<std::string>(data);
}

ScriptList& ScriptValue::AsList() const {
    return *std::get<std::shared_ptr<ScriptList>>(data);
}

ScriptMap& ScriptValue::AsMap() const {
    return *std::get<std::shared_ptr<ScriptMap>>(data);
}

const FunctionValue& ScriptValue::AsFunction() const {
    return std::get<FunctionValue>(data);
}

const NativeFunction& ScriptValue::AsNativeFunction() const {
    return std::get<NativeFunction>(data);
}

const ErrorValue& ScriptValue::AsError() const {
    return std::get<ErrorValue>(data);
}

// --- Truthiness ---

bool ScriptValue::IsTruthy() const {
    if (IsNull()) return false;
    if (IsBool()) return AsBool();
    if (IsInt()) return AsInt() != 0;
    if (IsFloat()) return AsFloat() != 0.0;
    if (IsString()) return !AsString().empty();
    if (IsList()) return !AsList().empty();
    if (IsMap()) return !AsMap().empty();
    if (IsError()) return false;
    return true;  // functions are truthy
}

// --- Type name ---

std::string_view ScriptValue::TypeName() const {
    if (IsNull()) return "null";
    if (IsBool()) return "bool";
    if (IsInt()) return "int";
    if (IsFloat()) return "float";
    if (IsString()) return "string";
    if (IsList()) return "list";
    if (IsMap()) return "map";
    if (IsFunction()) return "function";
    if (IsNativeFunction()) return "native_function";
    if (IsError()) return "error";
    return "unknown";
}

// --- String representation ---

std::string ScriptValue::ToString() const {
    if (IsNull()) return "null";
    if (IsBool()) return AsBool() ? "true" : "false";
    if (IsInt()) return std::to_string(AsInt());
    if (IsFloat()) {
        std::ostringstream oss;
        oss << AsFloat();
        return oss.str();
    }
    if (IsString()) return AsString();
    if (IsList()) {
        std::ostringstream oss;
        oss << "[";
        const auto& list = AsList();
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) oss << ", ";
            if (list[i].IsString()) {
                oss << "\"" << list[i].ToString() << "\"";
            } else {
                oss << list[i].ToString();
            }
        }
        oss << "]";
        return oss.str();
    }
    if (IsMap()) {
        std::ostringstream oss;
        oss << "{";
        const auto& map = AsMap();
        bool first = true;
        for (const auto& [key, val] : map) {
            if (!first) oss << ", ";
            oss << key << ": ";
            if (val.IsString()) {
                oss << "\"" << val.ToString() << "\"";
            } else {
                oss << val.ToString();
            }
            first = false;
        }
        oss << "}";
        return oss.str();
    }
    if (IsFunction()) return "<fn " + AsFunction().name + ">";
    if (IsNativeFunction()) return "<native_fn>";
    if (IsError()) return "<error: " + AsError().message + ">";
    return "<unknown>";
}

// --- Equality ---

bool ScriptValue::Equals(const ScriptValue& other) const {
    if (IsNull() && other.IsNull()) return true;
    if (IsBool() && other.IsBool()) return AsBool() == other.AsBool();
    if (IsInt() && other.IsInt()) return AsInt() == other.AsInt();
    if (IsFloat() && other.IsFloat()) return AsFloat() == other.AsFloat();
    if (IsInt() && other.IsFloat()) return static_cast<double>(AsInt()) == other.AsFloat();
    if (IsFloat() && other.IsInt()) return AsFloat() == static_cast<double>(other.AsInt());
    if (IsString() && other.IsString()) return AsString() == other.AsString();
    if (IsList() && other.IsList()) {
        const auto& a = AsList();
        const auto& b = other.AsList();
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!a[i].Equals(b[i])) return false;
        }
        return true;
    }
    if (IsMap() && other.IsMap()) {
        const auto& a = AsMap();
        const auto& b = other.AsMap();
        if (a.size() != b.size()) return false;
        for (const auto& [key, val] : a) {
            auto it = b.find(key);
            if (it == b.end() || !val.Equals(it->second)) return false;
        }
        return true;
    }
    return false;
}

// --- Arithmetic helpers ---

namespace {

ScriptValue NumericOp(const ScriptValue& lhs, const ScriptValue& rhs,
                      auto int_op, auto float_op, std::string_view op_name) {
    if (lhs.IsInt() && rhs.IsInt()) {
        return ScriptValue(int_op(lhs.AsInt(), rhs.AsInt()));
    }
    if (lhs.IsNumber() && rhs.IsNumber()) {
        return ScriptValue(float_op(lhs.AsNumber(), rhs.AsNumber()));
    }
    return ScriptValue::Error(std::string("Cannot ") + std::string(op_name) + " " +
                              std::string(lhs.TypeName()) + " and " +
                              std::string(rhs.TypeName()));
}

ScriptValue CompareOp(const ScriptValue& lhs, const ScriptValue& rhs,
                      auto int_cmp, auto float_cmp, auto str_cmp, std::string_view op_name) {
    if (lhs.IsInt() && rhs.IsInt()) {
        return ScriptValue(int_cmp(lhs.AsInt(), rhs.AsInt()));
    }
    if (lhs.IsNumber() && rhs.IsNumber()) {
        return ScriptValue(float_cmp(lhs.AsNumber(), rhs.AsNumber()));
    }
    if (lhs.IsString() && rhs.IsString()) {
        return ScriptValue(str_cmp(lhs.AsString(), rhs.AsString()));
    }
    return ScriptValue::Error(std::string("Cannot compare ") + std::string(lhs.TypeName()) +
                              " and " + std::string(rhs.TypeName()) + " with " +
                              std::string(op_name));
}

}  // namespace

// --- Arithmetic ---

ScriptValue ScriptValue::Add(const ScriptValue& lhs, const ScriptValue& rhs) {
    // A string on either side coerces the other operand via ToString — matches
    // JS semantics (`1 + "2" == "12"`), chosen so users can build log/response
    // strings without explicit str() calls. The numeric path below runs only
    // when neither operand is a string.
    if (lhs.IsString() && rhs.IsString()) {
        return ScriptValue(lhs.AsString() + rhs.AsString());
    }
    if (lhs.IsString()) {
        return ScriptValue(lhs.AsString() + rhs.ToString());
    }
    if (rhs.IsString()) {
        return ScriptValue(lhs.ToString() + rhs.AsString());
    }
    // List concatenation
    if (lhs.IsList() && rhs.IsList()) {
        ScriptList result = lhs.AsList();
        const auto& rhs_list = rhs.AsList();
        result.insert(result.end(), rhs_list.begin(), rhs_list.end());
        return ScriptValue(std::move(result));
    }
    return NumericOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a + b; },
        [](double a, double b) { return a + b; }, "add");
}

ScriptValue ScriptValue::Subtract(const ScriptValue& lhs, const ScriptValue& rhs) {
    return NumericOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a - b; },
        [](double a, double b) { return a - b; }, "subtract");
}

ScriptValue ScriptValue::Multiply(const ScriptValue& lhs, const ScriptValue& rhs) {
    return NumericOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a * b; },
        [](double a, double b) { return a * b; }, "multiply");
}

ScriptValue ScriptValue::Divide(const ScriptValue& lhs, const ScriptValue& rhs) {
    if (rhs.IsInt() && rhs.AsInt() == 0) {
        return Error("Division by zero");
    }
    if (rhs.IsFloat() && rhs.AsFloat() == 0.0) {
        return Error("Division by zero");
    }
    return NumericOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a / b; },
        [](double a, double b) { return a / b; }, "divide");
}

ScriptValue ScriptValue::Modulo(const ScriptValue& lhs, const ScriptValue& rhs) {
    if (rhs.IsInt() && rhs.AsInt() == 0) {
        return Error("Modulo by zero");
    }
    if (lhs.IsInt() && rhs.IsInt()) {
        return ScriptValue(lhs.AsInt() % rhs.AsInt());
    }
    return Error(std::string("Cannot modulo ") + std::string(lhs.TypeName()) + " and " +
                 std::string(rhs.TypeName()));
}

ScriptValue ScriptValue::Negate(const ScriptValue& val) {
    if (val.IsInt()) return ScriptValue(-val.AsInt());
    if (val.IsFloat()) return ScriptValue(-val.AsFloat());
    return Error(std::string("Cannot negate ") + std::string(val.TypeName()));
}

// --- Comparison ---

ScriptValue ScriptValue::LessThan(const ScriptValue& lhs, const ScriptValue& rhs) {
    return CompareOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a < b; },
        [](double a, double b) { return a < b; },
        [](const std::string& a, const std::string& b) { return a < b; }, "<");
}

ScriptValue ScriptValue::LessEqual(const ScriptValue& lhs, const ScriptValue& rhs) {
    return CompareOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a <= b; },
        [](double a, double b) { return a <= b; },
        [](const std::string& a, const std::string& b) { return a <= b; }, "<=");
}

ScriptValue ScriptValue::GreaterThan(const ScriptValue& lhs, const ScriptValue& rhs) {
    return CompareOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a > b; },
        [](double a, double b) { return a > b; },
        [](const std::string& a, const std::string& b) { return a > b; }, ">");
}

ScriptValue ScriptValue::GreaterEqual(const ScriptValue& lhs, const ScriptValue& rhs) {
    return CompareOp(
        lhs, rhs, [](int64_t a, int64_t b) { return a >= b; },
        [](double a, double b) { return a >= b; },
        [](const std::string& a, const std::string& b) { return a >= b; }, ">=");
}

// --- Factories ---

ScriptValue ScriptValue::Null() {
    return ScriptValue(NullValue{});
}

ScriptValue ScriptValue::Error(std::string message) {
    return ScriptValue(ErrorValue{std::move(message)});
}

ScriptValue ScriptValue::List(ScriptList elements) {
    return ScriptValue(std::move(elements));
}

ScriptValue ScriptValue::Map(ScriptMap entries) {
    return ScriptValue(std::move(entries));
}

}  // namespace lang
