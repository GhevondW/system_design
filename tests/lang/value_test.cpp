#include "lang/value.h"

#include <gtest/gtest.h>

namespace lang {
namespace {

// --- Construction and Type Queries ---

TEST(ScriptValue, DefaultIsNull) {
    ScriptValue val;
    EXPECT_TRUE(val.IsNull());
    EXPECT_EQ(val.TypeName(), "null");
}

TEST(ScriptValue, BoolConstruction) {
    ScriptValue t(true);
    ScriptValue f(false);
    EXPECT_TRUE(t.IsBool());
    EXPECT_TRUE(f.IsBool());
    EXPECT_TRUE(t.AsBool());
    EXPECT_FALSE(f.AsBool());
}

TEST(ScriptValue, IntConstruction) {
    ScriptValue val(42);
    EXPECT_TRUE(val.IsInt());
    EXPECT_TRUE(val.IsNumber());
    EXPECT_EQ(val.AsInt(), 42);
    EXPECT_EQ(val.TypeName(), "int");
}

TEST(ScriptValue, Int64Construction) {
    ScriptValue val(int64_t{9999999999});
    EXPECT_TRUE(val.IsInt());
    EXPECT_EQ(val.AsInt(), 9999999999);
}

TEST(ScriptValue, FloatConstruction) {
    ScriptValue val(3.14);
    EXPECT_TRUE(val.IsFloat());
    EXPECT_TRUE(val.IsNumber());
    EXPECT_DOUBLE_EQ(val.AsFloat(), 3.14);
    EXPECT_EQ(val.TypeName(), "float");
}

TEST(ScriptValue, StringConstruction) {
    ScriptValue from_literal("hello");
    ScriptValue from_string(std::string("world"));
    EXPECT_TRUE(from_literal.IsString());
    EXPECT_TRUE(from_string.IsString());
    EXPECT_EQ(from_literal.AsString(), "hello");
    EXPECT_EQ(from_string.AsString(), "world");
}

TEST(ScriptValue, ListConstruction) {
    auto val = ScriptValue::List({ScriptValue(1), ScriptValue(2), ScriptValue(3)});
    EXPECT_TRUE(val.IsList());
    EXPECT_EQ(val.AsList().size(), 3);
    EXPECT_EQ(val.AsList()[0].AsInt(), 1);
    EXPECT_EQ(val.AsList()[2].AsInt(), 3);
}

TEST(ScriptValue, MapConstruction) {
    auto val = ScriptValue::Map({{"name", ScriptValue("Alice")}, {"age", ScriptValue(30)}});
    EXPECT_TRUE(val.IsMap());
    EXPECT_EQ(val.AsMap().size(), 2);
    EXPECT_EQ(val.AsMap().at("name").AsString(), "Alice");
    EXPECT_EQ(val.AsMap().at("age").AsInt(), 30);
}

TEST(ScriptValue, ErrorConstruction) {
    auto val = ScriptValue::Error("something went wrong");
    EXPECT_TRUE(val.IsError());
    EXPECT_EQ(val.AsError().message, "something went wrong");
}

TEST(ScriptValue, FunctionConstruction) {
    ScriptValue val(FunctionValue{"add", {"a", "b"}});
    EXPECT_TRUE(val.IsFunction());
    EXPECT_TRUE(val.IsCallable());
    EXPECT_EQ(val.AsFunction().name, "add");
    EXPECT_EQ(val.AsFunction().params.size(), 2);
}

TEST(ScriptValue, NativeFunctionConstruction) {
    ScriptValue val(NativeFunction{[](std::vector<ScriptValue>) { return ScriptValue(42); }});
    EXPECT_TRUE(val.IsNativeFunction());
    EXPECT_TRUE(val.IsCallable());
}

// --- Truthiness ---

TEST(ScriptValue, Truthiness_NullIsFalsy) {
    EXPECT_FALSE(ScriptValue().IsTruthy());
}

TEST(ScriptValue, Truthiness_FalseIsFalsy) {
    EXPECT_FALSE(ScriptValue(false).IsTruthy());
}

TEST(ScriptValue, Truthiness_TrueIsTruthy) {
    EXPECT_TRUE(ScriptValue(true).IsTruthy());
}

TEST(ScriptValue, Truthiness_ZeroIsFalsy) {
    EXPECT_FALSE(ScriptValue(0).IsTruthy());
    EXPECT_FALSE(ScriptValue(0.0).IsTruthy());
}

TEST(ScriptValue, Truthiness_NonZeroIsTruthy) {
    EXPECT_TRUE(ScriptValue(1).IsTruthy());
    EXPECT_TRUE(ScriptValue(-1).IsTruthy());
    EXPECT_TRUE(ScriptValue(0.5).IsTruthy());
}

TEST(ScriptValue, Truthiness_EmptyStringIsFalsy) {
    EXPECT_FALSE(ScriptValue("").IsTruthy());
}

TEST(ScriptValue, Truthiness_NonEmptyStringIsTruthy) {
    EXPECT_TRUE(ScriptValue("hello").IsTruthy());
}

TEST(ScriptValue, Truthiness_EmptyListIsFalsy) {
    EXPECT_FALSE(ScriptValue::List().IsTruthy());
}

TEST(ScriptValue, Truthiness_NonEmptyListIsTruthy) {
    EXPECT_TRUE(ScriptValue::List({ScriptValue(1)}).IsTruthy());
}

TEST(ScriptValue, Truthiness_EmptyMapIsFalsy) {
    EXPECT_FALSE(ScriptValue::Map().IsTruthy());
}

TEST(ScriptValue, Truthiness_ErrorIsFalsy) {
    EXPECT_FALSE(ScriptValue::Error("err").IsTruthy());
}

TEST(ScriptValue, Truthiness_FunctionIsTruthy) {
    EXPECT_TRUE(ScriptValue(FunctionValue{"f", {}}).IsTruthy());
}

// --- Equality ---

TEST(ScriptValue, Equals_NullEqualsNull) {
    EXPECT_TRUE(ScriptValue().Equals(ScriptValue()));
}

TEST(ScriptValue, Equals_BoolValues) {
    EXPECT_TRUE(ScriptValue(true).Equals(ScriptValue(true)));
    EXPECT_FALSE(ScriptValue(true).Equals(ScriptValue(false)));
}

TEST(ScriptValue, Equals_IntValues) {
    EXPECT_TRUE(ScriptValue(42).Equals(ScriptValue(42)));
    EXPECT_FALSE(ScriptValue(42).Equals(ScriptValue(43)));
}

TEST(ScriptValue, Equals_FloatValues) {
    EXPECT_TRUE(ScriptValue(3.14).Equals(ScriptValue(3.14)));
    EXPECT_FALSE(ScriptValue(3.14).Equals(ScriptValue(2.72)));
}

TEST(ScriptValue, Equals_IntAndFloat) {
    EXPECT_TRUE(ScriptValue(5).Equals(ScriptValue(5.0)));
    EXPECT_TRUE(ScriptValue(5.0).Equals(ScriptValue(5)));
    EXPECT_FALSE(ScriptValue(5).Equals(ScriptValue(5.1)));
}

TEST(ScriptValue, Equals_StringValues) {
    EXPECT_TRUE(ScriptValue("hello").Equals(ScriptValue("hello")));
    EXPECT_FALSE(ScriptValue("hello").Equals(ScriptValue("world")));
}

TEST(ScriptValue, Equals_ListValues) {
    auto a = ScriptValue::List({ScriptValue(1), ScriptValue(2)});
    auto b = ScriptValue::List({ScriptValue(1), ScriptValue(2)});
    auto c = ScriptValue::List({ScriptValue(1), ScriptValue(3)});
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.Equals(c));
}

TEST(ScriptValue, Equals_MapValues) {
    auto a = ScriptValue::Map({{"x", ScriptValue(1)}});
    auto b = ScriptValue::Map({{"x", ScriptValue(1)}});
    auto c = ScriptValue::Map({{"x", ScriptValue(2)}});
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.Equals(c));
}

