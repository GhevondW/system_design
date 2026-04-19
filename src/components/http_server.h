#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component.h"
#include "lang/interpreter.h"

namespace components {

using RequestHandler = std::function<lang::ScriptValue(lang::ScriptValue)>;

class HttpServer : public engine::Component {
public:
    explicit HttpServer(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Route management
    void AddRoute(const std::string& method, const std::string& path, RequestHandler handler);

    // Set interpreter for running user code
    void SetInterpreter(lang::Interpreter* interpreter);

    // Handle a request and return response
    lang::ScriptValue HandleRequest(const std::string& method, const std::string& path,
                                    const lang::ScriptValue& body);

private:
    struct Route {
        std::string method;
        std::string path;
        RequestHandler handler;
    };

    std::vector<Route> routes_;
    lang::Interpreter* interpreter_ = nullptr;
};

}  // namespace components
