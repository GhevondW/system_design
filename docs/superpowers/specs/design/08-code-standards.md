# Code Design Standards

All code in this project — C++, JavaScript, build scripts — must follow these standards. No exceptions.

## C++ Standards

### Language Version
- **C++23** minimum. Use the latest features available in GCC 14+ / Clang 18+ / Emscripten.
- Enable `-std=c++23 -Wall -Wextra -Wpedantic -Werror` in CMake.

### Naming Conventions (userver style)

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / Types | PascalCase | `EventQueue`, `HttpServer`, `ScriptValue` |
| Functions / Methods | PascalCase | `ProcessEvent()`, `LoadGraph()`, `RunAll()` |
| Variables / Parameters | snake_case | `event_queue`, `component_id`, `test_result` |
| Constants | `k` + PascalCase | `kMaxEvents`, `kDefaultTimeout`, `kNetworkLatency` |
| Enum values | `k` + PascalCase | `kRunning`, `kStopped`, `kAtLeastOnce` |
| Namespaces | lowercase | `engine`, `lang`, `components`, `bridge` |
| Files | snake_case | `event_queue.h`, `http_server.cpp`, `script_value.h` |
| Macros (avoid) | SCREAMING_SNAKE | `SYSLANG_ASSERT(...)` |
| Template params | PascalCase | `template <typename EventType>` |

### File Organization

```
src/module_name/
  module_name.h          // public API (include guard or #pragma once)
  module_name.cpp        // implementation
  module_name_test.cpp   // unit tests (colocated)
  impl/                  // internal details (not part of public API)
    detail.h
```

- One class per file (exceptions: small tightly-coupled types).
- Public headers expose only the interface. Implementation details go in `impl/` or anonymous namespaces.
- Include order: corresponding header, C++ standard library, third-party, project headers. Blank line between groups.

```cpp
#include "engine/event_queue.h"      // corresponding header first

#include <algorithm>                  // standard library
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>          // third-party

#include "engine/clock.h"            // project headers
#include "engine/event.h"
```

### Modern C++ Idioms

**RAII everywhere.** No manual `new`/`delete`. Use `std::unique_ptr`, `std::shared_ptr`, or stack allocation.

```cpp
// GOOD
auto component = std::make_unique<HttpServer>(config);

// BAD
HttpServer* component = new HttpServer(config);
```

**Value semantics by default.** Pass small types by value, large types by `const&`. Return by value (rely on NRVO/move).

```cpp
// GOOD
ScriptValue Evaluate(const Expression& expr);

// BAD
ScriptValue* Evaluate(Expression* expr);
```

**Use `[[nodiscard]]` on functions where ignoring the return value is a bug.**

```cpp
[[nodiscard]] ValidationResult ValidateGraph(const Graph& graph);
```

**Use `std::optional` for nullable returns, `std::expected` (C++23) for results with errors.**

```cpp
std::optional<ScriptValue> TryGetVariable(const std::string& name);
std::expected<TestResult, Error> RunTest(const TestCase& test);
```

**Use `std::variant` for sum types (event types, script values).**

```cpp
using Event = std::variant<
    NetworkRequest,
    NetworkResponse,
    QueueMessage,
    CacheOp,
    TimerFired,
    Failure
>;
```

**Use `constexpr` and `consteval` where possible.**

```cpp
constexpr std::chrono::milliseconds kDefaultNetworkLatency{5};
```

**Use `std::string_view` for non-owning string references.**

```cpp
void Log(std::string_view message);
```

**Use structured bindings.**

```cpp
auto [status, body] = ParseResponse(raw);
```

**Use `enum class` (never plain enums).**

```cpp
enum class ComponentState : uint8_t {
    kCreated,
    kInitialized,
    kRunning,
    kStopped
};
```

**Use concepts for template constraints (C++20).**

```cpp
template <typename T>
concept Serializable = requires(T t) {
    { t.ToJson() } -> std::convertible_to<nlohmann::json>;
};

template <Serializable T>
std::string Serialize(const T& obj);
```

### Design Patterns

**Dependency Injection.** Components receive their dependencies through constructor parameters, not globals.

```cpp
class SimulationEngine {
public:
    SimulationEngine(
        std::unique_ptr<EventQueue> event_queue,
        std::unique_ptr<ComponentRuntime> runtime,
        std::unique_ptr<Clock> clock
    );
};
```

**Strategy Pattern.** For interchangeable behaviors (load balancing, queue delivery).

```cpp
class LoadBalancingStrategy {
public:
    virtual ~LoadBalancingStrategy() = default;
    virtual ComponentId SelectTarget(const std::vector<ComponentId>& targets) = 0;
};

class RoundRobinStrategy : public LoadBalancingStrategy { ... };
class LeastConnectionsStrategy : public LoadBalancingStrategy { ... };
```

**Visitor Pattern.** For event dispatch and AST evaluation.

```cpp
struct EventVisitor {
    void operator()(const NetworkRequest& req) { ... }
    void operator()(const NetworkResponse& resp) { ... }
    void operator()(const Failure& fail) { ... }
};

std::visit(EventVisitor{runtime}, event);
```

**Builder Pattern.** For complex object construction (graphs, test cases).

```cpp
auto graph = GraphBuilder()
    .AddNode("server", ComponentType::kHttpServer)
    .AddNode("db", ComponentType::kDatabase)
    .AddEdge("server", "db", PortType::kDatabaseConnection)
    .Build();
```