TEST(ScriptValue, Equals_DifferentTypes) {
    EXPECT_FALSE(ScriptValue(1).Equals(ScriptValue("1")));
    EXPECT_FALSE(ScriptValue(true).Equals(ScriptValue(1)));
    EXPECT_FALSE(ScriptValue().Equals(ScriptValue(false)));
}

// --- Arithmetic ---

TEST(ScriptValue, Add_Integers) {
    auto result = ScriptValue::Add(ScriptValue(3), ScriptValue(4));
    EXPECT_TRUE(result.IsInt());
    EXPECT_EQ(result.AsInt(), 7);
}

TEST(ScriptValue, Add_Floats) {
    auto result = ScriptValue::Add(ScriptValue(1.5), ScriptValue(2.5));
    EXPECT_TRUE(result.IsFloat());
    EXPECT_DOUBLE_EQ(result.AsFloat(), 4.0);
}

TEST(ScriptValue, Add_IntAndFloat) {
    auto result = ScriptValue::Add(ScriptValue(3), ScriptValue(0.5));
    EXPECT_TRUE(result.IsFloat());
    EXPECT_DOUBLE_EQ(result.AsFloat(), 3.5);
}

TEST(ScriptValue, Add_Strings) {
    auto result = ScriptValue::Add(ScriptValue("hello"), ScriptValue(" world"));
    EXPECT_TRUE(result.IsString());
    EXPECT_EQ(result.AsString(), "hello world");
}

