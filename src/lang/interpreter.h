#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lang/ast.h"
#include "lang/environment.h"
#include "lang/value.h"

namespace lang {

// Thrown to unwind the stack on return statements
struct ReturnSignal {
    ScriptValue value;
};

class Interpreter {
public:
    Interpreter();

    // Run a full program
    ScriptValue Execute(const StmtList& program);

    // Inject a variable into the global scope (for component APIs)
    void InjectVariable(const std::string& name, ScriptValue value);

    // Access the log output
    [[nodiscard]] const std::vector<std::string>& GetLogs() const;
    void ClearLogs();

    // Access the global environment
    [[nodiscard]] std::shared_ptr<Environment> GetGlobalEnv() const;

private:
    // Statement execution
    void ExecuteStatement(const Statement& stmt, std::shared_ptr<Environment> env);
    void ExecuteLetDecl(const LetDecl& decl, std::shared_ptr<Environment> env);
    void ExecuteFnDecl(const FnDecl& decl, std::shared_ptr<Environment> env);
    void ExecuteIfStmt(const IfStmt& stmt, std::shared_ptr<Environment> env);
    void ExecuteForStmt(const ForStmt& stmt, std::shared_ptr<Environment> env);
    void ExecuteWhileStmt(const WhileStmt& stmt, std::shared_ptr<Environment> env);
    void ExecuteReturnStmt(const ReturnStmt& stmt, std::shared_ptr<Environment> env);
    void ExecuteExprStmt(const ExprStmt& stmt, std::shared_ptr<Environment> env);
    void ExecuteBlock(const StmtList& stmts, std::shared_ptr<Environment> env);

    // Expression evaluation
    ScriptValue Evaluate(const Expression& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalLiteral(const LiteralExpr& expr);
    ScriptValue EvalIdentifier(const IdentifierExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalUnary(const UnaryExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalBinary(const BinaryExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalAssign(const AssignExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalCall(const CallExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalMemberAccess(const MemberAccessExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalIndexAccess(const IndexAccessExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalListLiteral(const ListLiteralExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalMapLiteral(const MapLiteralExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalLambda(const LambdaExpr& expr, std::shared_ptr<Environment> env);
    ScriptValue EvalMatch(const MatchExpr& expr, std::shared_ptr<Environment> env);

    // Call a function value with arguments
    ScriptValue CallFunction(const ScriptValue& callee, std::vector<ScriptValue> args,
                             std::shared_ptr<Environment> env);

    // Built-in method dispatch for strings, lists, maps
    ScriptValue CallMethod(const ScriptValue& object, const std::string& method,
                           std::vector<ScriptValue> args);

    void RegisterBuiltins();

    std::shared_ptr<Environment> global_env_;
    std::vector<std::string> logs_;
};

}  // namespace lang
