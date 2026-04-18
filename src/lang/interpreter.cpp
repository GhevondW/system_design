#include "lang/interpreter.h"

#include <algorithm>
#include <stdexcept>

#include "lang/lexer.h"
#include "lang/parser.h"

namespace lang {

Interpreter::Interpreter() : global_env_(std::make_shared<Environment>()) {
    RegisterBuiltins();
}

ScriptValue Interpreter::Execute(const StmtList& program) {
    ScriptValue last;
    for (const auto& stmt : program) {
        ExecuteStatement(*stmt, global_env_);
    }
    return last;
}

void Interpreter::InjectVariable(const std::string& name, ScriptValue value) {
    global_env_->Define(name, std::move(value));
}

const std::vector<std::string>& Interpreter::GetLogs() const {
    return logs_;
}

void Interpreter::ClearLogs() {
    logs_.clear();
}

std::shared_ptr<Environment> Interpreter::GetGlobalEnv() const {
    return global_env_;
}

// --- Builtins ---

void Interpreter::RegisterBuiltins() {
    global_env_->Define("log", ScriptValue(NativeFunction{[this](std::vector<ScriptValue> args) {
        for (const auto& arg : args) {
            logs_.push_back(arg.ToString());
        }
        return ScriptValue::Null();
    }}));

    global_env_->Define("print", ScriptValue(NativeFunction{[this](std::vector<ScriptValue> args) {
        for (const auto& arg : args) {
            logs_.push_back(arg.ToString());
        }
        return ScriptValue::Null();
    }}));

    global_env_->Define("len", ScriptValue(NativeFunction{[](std::vector<ScriptValue> args) -> ScriptValue {
        if (args.size() != 1) return ScriptValue::Error("len() takes 1 argument");
        const auto& val = args[0];
        if (val.IsString()) return ScriptValue(static_cast<int64_t>(val.AsString().size()));
        if (val.IsList()) return ScriptValue(static_cast<int64_t>(val.AsList().size()));
        if (val.IsMap()) return ScriptValue(static_cast<int64_t>(val.AsMap().size()));
        return ScriptValue::Error("len() requires string, list, or map");
    }}));

    global_env_->Define("str", ScriptValue(NativeFunction{[](std::vector<ScriptValue> args) -> ScriptValue {
        if (args.size() != 1) return ScriptValue::Error("str() takes 1 argument");
        return ScriptValue(args[0].ToString());
    }}));

    global_env_->Define("int", ScriptValue(NativeFunction{[](std::vector<ScriptValue> args) -> ScriptValue {
        if (args.size() != 1) return ScriptValue::Error("int() takes 1 argument");
        const auto& val = args[0];
        if (val.IsInt()) return val;
        if (val.IsFloat()) return ScriptValue(static_cast<int64_t>(val.AsFloat()));
        if (val.IsString()) {
            try { return ScriptValue(static_cast<int64_t>(std::stoll(val.AsString()))); }
            catch (...) { return ScriptValue::Error("Cannot convert string to int"); }
        }
        if (val.IsBool()) return ScriptValue(val.AsBool() ? int64_t{1} : int64_t{0});
        return ScriptValue::Error("Cannot convert to int");
    }}));

    global_env_->Define("float", ScriptValue(NativeFunction{[](std::vector<ScriptValue> args) -> ScriptValue {
        if (args.size() != 1) return ScriptValue::Error("float() takes 1 argument");
        const auto& val = args[0];
        if (val.IsFloat()) return val;
        if (val.IsInt()) return ScriptValue(static_cast<double>(val.AsInt()));
        if (val.IsString()) {
            try { return ScriptValue(std::stod(val.AsString())); }
            catch (...) { return ScriptValue::Error("Cannot convert string to float"); }
        }
        return ScriptValue::Error("Cannot convert to float");
    }}));
}

// --- Statement execution ---

void Interpreter::ExecuteStatement(const Statement& stmt, std::shared_ptr<Environment> env) {
    std::visit(
        [this, &env](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, LetDecl>) ExecuteLetDecl(s, env);
            else if constexpr (std::is_same_v<T, FnDecl>) ExecuteFnDecl(s, env);
            else if constexpr (std::is_same_v<T, IfStmt>) ExecuteIfStmt(s, env);
            else if constexpr (std::is_same_v<T, ForStmt>) ExecuteForStmt(s, env);
            else if constexpr (std::is_same_v<T, WhileStmt>) ExecuteWhileStmt(s, env);
            else if constexpr (std::is_same_v<T, ReturnStmt>) ExecuteReturnStmt(s, env);
            else if constexpr (std::is_same_v<T, ExprStmt>) ExecuteExprStmt(s, env);
            else if constexpr (std::is_same_v<T, BlockStmt>) ExecuteBlock(s.statements, env);
        },
        stmt.data);
}

