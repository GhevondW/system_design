#include "lang/environment.h"

namespace lang {

Environment::Environment(std::shared_ptr<Environment> parent) : parent_(std::move(parent)) {}

void Environment::Define(const std::string& name, ScriptValue value) {
    variables_[name] = std::move(value);
}

std::optional<ScriptValue> Environment::Get(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) return it->second;
    if (parent_) return parent_->Get(name);
    return std::nullopt;
}

bool Environment::Set(const std::string& name, ScriptValue value) {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        it->second = std::move(value);
        return true;
    }
    if (parent_) return parent_->Set(name, value);
    return false;
}

std::shared_ptr<Environment> Environment::CreateChild() {
    return std::make_shared<Environment>(shared_from_this());
}

}  // namespace lang