TEST(ScriptValue, Add_StringAndNumber) {
    auto result = ScriptValue::Add(ScriptValue("count: "), ScriptValue(42));
    EXPECT_TRUE(result.IsString());
    EXPECT_EQ(result.AsString(), "count: 42");
}

TEST(ScriptValue, Add_Lists) {
    auto a = ScriptValue::List({ScriptValue(1), ScriptValue(2)});
    auto b = ScriptValue::List({ScriptValue(3)});
    auto result = ScriptValue::Add(a, b);
    EXPECT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 3);
}

TEST(ScriptValue, Add_IncompatibleTypes) {
    auto result = ScriptValue::Add(ScriptValue(1), ScriptValue::List());
    EXPECT_TRUE(result.IsError());
}

TEST(ScriptValue, Subtract_Integers) {
    auto result = ScriptValue::Subtract(ScriptValue(10), ScriptValue(3));
    EXPECT_EQ(result.AsInt(), 7);
}

TEST(ScriptValue, Subtract_Floats) {
    auto result = ScriptValue::Subtract(ScriptValue(5.5), ScriptValue(2.0));
    EXPECT_DOUBLE_EQ(result.AsFloat(), 3.5);
}

TEST(ScriptValue, Multiply_Integers) {
    auto result = ScriptValue::Multiply(ScriptValue(6), ScriptValue(7));
    EXPECT_EQ(result.AsInt(), 42);
}

TEST(ScriptValue, Divide_Integers) {
    auto result = ScriptValue::Divide(ScriptValue(10), ScriptValue(3));
    EXPECT_EQ(result.AsInt(), 3);
}

TEST(ScriptValue, Divide_Floats) {
    auto result = ScriptValue::Divide(ScriptValue(7.0), ScriptValue(2.0));
    EXPECT_DOUBLE_EQ(result.AsFloat(), 3.5);
}

TEST(ScriptValue, Divide_ByZero) {
    auto result = ScriptValue::Divide(ScriptValue(1), ScriptValue(0));
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.AsError().message, "Division by zero");
}

TEST(ScriptValue, Divide_FloatByZero) {
    auto result = ScriptValue::Divide(ScriptValue(1.0), ScriptValue(0.0));
    EXPECT_TRUE(result.IsError());
}

TEST(ScriptValue, Modulo_Integers) {
    auto result = ScriptValue::Modulo(ScriptValue(10), ScriptValue(3));
    EXPECT_EQ(result.AsInt(), 1);
}

TEST(ScriptValue, Modulo_ByZero) {
    auto result = ScriptValue::Modulo(ScriptValue(10), ScriptValue(0));
    EXPECT_TRUE(result.IsError());
}

TEST(ScriptValue, Modulo_Floats_IsError) {
    auto result = ScriptValue::Modulo(ScriptValue(10.0), ScriptValue(3.0));
    EXPECT_TRUE(result.IsError());
}

TEST(ScriptValue, Negate_Int) {
    auto result = ScriptValue::Negate(ScriptValue(5));
    EXPECT_EQ(result.AsInt(), -5);
}

