#pragma once

#include <string>
#include <unordered_map>

#include "engine/component.h"

namespace components {

class Cache : public engine::Component {
public:
    explicit Cache(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Direct API
    lang::ScriptValue Get(const std::string& key);
    void Set(const std::string& key, lang::ScriptValue value, int64_t ttl = 0);
    bool Del(const std::string& key);
    bool Has(const std::string& key) const;

private:
    struct CacheEntry {
        lang::ScriptValue value;
        int64_t ttl = 0;           // 0 = no expiry
        int64_t created_at = 0;    // virtual time when set
    };

    std::unordered_map<std::string, CacheEntry> store_;
};

}  // namespace components
