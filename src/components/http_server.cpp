#include "components/http_server.h"

namespace components {

HttpServer::HttpServer(engine::ComponentId id)
    : Component(std::move(id), engine::ComponentType::kHttpServer) {}

std::vector<engine::Event> HttpServer::HandleEvent(const engine::Event& event,
                                                    engine::Timestamp current_time) {
    if (auto* req = std::get_if<engine::NetworkRequest>(&event)) {
        auto response_body = HandleRequest(req->method, req->path, req->body);

        int status = 200;
        lang::ScriptValue body = response_body;
        if (response_body.IsMap()) {
            auto& map = response_body.AsMap();
            auto status_it = map.find("status");
            if (status_it != map.end() && status_it->second.IsInt()) {
                status = static_cast<int>(status_it->second.AsInt());
            }
            auto body_it = map.find("body");
            if (body_it != map.end()) {
                body = body_it->second;
            }
        }

        engine::NetworkResponse resp;
        resp.header.from = GetId();
        resp.header.to = req->header.from;
        resp.header.timestamp = current_time + 1;
        resp.header.id = req->header.id + 1000;
        resp.status = status;
        resp.body = std::move(body);
        resp.request_id = req->header.id;

        return {std::move(resp)};
    }
    return {};
}

lang::ScriptValue HttpServer::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});
}

void HttpServer::Reset() {
    // Preserve routes — they're part of the design, not runtime state
}

void HttpServer::AddRoute(const std::string& method, const std::string& path,
                          RequestHandler handler) {
    routes_.push_back(Route{method, path, "", std::move(handler)});
}

void HttpServer::AddRoute(const std::string& method, const std::string& path,
                          const std::string& handler_name) {
    routes_.push_back(Route{method, path, handler_name, nullptr});
}

void HttpServer::SetInterpreter(lang::Interpreter* interpreter) {
    interpreter_ = interpreter;
}

lang::ScriptValue HttpServer::HandleRequest(const std::string& method, const std::string& path,
                                            const lang::ScriptValue& body) {
    for (const auto& route : routes_) {
        if (route.method == method && route.path == path) {
            lang::ScriptValue req(lang::ScriptMap{
                {"method", lang::ScriptValue(method)},
                {"path", lang::ScriptValue(path)},
                {"body", body},
                {"params", lang::ScriptValue(lang::ScriptMap{})},
            });

            // If handler_name is set, resolve via interpreter
            if (!route.handler_name.empty() && interpreter_) {
                auto fn_opt = interpreter_->GetGlobalEnv()->Get(route.handler_name);
                if (fn_opt.has_value() && fn_opt->IsNativeFunction()) {
                    return fn_opt->AsNativeFunction()(std::vector<lang::ScriptValue>{req});
                }
                return lang::ScriptValue(lang::ScriptMap{
                    {"status", lang::ScriptValue(500)},
                    {"body", lang::ScriptValue("Handler not found: " + route.handler_name)},
                });
            }

            // Direct callable handler
            if (route.handler) {
                return route.handler(req);
            }
        }
    }
    return lang::ScriptValue(lang::ScriptMap{
        {"status", lang::ScriptValue(404)},
        {"body", lang::ScriptValue("Not found: " + method + " " + path)},
    });
}

}  // namespace components