void Interpreter::ExecuteLetDecl(const LetDecl& decl, std::shared_ptr<Environment> env) {
    auto value = Evaluate(*decl.initializer, env);
    env->Define(decl.name.lexeme, std::move(value));
}

void Interpreter::ExecuteFnDecl(const FnDecl& decl, std::shared_ptr<Environment> env) {
    // Capture the environment for closures
    auto closure_env = env;
    std::vector<std::string> param_names;
    for (const auto& p : decl.params) param_names.push_back(p.lexeme);

    // Store the body as a raw pointer (it lives in the AST which outlives execution)
    const StmtList* body_ptr = &decl.body;
    std::string fn_name = decl.name.lexeme;

    auto fn = NativeFunction{[this, closure_env, param_names, body_ptr,
                              fn_name](std::vector<ScriptValue> args) -> ScriptValue {
        auto call_env = closure_env->CreateChild();
        for (size_t i = 0; i < param_names.size(); ++i) {
            call_env->Define(param_names[i], i < args.size() ? args[i] : ScriptValue::Null());
        }
        try {
            ExecuteBlock(*body_ptr, call_env);
        } catch (const ReturnSignal& ret) {
            return ret.value;
        }
        return ScriptValue::Null();
    }};

    env->Define(decl.name.lexeme, ScriptValue(std::move(fn)));
}

void Interpreter::ExecuteIfStmt(const IfStmt& stmt, std::shared_ptr<Environment> env) {
    auto condition = Evaluate(*stmt.condition, env);
    if (condition.IsTruthy()) {
        ExecuteBlock(stmt.then_branch, env->CreateChild());
    } else if (!stmt.else_branch.empty()) {
        ExecuteBlock(stmt.else_branch, env->CreateChild());
    }
}

void Interpreter::ExecuteForStmt(const ForStmt& stmt, std::shared_ptr<Environment> env) {
    auto iterable = Evaluate(*stmt.iterable, env);
    if (iterable.IsList()) {
        for (const auto& item : iterable.AsList()) {
            auto loop_env = env->CreateChild();
            loop_env->Define(stmt.variable.lexeme, item);
            ExecuteBlock(stmt.body, loop_env);
        }
    } else if (iterable.IsString()) {
        for (char c : iterable.AsString()) {
            auto loop_env = env->CreateChild();
            loop_env->Define(stmt.variable.lexeme, ScriptValue(std::string(1, c)));
            ExecuteBlock(stmt.body, loop_env);
        }
    } else {
        throw std::runtime_error("Cannot iterate over " + std::string(iterable.TypeName()));
    }
}

void Interpreter::ExecuteWhileStmt(const WhileStmt& stmt, std::shared_ptr<Environment> env) {
    while (Evaluate(*stmt.condition, env).IsTruthy()) {
        ExecuteBlock(stmt.body, env->CreateChild());
    }
}

void Interpreter::ExecuteReturnStmt(const ReturnStmt& stmt, std::shared_ptr<Environment> env) {
    ScriptValue value = ScriptValue::Null();
    if (stmt.value.has_value()) {
        value = Evaluate(*stmt.value.value(), env);
    }
    throw ReturnSignal{std::move(value)};
}

void Interpreter::ExecuteExprStmt(const ExprStmt& stmt, std::shared_ptr<Environment> env) {
    Evaluate(*stmt.expression, env);
}

void Interpreter::ExecuteBlock(const StmtList& stmts, std::shared_ptr<Environment> env) {
    for (const auto& stmt : stmts) {
        ExecuteStatement(*stmt, env);
    }
}

// --- Expression evaluation ---

ScriptValue Interpreter::Evaluate(const Expression& expr, std::shared_ptr<Environment> env) {
    return std::visit(
        [this, &env](const auto& e) -> ScriptValue {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, LiteralExpr>) return EvalLiteral(e);
            else if constexpr (std::is_same_v<T, IdentifierExpr>) return EvalIdentifier(e, env);
            else if constexpr (std::is_same_v<T, UnaryExpr>) return EvalUnary(e, env);
            else if constexpr (std::is_same_v<T, BinaryExpr>) return EvalBinary(e, env);
            else if constexpr (std::is_same_v<T, AssignExpr>) return EvalAssign(e, env);
            else if constexpr (std::is_same_v<T, CallExpr>) return EvalCall(e, env);
            else if constexpr (std::is_same_v<T, MemberAccessExpr>) return EvalMemberAccess(e, env);
            else if constexpr (std::is_same_v<T, IndexAccessExpr>) return EvalIndexAccess(e, env);
            else if constexpr (std::is_same_v<T, ListLiteralExpr>) return EvalListLiteral(e, env);
            else if constexpr (std::is_same_v<T, MapLiteralExpr>) return EvalMapLiteral(e, env);
            else if constexpr (std::is_same_v<T, LambdaExpr>) return EvalLambda(e, env);
            else if constexpr (std::is_same_v<T, MatchExpr>) return EvalMatch(e, env);
            else return ScriptValue::Error("Unknown expression type");
        },
        expr.data);
}