TEST(ScriptValue, Negate_Float) {
    auto result = ScriptValue::Negate(ScriptValue(3.14));
    EXPECT_DOUBLE_EQ(result.AsFloat(), -3.14);
}

TEST(ScriptValue, Negate_String_IsError) {
    auto result = ScriptValue::Negate(ScriptValue("hello"));
    EXPECT_TRUE(result.IsError());
}

// --- Comparison ---

TEST(ScriptValue, LessThan_Integers) {
    EXPECT_TRUE(ScriptValue::LessThan(ScriptValue(1), ScriptValue(2)).AsBool());
    EXPECT_FALSE(ScriptValue::LessThan(ScriptValue(2), ScriptValue(1)).AsBool());
    EXPECT_FALSE(ScriptValue::LessThan(ScriptValue(1), ScriptValue(1)).AsBool());
}

TEST(ScriptValue, LessThan_Floats) {
    EXPECT_TRUE(ScriptValue::LessThan(ScriptValue(1.0), ScriptValue(2.0)).AsBool());
}

TEST(ScriptValue, LessThan_IntAndFloat) {
    EXPECT_TRUE(ScriptValue::LessThan(ScriptValue(1), ScriptValue(1.5)).AsBool());
}

TEST(ScriptValue, LessThan_Strings) {
    EXPECT_TRUE(ScriptValue::LessThan(ScriptValue("apple"), ScriptValue("banana")).AsBool());
}

TEST(ScriptValue, LessEqual_Integers) {
    EXPECT_TRUE(ScriptValue::LessEqual(ScriptValue(1), ScriptValue(1)).AsBool());
    EXPECT_TRUE(ScriptValue::LessEqual(ScriptValue(1), ScriptValue(2)).AsBool());
    EXPECT_FALSE(ScriptValue::LessEqual(ScriptValue(2), ScriptValue(1)).AsBool());
}

TEST(ScriptValue, GreaterThan_Integers) {
    EXPECT_TRUE(ScriptValue::GreaterThan(ScriptValue(2), ScriptValue(1)).AsBool());
    EXPECT_FALSE(ScriptValue::GreaterThan(ScriptValue(1), ScriptValue(2)).AsBool());
}

TEST(ScriptValue, GreaterEqual_Integers) {
    EXPECT_TRUE(ScriptValue::GreaterEqual(ScriptValue(2), ScriptValue(2)).AsBool());
    EXPECT_TRUE(ScriptValue::GreaterEqual(ScriptValue(3), ScriptValue(2)).AsBool());
}

TEST(ScriptValue, Compare_IncompatibleTypes) {
    auto result = ScriptValue::LessThan(ScriptValue(1), ScriptValue("hello"));
    EXPECT_TRUE(result.IsError());
}

// --- ToString ---

TEST(ScriptValue, ToString_Null) {
    EXPECT_EQ(ScriptValue().ToString(), "null");
}

TEST(ScriptValue, ToString_Bool) {
    EXPECT_EQ(ScriptValue(true).ToString(), "true");
    EXPECT_EQ(ScriptValue(false).ToString(), "false");
}

TEST(ScriptValue, ToString_Int) {
    EXPECT_EQ(ScriptValue(42).ToString(), "42");
}

TEST(ScriptValue, ToString_String) {
    EXPECT_EQ(ScriptValue("hello").ToString(), "hello");
}

TEST(ScriptValue, ToString_List) {
    auto val = ScriptValue::List({ScriptValue(1), ScriptValue("two"), ScriptValue(3)});
    EXPECT_EQ(val.ToString(), "[1, \"two\", 3]");
}

TEST(ScriptValue, ToString_Map) {
    auto val = ScriptValue::Map({{"a", ScriptValue(1)}, {"b", ScriptValue("two")}});
    EXPECT_EQ(val.ToString(), "{a: 1, b: \"two\"}");
}

TEST(ScriptValue, ToString_Error) {
    auto val = ScriptValue::Error("bad");
    EXPECT_EQ(val.ToString(), "<error: bad>");
}

TEST(ScriptValue, ToString_Function) {
    ScriptValue val(FunctionValue{"myFunc", {}});
    EXPECT_EQ(val.ToString(), "<fn myFunc>");
}

}  // namespace
}  // namespace lang
