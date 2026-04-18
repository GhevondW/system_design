#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "lang/value.h"

namespace lang {

class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr);

    void Define(const std::string& name, ScriptValue value);
    [[nodiscard]] std::optional<ScriptValue> Get(const std::string& name) const;
    bool Set(const std::string& name, ScriptValue value);

    [[nodiscard]] std::shared_ptr<Environment> CreateChild();

private:
    std::unordered_map<std::string, ScriptValue> variables_;
    std::shared_ptr<Environment> parent_;
};

}  // namespace lang