ScriptValue Interpreter::EvalLiteral(const LiteralExpr& expr) {
    switch (expr.token.type) {
        case TokenType::kInteger: return ScriptValue(static_cast<int64_t>(std::stoll(expr.token.lexeme)));
        case TokenType::kFloat: return ScriptValue(std::stod(expr.token.lexeme));
        case TokenType::kString: return ScriptValue(expr.token.lexeme);
        case TokenType::kTrue: return ScriptValue(true);
        case TokenType::kFalse: return ScriptValue(false);
        case TokenType::kNull: return ScriptValue::Null();
        default: return ScriptValue::Error("Unknown literal type");
    }
}

ScriptValue Interpreter::EvalIdentifier(const IdentifierExpr& expr,
                                        std::shared_ptr<Environment> env) {
    auto val = env->Get(expr.name.lexeme);
    if (!val.has_value()) {
        throw std::runtime_error("Undefined variable '" + expr.name.lexeme + "'");
    }
    return val.value();
}

ScriptValue Interpreter::EvalUnary(const UnaryExpr& expr, std::shared_ptr<Environment> env) {
    auto operand = Evaluate(*expr.operand, env);
    switch (expr.op.type) {
        case TokenType::kMinus: return ScriptValue::Negate(operand);
        case TokenType::kNot: return ScriptValue(!operand.IsTruthy());
        default: return ScriptValue::Error("Unknown unary operator");
    }
}

ScriptValue Interpreter::EvalBinary(const BinaryExpr& expr, std::shared_ptr<Environment> env) {
    // Short-circuit for logical operators
    if (expr.op.type == TokenType::kAnd) {
        auto left = Evaluate(*expr.left, env);
        if (!left.IsTruthy()) return left;
        return Evaluate(*expr.right, env);
    }
    if (expr.op.type == TokenType::kOr) {
        auto left = Evaluate(*expr.left, env);
        if (left.IsTruthy()) return left;
        return Evaluate(*expr.right, env);
    }

    auto left = Evaluate(*expr.left, env);
    auto right = Evaluate(*expr.right, env);

    switch (expr.op.type) {
        case TokenType::kPlus: return ScriptValue::Add(left, right);
        case TokenType::kMinus: return ScriptValue::Subtract(left, right);
        case TokenType::kStar: return ScriptValue::Multiply(left, right);
        case TokenType::kSlash: return ScriptValue::Divide(left, right);
        case TokenType::kPercent: return ScriptValue::Modulo(left, right);
        case TokenType::kEqual: return ScriptValue(left.Equals(right));
        case TokenType::kNotEqual: return ScriptValue(!left.Equals(right));
        case TokenType::kLess: return ScriptValue::LessThan(left, right);
        case TokenType::kLessEqual: return ScriptValue::LessEqual(left, right);
        case TokenType::kGreater: return ScriptValue::GreaterThan(left, right);
        case TokenType::kGreaterEqual: return ScriptValue::GreaterEqual(left, right);
        default: return ScriptValue::Error("Unknown binary operator");
    }
}

ScriptValue Interpreter::EvalAssign(const AssignExpr& expr, std::shared_ptr<Environment> env) {
    auto value = Evaluate(*expr.value, env);

    // Simple variable assignment
    if (auto* ident = std::get_if<IdentifierExpr>(&expr.target->data)) {
        if (!env->Set(ident->name.lexeme, value)) {
            throw std::runtime_error("Undefined variable '" + ident->name.lexeme + "'");
        }
        return value;
    }

    // Member assignment: obj.field = value
    if (auto* member = std::get_if<MemberAccessExpr>(&expr.target->data)) {
        auto object = Evaluate(*member->object, env);
        if (object.IsMap()) {
            object.AsMap()[member->member.lexeme] = value;
            return value;
        }
        throw std::runtime_error("Cannot assign to member of " + std::string(object.TypeName()));
    }

    // Index assignment: arr[i] = value
    if (auto* index_expr = std::get_if<IndexAccessExpr>(&expr.target->data)) {
        auto object = Evaluate(*index_expr->object, env);
        auto index = Evaluate(*index_expr->index, env);
        if (object.IsList() && index.IsInt()) {
            auto& list = object.AsList();
            auto idx = index.AsInt();
            if (idx < 0 || static_cast<size_t>(idx) >= list.size()) {
                throw std::runtime_error("List index out of bounds");
            }
            list[static_cast<size_t>(idx)] = value;
            return value;
        }
        if (object.IsMap() && index.IsString()) {
            object.AsMap()[index.AsString()] = value;
            return value;
        }
        throw std::runtime_error("Invalid assignment target");
    }

    throw std::runtime_error("Invalid assignment target");
}

