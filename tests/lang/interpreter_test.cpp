#include "lang/interpreter.h"

#include <gtest/gtest.h>

#include "lang/lexer.h"
#include "lang/parser.h"

namespace lang {
namespace {

// Helper: run source code and return the interpreter
Interpreter RunCode(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.Tokenize();
    Parser parser(std::move(tokens));
    auto result = parser.Parse();
    EXPECT_TRUE(result.has_value()) << result.error().message;

    Interpreter interp;
    interp.Execute(result.value());
    return interp;
}

// Helper: run and get a variable value
ScriptValue RunCodeAndGet(const std::string& source, const std::string& var_name) {
    auto interp = RunCode(source);
    auto val = interp.GetGlobalEnv()->Get(var_name);
    EXPECT_TRUE(val.has_value()) << "Variable '" << var_name << "' not found";
    return val.value();
}

// --- Variables ---

TEST(Interpreter, LetDecl_Integer) {
    auto val = RunCodeAndGet("let x = 42;", "x");
    EXPECT_TRUE(val.IsInt());
    EXPECT_EQ(val.AsInt(), 42);
}

TEST(Interpreter, LetDecl_String) {
    auto val = RunCodeAndGet("let s = \"hello\";", "s");
    EXPECT_EQ(val.AsString(), "hello");
}

TEST(Interpreter, LetDecl_Bool) {
    EXPECT_TRUE(RunCodeAndGet("let a = true;", "a").AsBool());
    EXPECT_FALSE(RunCodeAndGet("let a = false;", "a").AsBool());
}

TEST(Interpreter, LetDecl_Null) {
    auto val = RunCodeAndGet("let n = null;", "n");
    EXPECT_TRUE(val.IsNull());
}

TEST(Interpreter, LetDecl_Float) {
    auto val = RunCodeAndGet("let f = 3.14;", "f");
    EXPECT_DOUBLE_EQ(val.AsFloat(), 3.14);
}

// --- Arithmetic ---

TEST(Interpreter, Arithmetic_Addition) {
    auto val = RunCodeAndGet("let x = 3 + 4;", "x");
    EXPECT_EQ(val.AsInt(), 7);
}

TEST(Interpreter, Arithmetic_Subtraction) {
    auto val = RunCodeAndGet("let x = 10 - 3;", "x");
    EXPECT_EQ(val.AsInt(), 7);
}

TEST(Interpreter, Arithmetic_Multiplication) {
    auto val = RunCodeAndGet("let x = 6 * 7;", "x");
    EXPECT_EQ(val.AsInt(), 42);
}

TEST(Interpreter, Arithmetic_Division) {
    auto val = RunCodeAndGet("let x = 10 / 3;", "x");
    EXPECT_EQ(val.AsInt(), 3);
}

TEST(Interpreter, Arithmetic_Modulo) {
    auto val = RunCodeAndGet("let x = 10 % 3;", "x");
    EXPECT_EQ(val.AsInt(), 1);
}

TEST(Interpreter, Arithmetic_Precedence) {
    auto val = RunCodeAndGet("let x = 2 + 3 * 4;", "x");
    EXPECT_EQ(val.AsInt(), 14);
}

TEST(Interpreter, Arithmetic_Parentheses) {
    auto val = RunCodeAndGet("let x = (2 + 3) * 4;", "x");
    EXPECT_EQ(val.AsInt(), 20);
}

TEST(Interpreter, Arithmetic_Negation) {
    auto val = RunCodeAndGet("let x = -5;", "x");
    EXPECT_EQ(val.AsInt(), -5);
}

TEST(Interpreter, Arithmetic_StringConcat) {
    auto val = RunCodeAndGet("let s = \"hello\" + \" world\";", "s");
    EXPECT_EQ(val.AsString(), "hello world");
}

// --- Comparison ---

TEST(Interpreter, Comparison_Equal) {
    EXPECT_TRUE(RunCodeAndGet("let r = 1 == 1;", "r").AsBool());
    EXPECT_FALSE(RunCodeAndGet("let r = 1 == 2;", "r").AsBool());
}

TEST(Interpreter, Comparison_NotEqual) {
    EXPECT_TRUE(RunCodeAndGet("let r = 1 != 2;", "r").AsBool());
}

TEST(Interpreter, Comparison_Less) {
    EXPECT_TRUE(RunCodeAndGet("let r = 1 < 2;", "r").AsBool());
    EXPECT_FALSE(RunCodeAndGet("let r = 2 < 1;", "r").AsBool());
}

TEST(Interpreter, Comparison_LessEqual) {
    EXPECT_TRUE(RunCodeAndGet("let r = 1 <= 1;", "r").AsBool());
}

TEST(Interpreter, Comparison_Greater) {
    EXPECT_TRUE(RunCodeAndGet("let r = 2 > 1;", "r").AsBool());
}

TEST(Interpreter, Comparison_GreaterEqual) {
    EXPECT_TRUE(RunCodeAndGet("let r = 2 >= 2;", "r").AsBool());
}

// --- Logical ---

TEST(Interpreter, Logical_And) {
    EXPECT_TRUE(RunCodeAndGet("let r = true && true;", "r").IsTruthy());
    EXPECT_FALSE(RunCodeAndGet("let r = true && false;", "r").IsTruthy());
}

TEST(Interpreter, Logical_Or) {
    EXPECT_TRUE(RunCodeAndGet("let r = false || true;", "r").IsTruthy());
    EXPECT_FALSE(RunCodeAndGet("let r = false || false;", "r").IsTruthy());
}

TEST(Interpreter, Logical_Not) {
    EXPECT_FALSE(RunCodeAndGet("let r = !true;", "r").AsBool());
    EXPECT_TRUE(RunCodeAndGet("let r = !false;", "r").AsBool());
}

// --- Functions ---

TEST(Interpreter, Function_Basic) {
    auto val = RunCodeAndGet("fn add(a, b) { return a + b; } let r = add(3, 4);", "r");
    EXPECT_EQ(val.AsInt(), 7);
}

TEST(Interpreter, Function_NoReturn) {
    auto val = RunCodeAndGet("fn noop() {} let r = noop();", "r");
    EXPECT_TRUE(val.IsNull());
}

TEST(Interpreter, Function_Recursion) {
    auto val = RunCodeAndGet(
        "fn fact(n) { if (n <= 1) { return 1; } return n * fact(n - 1); } let r = fact(5);", "r");
    EXPECT_EQ(val.AsInt(), 120);
}

TEST(Interpreter, Function_Closure) {
    auto val = RunCodeAndGet(
        "fn make_adder(x) { return [x](y) { return x + y; }; } let add5 = make_adder(5); let r = add5(3);",
        "r");
    EXPECT_EQ(val.AsInt(), 8);
}

// --- Lambda ---

TEST(Interpreter, Lambda_Basic) {
    auto val = RunCodeAndGet("let add = [](a, b) { return a + b; }; let r = add(3, 4);", "r");
    EXPECT_EQ(val.AsInt(), 7);
}

TEST(Interpreter, Lambda_WithCapture) {
    auto val = RunCodeAndGet(
        "let threshold = 10; let check = [threshold](val) { return val > threshold; }; let r = check(15);",
        "r");
    EXPECT_TRUE(val.AsBool());
}

// --- If/Else ---

TEST(Interpreter, If_TrueBranch) {
    auto interp = RunCode("let x = 0; if (true) { x = 1; }");
    auto val = interp.GetGlobalEnv()->Get("x");
    EXPECT_EQ(val.value().AsInt(), 1);
}

TEST(Interpreter, If_FalseBranch) {
    auto interp = RunCode("let x = 0; if (false) { x = 1; } else { x = 2; }");
    auto val = interp.GetGlobalEnv()->Get("x");
    EXPECT_EQ(val.value().AsInt(), 2);
}

TEST(Interpreter, If_ElseIf) {
    auto interp = RunCode(
        "let x = 5; let r = 0; if (x > 10) { r = 1; } else if (x > 3) { r = 2; } else { r = 3; }");
    auto val = interp.GetGlobalEnv()->Get("r");
    EXPECT_EQ(val.value().AsInt(), 2);
}

// --- For loop ---

TEST(Interpreter, For_BasicIteration) {
    auto interp = RunCode("let sum = 0; for (let x : [1, 2, 3]) { sum = sum + x; }");
    auto val = interp.GetGlobalEnv()->Get("sum");
    EXPECT_EQ(val.value().AsInt(), 6);
}

// --- While loop ---

TEST(Interpreter, While_BasicLoop) {
    auto interp = RunCode("let x = 0; while (x < 5) { x = x + 1; }");
    auto val = interp.GetGlobalEnv()->Get("x");
    EXPECT_EQ(val.value().AsInt(), 5);
}

// --- Lists ---

TEST(Interpreter, List_Create) {
    auto val = RunCodeAndGet("let items = [1, 2, 3];", "items");
    EXPECT_TRUE(val.IsList());
    EXPECT_EQ(val.AsList().size(), 3);
}

TEST(Interpreter, List_IndexAccess) {
    auto val = RunCodeAndGet("let items = [10, 20, 30]; let r = items[1];", "r");
    EXPECT_EQ(val.AsInt(), 20);
}

TEST(Interpreter, List_Push) {
    auto val = RunCodeAndGet("let items = [1, 2]; items.push(3); let r = items.size();", "r");
    EXPECT_EQ(val.AsInt(), 3);
}

TEST(Interpreter, List_Pop) {
    auto val = RunCodeAndGet("let items = [1, 2, 3]; let r = items.pop();", "r");
    EXPECT_EQ(val.AsInt(), 3);
}

TEST(Interpreter, List_Size) {
    auto val = RunCodeAndGet("let r = [1, 2, 3].size();", "r");
    EXPECT_EQ(val.AsInt(), 3);
}

// --- Maps ---

TEST(Interpreter, Map_Create) {
    auto val = RunCodeAndGet("let user = { name: \"Alice\", age: 30 };", "user");
    EXPECT_TRUE(val.IsMap());
    EXPECT_EQ(val.AsMap().size(), 2);
}

TEST(Interpreter, Map_MemberAccess) {
    auto val = RunCodeAndGet("let user = { name: \"Alice\" }; let r = user.name;", "r");
    EXPECT_EQ(val.AsString(), "Alice");
}

TEST(Interpreter, Map_MemberAssign) {
    auto val = RunCodeAndGet(
        "let user = { name: \"Alice\" }; user.email = \"alice@example.com\"; let r = user.email;",
        "r");
    EXPECT_EQ(val.AsString(), "alice@example.com");
}

TEST(Interpreter, Map_Has) {
    EXPECT_TRUE(RunCodeAndGet("let m = { a: 1 }; let r = m.has(\"a\");", "r").AsBool());
    EXPECT_FALSE(RunCodeAndGet("let m = { a: 1 }; let r = m.has(\"b\");", "r").AsBool());
}

TEST(Interpreter, Map_Size) {
    auto val = RunCodeAndGet("let r = { a: 1, b: 2 }.size();", "r");
    EXPECT_EQ(val.AsInt(), 2);
}

TEST(Interpreter, Map_Nested) {
    auto val = RunCodeAndGet(
        "let resp = { status: 200, body: { name: \"Alice\" } }; let r = resp.body.name;", "r");
    EXPECT_EQ(val.AsString(), "Alice");
}

// --- Strings ---

TEST(Interpreter, String_Length) {
    auto val = RunCodeAndGet("let r = \"hello\".length();", "r");
    EXPECT_EQ(val.AsInt(), 5);
}

TEST(Interpreter, String_Contains) {
    EXPECT_TRUE(RunCodeAndGet("let r = \"hello world\".contains(\"world\");", "r").AsBool());
    EXPECT_FALSE(RunCodeAndGet("let r = \"hello\".contains(\"xyz\");", "r").AsBool());
}

TEST(Interpreter, String_Split) {
    auto val = RunCodeAndGet("let r = \"a,b,c\".split(\",\");", "r");
    EXPECT_TRUE(val.IsList());
    EXPECT_EQ(val.AsList().size(), 3);
    EXPECT_EQ(val.AsList()[0].AsString(), "a");
}

TEST(Interpreter, String_Upper) {
    auto val = RunCodeAndGet("let r = \"hello\".upper();", "r");
    EXPECT_EQ(val.AsString(), "HELLO");
}

TEST(Interpreter, String_Substr) {
    auto val = RunCodeAndGet("let r = \"hello world\".substr(0, 5);", "r");
    EXPECT_EQ(val.AsString(), "hello");
}

TEST(Interpreter, String_Concat) {
    auto val = RunCodeAndGet("let r = \"hello\" + \" \" + \"world\";", "r");
    EXPECT_EQ(val.AsString(), "hello world");
}

// --- Match ---

TEST(Interpreter, Match_BasicMatch) {
    auto val = RunCodeAndGet("let r = match (\"a\") { \"a\" => 1, \"b\" => 2, _ => 0 };", "r");
    EXPECT_EQ(val.AsInt(), 1);
}

TEST(Interpreter, Match_Wildcard) {
    auto val = RunCodeAndGet("let r = match (\"c\") { \"a\" => 1, _ => 99 };", "r");
    EXPECT_EQ(val.AsInt(), 99);
}

TEST(Interpreter, Match_NoMatch) {
    auto val = RunCodeAndGet("let r = match (\"z\") { \"a\" => 1 };", "r");
    EXPECT_TRUE(val.IsNull());
}

// --- Builtins ---

TEST(Interpreter, Builtin_Log) {
    auto interp = RunCode("log(\"hello\"); log(42);");
    ASSERT_EQ(interp.GetLogs().size(), 2);
    EXPECT_EQ(interp.GetLogs()[0], "hello");
    EXPECT_EQ(interp.GetLogs()[1], "42");
}

TEST(Interpreter, Builtin_Len) {
    EXPECT_EQ(RunCodeAndGet("let r = len(\"hello\");", "r").AsInt(), 5);
    EXPECT_EQ(RunCodeAndGet("let r = len([1, 2, 3]);", "r").AsInt(), 3);
}

TEST(Interpreter, Builtin_Str) {
    EXPECT_EQ(RunCodeAndGet("let r = str(42);", "r").AsString(), "42");
    EXPECT_EQ(RunCodeAndGet("let r = str(true);", "r").AsString(), "true");
}

TEST(Interpreter, Builtin_Int) {
    EXPECT_EQ(RunCodeAndGet("let r = int(3.14);", "r").AsInt(), 3);
    EXPECT_EQ(RunCodeAndGet("let r = int(\"42\");", "r").AsInt(), 42);
}

TEST(Interpreter, Builtin_Float) {
    EXPECT_DOUBLE_EQ(RunCodeAndGet("let r = float(3);", "r").AsFloat(), 3.0);
}

// --- Inject variable ---

TEST(Interpreter, InjectVariable) {
    Lexer lexer("let r = injected + 1;");
    auto tokens = lexer.Tokenize();
    Parser parser(std::move(tokens));
    auto result = parser.Parse();
    ASSERT_TRUE(result.has_value());

    Interpreter interp;
    interp.InjectVariable("injected", ScriptValue(41));
    interp.Execute(result.value());

    auto val = interp.GetGlobalEnv()->Get("r");
    EXPECT_EQ(val.value().AsInt(), 42);
}

// --- Complex spec examples ---

TEST(Interpreter, SpecExample_ValidatePlate) {
    auto val = RunCodeAndGet(R"(
        fn validate_plate(plate) {
            if (plate.length() != 6) {
                return { ok: false, error: "invalid length" };
            }
            return { ok: true };
        }
        let r1 = validate_plate("ABC123");
        let r2 = validate_plate("AB");
    )", "r1");
    EXPECT_TRUE(val.IsMap());
    EXPECT_TRUE(val.AsMap().at("ok").AsBool());

    // Also check r2
    auto interp = RunCode(R"(
        fn validate_plate(plate) {
            if (plate.length() != 6) {
                return { ok: false, error: "invalid length" };
            }
            return { ok: true };
        }
        let r2 = validate_plate("AB");
    )");
    auto r2 = interp.GetGlobalEnv()->Get("r2");
    EXPECT_FALSE(r2.value().AsMap().at("ok").AsBool());
}

TEST(Interpreter, SpecExample_HandleRequest) {
    auto val = RunCodeAndGet(R"(
        fn get_car(plate) { return { found: true, plate: plate }; }
        fn register_car(body) { return { registered: true }; }

        fn handle_request(req) {
            return match (req.method) {
                "GET"  => get_car(req.params.plate),
                "POST" => register_car(req.body),
                _      => { status: 405, body: "Method not allowed" }
            };
        }

        let req = { method: "GET", params: { plate: "ABC123" } };
        let r = handle_request(req);
    )", "r");
    EXPECT_TRUE(val.IsMap());
    EXPECT_TRUE(val.AsMap().at("found").AsBool());
    EXPECT_EQ(val.AsMap().at("plate").AsString(), "ABC123");
}

TEST(Interpreter, SpecExample_ForLoop) {
    auto interp = RunCode(R"(
        let plates = ["ABC123", "XYZ789"];
        for (let plate : plates) {
            log(plate);
        }
    )");
    ASSERT_EQ(interp.GetLogs().size(), 2);
    EXPECT_EQ(interp.GetLogs()[0], "ABC123");
    EXPECT_EQ(interp.GetLogs()[1], "XYZ789");
}

}  // namespace
}  // namespace lang
