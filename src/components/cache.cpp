#include "components/cache.h"

namespace components {

Cache::Cache(engine::ComponentId id) : Component(std::move(id), engine::ComponentType::kCache) {}

std::vector<engine::Event> Cache::HandleEvent(const engine::Event& /*event*/,
                                              engine::Timestamp /*current_time*/) {
    return {};
}

lang::ScriptValue Cache::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});
}

void Cache::Reset() {
    store_.clear();
}

lang::ScriptValue Cache::Get(const std::string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) return lang::ScriptValue::Null();
    return it->second.value;
}

void Cache::Set(const std::string& key, lang::ScriptValue value, int64_t ttl) {
    store_[key] = CacheEntry{std::move(value), ttl, 0};
}

bool Cache::Del(const std::string& key) {
    return store_.erase(key) > 0;
}

bool Cache::Has(const std::string& key) const {
    return store_.count(key) > 0;
}

}  // namespace components