ScriptValue Interpreter::EvalCall(const CallExpr& expr, std::shared_ptr<Environment> env) {
    // Check for method call: obj.method(args)
    if (auto* member = std::get_if<MemberAccessExpr>(&expr.callee->data)) {
        auto object = Evaluate(*member->object, env);
        std::vector<ScriptValue> args;
        for (const auto& arg : expr.arguments) {
            args.push_back(Evaluate(*arg, env));
        }
        return CallMethod(object, member->member.lexeme, std::move(args));
    }

    auto callee = Evaluate(*expr.callee, env);
    std::vector<ScriptValue> args;
    for (const auto& arg : expr.arguments) {
        args.push_back(Evaluate(*arg, env));
    }
    return CallFunction(callee, std::move(args), env);
}

ScriptValue Interpreter::EvalMemberAccess(const MemberAccessExpr& expr,
                                          std::shared_ptr<Environment> env) {
    auto object = Evaluate(*expr.object, env);
    if (object.IsMap()) {
        auto& map = object.AsMap();
        auto it = map.find(expr.member.lexeme);
        if (it != map.end()) return it->second;
        return ScriptValue::Null();
    }
    if (object.IsError() && expr.member.lexeme == "error") {
        return ScriptValue(object.AsError().message);
    }
    // For method-style access that isn't a call, return null
    return ScriptValue::Null();
}

ScriptValue Interpreter::EvalIndexAccess(const IndexAccessExpr& expr,
                                         std::shared_ptr<Environment> env) {
    auto object = Evaluate(*expr.object, env);
    auto index = Evaluate(*expr.index, env);

    if (object.IsList() && index.IsInt()) {
        auto& list = object.AsList();
        auto idx = index.AsInt();
        if (idx < 0 || static_cast<size_t>(idx) >= list.size()) {
            return ScriptValue::Null();
        }
        return list[static_cast<size_t>(idx)];
    }
    if (object.IsMap() && index.IsString()) {
        auto& map = object.AsMap();
        auto it = map.find(index.AsString());
        if (it != map.end()) return it->second;
        return ScriptValue::Null();
    }
    if (object.IsString() && index.IsInt()) {
        auto idx = index.AsInt();
        const auto& str = object.AsString();
        if (idx < 0 || static_cast<size_t>(idx) >= str.size()) return ScriptValue::Null();
        return ScriptValue(std::string(1, str[static_cast<size_t>(idx)]));
    }
    return ScriptValue::Error("Cannot index " + std::string(object.TypeName()));
}

ScriptValue Interpreter::EvalListLiteral(const ListLiteralExpr& expr,
                                         std::shared_ptr<Environment> env) {
    ScriptList elements;
    for (const auto& elem : expr.elements) {
        elements.push_back(Evaluate(*elem, env));
    }
    return ScriptValue::List(std::move(elements));
}

ScriptValue Interpreter::EvalMapLiteral(const MapLiteralExpr& expr,
                                        std::shared_ptr<Environment> env) {
    ScriptMap entries;
    for (const auto& entry : expr.entries) {
        entries[entry.key.lexeme] = Evaluate(*entry.value, env);
    }
    return ScriptValue::Map(std::move(entries));
}

ScriptValue Interpreter::EvalLambda(const LambdaExpr& expr, std::shared_ptr<Environment> env) {
    auto closure_env = env;

    // If there are captures, create a new env with captured values
    if (!expr.captures.empty()) {
        closure_env = env->CreateChild();
        for (const auto& cap : expr.captures) {
            auto val = env->Get(cap.lexeme);
            if (val.has_value()) {
                closure_env->Define(cap.lexeme, val.value());
            }
        }
    }

    std::vector<std::string> param_names;
    for (const auto& p : expr.params) param_names.push_back(p.lexeme);
    const StmtList* body_ptr = &expr.body;

    return ScriptValue(NativeFunction{[this, closure_env, param_names,
                                       body_ptr](std::vector<ScriptValue> args) -> ScriptValue {
        auto call_env = closure_env->CreateChild();
        for (size_t i = 0; i < param_names.size(); ++i) {
            call_env->Define(param_names[i], i < args.size() ? args[i] : ScriptValue::Null());
        }
        try {
            ExecuteBlock(*body_ptr, call_env);
        } catch (const ReturnSignal& ret) {
            return ret.value;
        }
        return ScriptValue::Null();
    }});
}