**Observer Pattern.** For simulation state changes (UI updates, logging).

```cpp
class SimulationObserver {
public:
    virtual void OnEventProcessed(const Event& event) = 0;
    virtual void OnComponentStateChanged(ComponentId id, ComponentState state) = 0;
    virtual void OnLog(ComponentId id, std::string_view message) = 0;
};
```

### Error Handling

- Use `std::expected<T, Error>` for recoverable errors at API boundaries.
- Use exceptions only for truly exceptional / programmer-error situations.
- Never use exceptions in WASM-targeted code (Emscripten exception support is expensive). Use `std::expected` exclusively in the engine.
- Define a project-level `Error` type:

```cpp
struct Error {
    enum class Code : uint8_t {
        kParseError,
        kTypeError,
        kRuntimeError,
        kTimeout,
        kComponentNotFound,
        kPortTypeMismatch
    };

    Code code;
    std::string message;
    std::optional<SourceLocation> location;  // for script errors
};
```

### Testing

- Use **Google Test** for all C++ tests.
- One `_test.cpp` file per module, colocated with the source.
- Test names: `TEST(ClassName, MethodName_Condition_ExpectedResult)`.
- Use test fixtures for shared setup.
- Aim for unit tests on every public API. Integration tests for engine + components together.

```cpp
TEST(EventQueue, Pop_EmptyQueue_ReturnsNullopt) {
    EventQueue queue;
    EXPECT_EQ(queue.Pop(), std::nullopt);
}

TEST(EventQueue, Pop_MultipleEvents_ReturnsInTimestampOrder) {
    EventQueue queue;
    queue.Push(MakeEvent(/* timestamp */ 5));
    queue.Push(MakeEvent(/* timestamp */ 1));
    queue.Push(MakeEvent(/* timestamp */ 3));

    EXPECT_EQ(queue.Pop()->timestamp, 1);
    EXPECT_EQ(queue.Pop()->timestamp, 3);
    EXPECT_EQ(queue.Pop()->timestamp, 5);
}
```

---

## JavaScript Standards

### Language Version
- **ES2022+** (modules, private fields, top-level await).
- No TypeScript for MVP — keep the JS layer thin. Add TS in Phase 2 if the JS grows.

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes | PascalCase | `GraphEditor`, `CodePanel` |
| Functions / Methods | camelCase | `loadGraph()`, `onNodeSelect()` |
| Variables / Constants | camelCase | `selectedNode`, `canvasWidth` |
| Module-level constants | SCREAMING_SNAKE | `MAX_ZOOM`, `DEFAULT_PORT_RADIUS` |
| Files | kebab-case | `graph-editor.js`, `sim-controls.js` |
| CSS classes | kebab-case | `.graph-editor`, `.node-selected` |
| Event handlers | `on` + Event | `onNodeClick`, `onEdgeDrag` |

### File Organization

```
web/
  index.html
  js/
    graph-editor.js    // canvas-based graph editor
    code-editor.js     // Monaco integration
    sim-controls.js    // run/step/reset controls
    test-panel.js      // test results display
    console.js         // log output
    wasm-bridge.js     // wrapper around WASM C API
    problem-loader.js  // load problem definitions
    app.js             // entry point, wires everything together
  css/
    style.css          // single stylesheet for MVP
```

### Principles

- **No frameworks for MVP.** Vanilla JS + Canvas API. The JS layer is a thin rendering shell — all logic lives in WASM.
- **ES modules** (`import`/`export`). No bundler for MVP — use native browser module support.
- **Strict mode** (`"use strict"` or implicit via modules).
- **No global state.** Each module exports a class or factory function. `app.js` wires them together.
- **JSDoc comments** on public functions (since no TypeScript):

```js
/**
 * @param {string} graphJson - JSON string of the graph definition
 * @returns {boolean} true if the graph loaded successfully
 */
export function loadGraph(graphJson) { ... }
```

- **Event-driven communication** between UI modules (custom events or a simple pub/sub).
- **`const` by default.** Use `let` only when reassignment is needed. Never `var`.
- **Early returns** over deep nesting.

---

## Cross-Language Principles

These apply to ALL code regardless of language:

### Modularity
- Each module has **one clear responsibility**.
- Modules communicate through **well-defined interfaces** (pure virtual classes in C++, exported functions in JS).
- A module's internals can change without affecting its consumers.
- If a file exceeds ~300 lines, it's likely doing too much — split it.

### No Dead Code
- No commented-out code in version control.
- No unused functions, variables, or includes.
- No `// TODO: maybe later` without an associated issue.

### Naming Is Documentation
- Names should make comments unnecessary.
- `ProcessNextEvent()` not `Process()`. `kMaxEventsPerStep` not `kMax`.
- If you need a comment to explain what a variable is, rename it instead.

### Separation of Concerns
```
  [Simulation Engine]  -- knows nothing about UI or network
        |
  [WASM Bridge]        -- thin translation layer (JSON in/out)
        |
  [JS UI]              -- knows nothing about simulation internals
```

Each layer depends only on the layer directly below it. No circular dependencies. The engine is testable without any UI or bridge code.

### Formatting
- **C++**: clang-format (based on Google style, modified for 4-space indent).
- **JS**: Prettier with defaults.
- Both enforced in CI. No style debates in code review.