ScriptValue Interpreter::EvalMatch(const MatchExpr& expr, std::shared_ptr<Environment> env) {
    auto subject = Evaluate(*expr.subject, env);
    for (const auto& arm : expr.arms) {
        if (!arm.pattern) {
            // Wildcard — always matches
            return Evaluate(*arm.value, env);
        }
        auto pattern_val = Evaluate(*arm.pattern, env);
        if (subject.Equals(pattern_val)) {
            return Evaluate(*arm.value, env);
        }
    }
    return ScriptValue::Null();
}

// --- Function calling ---

ScriptValue Interpreter::CallFunction(const ScriptValue& callee, std::vector<ScriptValue> args,
                                      std::shared_ptr<Environment> /*env*/) {
    if (callee.IsNativeFunction()) {
        return callee.AsNativeFunction()(std::move(args));
    }
    throw std::runtime_error("Cannot call " + std::string(callee.TypeName()));
}

// --- Method dispatch ---

ScriptValue Interpreter::CallMethod(const ScriptValue& object, const std::string& method,
                                    std::vector<ScriptValue> args) {
    // String methods
    if (object.IsString()) {
        const auto& str = object.AsString();
        if (method == "length") return ScriptValue(static_cast<int64_t>(str.size()));
        if (method == "contains") {
            if (args.empty() || !args[0].IsString()) return ScriptValue::Error("contains() requires a string argument");
            return ScriptValue(str.find(args[0].AsString()) != std::string::npos);
        }
        if (method == "split") {
            if (args.empty() || !args[0].IsString()) return ScriptValue::Error("split() requires a string argument");
            ScriptList parts;
            std::string delim = args[0].AsString();
            size_t start = 0;
            size_t end;
            while ((end = str.find(delim, start)) != std::string::npos) {
                parts.push_back(ScriptValue(str.substr(start, end - start)));
                start = end + delim.size();
            }
            parts.push_back(ScriptValue(str.substr(start)));
            return ScriptValue::List(std::move(parts));
        }
        if (method == "upper") {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return ScriptValue(std::move(result));
        }
        if (method == "lower") {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return ScriptValue(std::move(result));
        }
        if (method == "substr") {
            if (args.size() < 2 || !args[0].IsInt() || !args[1].IsInt())
                return ScriptValue::Error("substr() requires two integer arguments");
            auto start_idx = static_cast<size_t>(args[0].AsInt());
            auto length = static_cast<size_t>(args[1].AsInt());
            if (start_idx >= str.size()) return ScriptValue("");
            return ScriptValue(str.substr(start_idx, length));
        }
    }

    // List methods
    if (object.IsList()) {
        auto& list = object.AsList();
        if (method == "size") return ScriptValue(static_cast<int64_t>(list.size()));
        if (method == "push") {
            if (!args.empty()) list.push_back(args[0]);
            return ScriptValue::Null();
        }
        if (method == "pop") {
            if (list.empty()) return ScriptValue::Null();
            auto val = list.back();
            list.pop_back();
            return val;
        }
    }

    // Map methods
    if (object.IsMap()) {
        auto& map = object.AsMap();
        if (method == "size") return ScriptValue(static_cast<int64_t>(map.size()));
        if (method == "has") {
            if (args.empty() || !args[0].IsString()) return ScriptValue::Error("has() requires a string argument");
            return ScriptValue(map.count(args[0].AsString()) > 0);
        }
        if (method == "keys") {
            ScriptList keys;
            for (const auto& [k, v] : map) {
                keys.push_back(ScriptValue(k));
            }
            return ScriptValue::List(std::move(keys));
        }
        if (method == "values") {
            ScriptList values;
            for (const auto& [k, v] : map) {
                values.push_back(v);
            }
            return ScriptValue::List(std::move(values));
        }
        // Map member access as method fallback
        auto it = map.find(method);
        if (it != map.end() && it->second.IsCallable()) {
            return CallFunction(it->second, std::move(args), nullptr);
        }
    }

    return ScriptValue::Error("Unknown method '" + method + "' on " +
                              std::string(object.TypeName()));
}

}  // namespace lang
