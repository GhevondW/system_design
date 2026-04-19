# SystemForge — LeetCode for System Design

[![CI](https://github.com/GhevondW/system_design/actions/workflows/ci.yml/badge.svg)](https://github.com/GhevondW/system_design/actions/workflows/ci.yml)
[![Deploy](https://github.com/GhevondW/system_design/actions/workflows/deploy.yml/badge.svg)](https://github.com/GhevondW/system_design/actions/workflows/deploy.yml)

**Live:** [https://ghevondw.github.io/system_design/](https://ghevondw.github.io/system_design/)

An interactive platform where engineers learn system design by building real systems. Users solve problems by wiring together components (servers, databases, queues, caches) in a visual graph editor and writing handler logic in SysLang — a small, C++-flavored scripting language. The system simulates execution and validates correctness via test cases.

```
+-----------------------------------------------------+
|                    WEB CLIENT                        |
|  [Graph Editor] [Code Editor] [Problem Browser]     |
|         thin JS  <---->  WASM Engine (C++)           |
+-----------------------------------------------------+
            |  HTTP / WebSocket  |
+-----------------------------------------------------+
|                  C++ BACKEND                         |
|  [REST API] [Auth] [Problem Store] [Leaderboards]   |
|  [Solution Validator] [Simulation Runner]            |
|                      |                               |
|               [PostgreSQL DB]                        |
+-----------------------------------------------------+
```

## Design Documents

Full spec lives in [`docs/superpowers/specs/design/`](docs/superpowers/specs/design/):

| Doc | Contents |
|-----|----------|
| [00-overview](docs/superpowers/specs/design/00-overview.md) | Vision, tech stack, architecture |
| [01-component-model](docs/superpowers/specs/design/01-component-model.md) | 8 component types, ports, APIs, graph edges |
| [02-scripting-language](docs/superpowers/specs/design/02-scripting-language.md) | SysLang syntax and semantics |
| [03-simulation-engine](docs/superpowers/specs/design/03-simulation-engine.md) | Hybrid event queue + Cortex fiber engine |
| [04-problem-format](docs/superpowers/specs/design/04-problem-format.md) | Problem definitions, test cases, validation |
| [05-web-client](docs/superpowers/specs/design/05-web-client.md) | Browser architecture, UI, WASM bridge |
| [06-backend](docs/superpowers/specs/design/06-backend.md) | REST API, DB schema, server-side validation |
| [07-phased-mvp](docs/superpowers/specs/design/07-phased-mvp.md) | 3-phase delivery plan |
| [08-code-standards](docs/superpowers/specs/design/08-code-standards.md) | C++23, userver style, JS standards, patterns |

---

## Phase 1 Implementation Plan

Phase 1 delivers the core engine running entirely in the browser. No backend. 10 steps, each self-contained and buildable.

---

### Step 1: Project Scaffolding & Build System

**Goal**: CMake project that builds both a native binary and a WASM module.

**What to do**:
- Create the directory structure (see below)
- Set up `CMakeLists.txt` with two targets: `systemforge` (native) and `systemforge_wasm` (Emscripten)
- Integrate Cortex as a Git submodule under `third_party/`
- Add nlohmann/json as header-only dep under `third_party/`
- Add Google Test for the native target
- Create a minimal `src/main.cpp` that prints "SystemForge" (native) and a `src/bridge/bridge.cpp` that exports a test function (WASM)
- Add a `.clang-format` config (Google style, 4-space indent)
- Verify: `cmake --build build-native` and `cmake --build build-wasm` both succeed

**Directory structure**:
```
system_design/
  CMakeLists.txt
  .clang-format
  third_party/
    cortex/                  # git submodule
    nlohmann/json.hpp        # header-only
  src/
    engine/                  # simulation engine
    components/              # component implementations
    lang/                    # SysLang interpreter
    bridge/                  # WASM C API bridge
    server/                  # backend (Phase 2)
    main.cpp                 # native entry point
  web/
    index.html
    js/
    css/
  problems/
  tests/
  docs/
```

**References**:
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — file org, naming, includes
- Spec: [07-phased-mvp.md](docs/superpowers/specs/design/07-phased-mvp.md) — directory structure
- Cortex repo: https://github.com/GhevondW/cortex — build instructions, WASM setup
- Emscripten CMake: https://emscripten.org/docs/compiling/Building-Projects.html
- nlohmann/json: https://github.com/nlohmann/json

---

### Step 2: SysLang Value System & Lexer

**Goal**: Tokenizer and dynamic value types for the scripting language.

**What to do**:
- Implement `ScriptValue` — a `std::variant` holding: int64, double, bool, string, list (`std::vector<ScriptValue>`), map (`ordered_map<string, ScriptValue>`), function, null, error
- Implement `Lexer` — tokenize SysLang source into a stream of tokens
- Token types: `let`, `fn`, `if`, `else`, `for`, `while`, `match`, `return`, identifiers, numbers, strings, operators (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`), punctuation (`(`, `)`, `{`, `}`, `[`, `]`, `:`, `;`, `,`, `.`, `=>`, `->`)
- Write unit tests for value operations (arithmetic, comparison, truthiness) and lexer (all token types, edge cases)
- Verify: all tests pass

**Files**:
```
src/lang/value.h           # ScriptValue variant type
src/lang/value.cpp
src/lang/lexer.h           # Lexer class
src/lang/lexer.cpp
src/lang/token.h           # Token type enum + Token struct
tests/lang/value_test.cpp
tests/lang/lexer_test.cpp
```

**References**:
- Spec: [02-scripting-language.md](docs/superpowers/specs/design/02-scripting-language.md) — all syntax to tokenize
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — use `std::variant`, `std::string_view`, naming conventions
- Reference: Crafting Interpreters by Robert Nystrom (lexer chapter) — https://craftinginterpreters.com/scanning.html

---

### Step 3: SysLang Parser & AST

**Goal**: Parse SysLang tokens into an abstract syntax tree.

**What to do**:
- Define AST node types:
  - Expressions: `Literal`, `Identifier`, `BinaryOp`, `UnaryOp`, `Call`, `MemberAccess`, `IndexAccess`, `Lambda`, `MatchExpr`, `MapLiteral`, `ListLiteral`
  - Statements: `LetDecl`, `FnDecl`, `IfStmt`, `ForStmt`, `WhileStmt`, `ReturnStmt`, `ExprStmt`, `Block`
- Implement recursive descent parser: `ParseProgram()` → `ParseStatement()` → `ParseExpression()` with Pratt parsing for operator precedence
- Write unit tests: parse every example from the language spec, verify AST structure
- Verify: all language spec examples parse without errors

**Files**:
```
src/lang/ast.h             # AST node types (using std::variant or class hierarchy)
src/lang/parser.h          # Parser class
src/lang/parser.cpp
tests/lang/parser_test.cpp
```

**References**:
- Spec: [02-scripting-language.md](docs/superpowers/specs/design/02-scripting-language.md) — complete syntax reference
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — use Visitor pattern for AST
- Reference: Crafting Interpreters (parser chapter) — https://craftinginterpreters.com/parsing-expressions.html
- Reference: Pratt parsing — https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html

---

### Step 4: SysLang Interpreter (Tree-Walk)

**Goal**: Execute SysLang AST. All language features working.

**What to do**:
- Implement `Environment` — scope chain with variable lookup (lexical scoping, closures)
- Implement `Interpreter` — tree-walk evaluator using Visitor pattern over AST
- Support all language features:
  - `let` declarations, assignment, variable lookup
  - `fn` declarations, function calls, closures, recursion
  - Lambdas with capture
  - `if/else`, `for`, `while`, `return` (early return via exception or special value)
  - `match` expressions
  - List/map operations: index, member access, `.push()`, `.size()`, `.has()`, etc.
  - String operations: `.length()`, `.contains()`, `.split()`, `.upper()`, `.substr()`
  - Built-ins: `log()`, `print()`, `len()`, `str()`, `int()`, `float()`
- Implement component API injection: `Interpreter::InjectVariable(name, callable)` — used later to inject `db`, `cache`, etc.
- Write comprehensive tests for every feature
- Verify: every code example from [02-scripting-language.md] runs and produces correct output

**Files**:
```
src/lang/environment.h     # Scope chain
src/lang/environment.cpp
src/lang/interpreter.h     # Tree-walk evaluator
src/lang/interpreter.cpp
src/lang/builtins.h        # Built-in functions (log, len, str, etc.)
src/lang/builtins.cpp
tests/lang/interpreter_test.cpp
```

**References**:
- Spec: [02-scripting-language.md](docs/superpowers/specs/design/02-scripting-language.md) — every feature to implement
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — DI, Visitor pattern, `std::expected` for errors
- Reference: Crafting Interpreters (evaluation chapters) — https://craftinginterpreters.com/evaluating-expressions.html

---

### Step 5: Event System & Simulation Clock

**Goal**: Core event infrastructure for the simulation engine.

**What to do**:
- Define `Event` as `std::variant<NetworkRequest, NetworkResponse, QueueMessage, CacheOp, TimerFired, Failure>` with common fields (timestamp, from, to)
- Implement `EventQueue` — priority queue ordered by virtual timestamp (min-heap)
- Implement `SimulationClock` — tracks current virtual time, advances on `EventQueue::Pop()`
- Implement `LatencyConfig` — configurable latency per connection type (network, db, cache)
- Write unit tests: enqueue/dequeue ordering, clock advancement, latency application
- Verify: all tests pass

**Files**:
```
src/engine/event.h          # Event variant + concrete event types
src/engine/event_queue.h    # Priority queue
src/engine/event_queue.cpp
src/engine/clock.h          # Virtual simulation clock
src/engine/clock.cpp
tests/engine/event_queue_test.cpp
tests/engine/clock_test.cpp
```

**References**:
- Spec: [03-simulation-engine.md](docs/superpowers/specs/design/03-simulation-engine.md) — event types, simulated time, deterministic replay
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — `std::variant`, `enum class`, `constexpr` for defaults

---

### Step 6: Component Runtime & Implementations

**Goal**: The 4 MVP component types, fully functional.

**What to do**:
- Define `Component` base class: id, type, state (lifecycle), ports, fiber pool reference
- Define `Port` types and `Graph` (adjacency list of components + typed edges)
- Implement connection variable injection: when graph wires A→B, inject B's API into A's interpreter scope
- Implement **Client**: `Send()` enqueues `NetworkRequest`, `Expect()` records assertion
- Implement **HttpServer**: route table, `AddRoute()`, dispatches to user handler via interpreter
- Implement **Database**: in-memory table storage, basic SQL parsing (`SELECT`, `INSERT`, `UPDATE`, `DELETE` with `WHERE`), `CreateTable()` with schema
- Implement **Cache**: in-memory `std::unordered_map` with TTL tracking, `Get()`/`Set()`/`Del()`/`Has()`
- Write unit tests for each component in isolation (mock event queue)
- Verify: each component correctly handles its API calls

**Files**:
```
src/engine/component.h       # Base Component class
src/engine/graph.h           # Graph structure (nodes + edges)
src/engine/graph.cpp
src/components/client.h
src/components/client.cpp
src/components/http_server.h
src/components/http_server.cpp
src/components/database.h
src/components/database.cpp
src/components/cache.h
src/components/cache.cpp
tests/components/client_test.cpp
tests/components/http_server_test.cpp
tests/components/database_test.cpp
tests/components/cache_test.cpp
```

**References**:
- Spec: [01-component-model.md](docs/superpowers/specs/design/01-component-model.md) — ports, APIs, variable injection
- Spec: [03-simulation-engine.md](docs/superpowers/specs/design/03-simulation-engine.md) — component lifecycle
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — Strategy pattern (for future LB), DI, Observer

---

### Step 7: Simulation Engine (Orchestrator)

**Goal**: Wire everything together. Event queue + components + Cortex fibers = working simulation.

**What to do**:
- Implement `SimulationEngine` — the main orchestrator:
  - `LoadGraph(json)` — parse graph JSON, instantiate components, wire connections
  - `LoadCode(component_id, source)` — parse + attach SysLang code to a component
  - Main loop: pop event → deliver to target component → spawn Cortex fiber → run handler → collect new events
  - `RunAll()` — drain queue at full speed, return results
  - `StepEvent()` — pop one event, execute, return state snapshot
  - `StepFiber()` — advance one fiber by one yield point, return state snapshot
  - `Reset()` — restore to initial state
  - `GetState()` — snapshot: queue contents, component states, active fibers, logs
- Integrate Cortex: each request handler runs in a fiber, yields on cross-component calls (db.query, cache.get, etc.)
- Implement timeout: max events per run, max virtual time
- Implement `SimulationObserver` interface for logging/UI hooks
- Write integration tests: load a simple graph (client → server → db), send a request, verify full round-trip
- Verify: end-to-end test with car registration scenario passes

**Files**:
```
src/engine/simulation_engine.h
src/engine/simulation_engine.cpp
src/engine/simulation_observer.h   # Observer interface
src/engine/state_snapshot.h        # SimState struct (for stepping)
tests/engine/simulation_engine_test.cpp
```

**References**:
- Spec: [03-simulation-engine.md](docs/superpowers/specs/design/03-simulation-engine.md) — execution flow, stepping modes, failure injection, replay
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — DI, Observer pattern
- Cortex API: https://github.com/GhevondW/cortex — fiber creation, yield, scheduler step API

---

### Step 8: Test Runner & Validation

**Goal**: Load problem test cases, run them against user solutions, report pass/fail.

**What to do**:
- Implement `TestRunner`:
  - `LoadTestCases(json)` — parse test case definitions
  - `RunAll(engine)` — execute each test case sequentially, collect results
  - `RunSingle(engine, test_id)` — run one test
- Implement test step execution: parse `client.send(...)` actions, feed them to the simulation engine
- Implement assertion checking:
  - Response assertions: `status == N`, `body.field == value`
  - State assertions: `db.table(name).count()`, `cache.has(key)`
- Implement `TestResult` struct: pass/fail, expected, actual, logs, execution time
- Write tests: load car registration test cases, run against a correct solution, verify all pass. Run against a buggy solution, verify correct failures.
- Verify: test runner correctly identifies passing and failing solutions

**Files**:
```
src/engine/test_runner.h
src/engine/test_runner.cpp
src/engine/assertions.h
src/engine/assertions.cpp
src/engine/test_result.h
tests/engine/test_runner_test.cpp
```

**References**:
- Spec: [04-problem-format.md](docs/superpowers/specs/design/04-problem-format.md) — test case format, assertion types, validation flow
- Spec: [08-code-standards.md](docs/superpowers/specs/design/08-code-standards.md) — `std::expected` for results

---

### Step 9: WASM Bridge

**Goal**: Expose the C++ engine to JavaScript via a C API compiled to WebAssembly.

**What to do**:
- Implement C API functions (exported via `EMSCRIPTEN_KEEPALIVE`):
  - `engine_create()` → handle
  - `engine_destroy(handle)`
  - `engine_load_graph(handle, json_ptr, json_len)`
  - `engine_load_code(handle, component_id_ptr, code_ptr, code_len)`
  - `engine_run_all(handle)` → JSON result (ptr + len)
  - `engine_step_event(handle)` → JSON state (ptr + len)
  - `engine_step_fiber(handle)` → JSON state (ptr + len)
  - `engine_reset(handle)`
  - `engine_get_state(handle)` → JSON
  - `engine_get_logs(handle)` → JSON
  - `engine_validate_graph(handle)` → JSON
  - `engine_free_string(ptr)` — free JSON strings allocated on WASM side
- Implement `wasm-bridge.js` — JS wrapper that manages memory, converts strings, provides a clean async API
- All data crosses the boundary as JSON strings
- Write tests: call bridge functions from Node.js, verify JSON round-trips
- Verify: load a graph, run tests, get results — all through the bridge

**Files**:
```
src/bridge/bridge.h
src/bridge/bridge.cpp
web/js/wasm-bridge.js       # JS wrapper around the raw C API
tests/bridge/bridge_test.cpp  # native test that exercises the same code path
```

**References**:
- Spec: [05-web-client.md](docs/superpowers/specs/design/05-web-client.md) — bridge API specification
- Emscripten C API: https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html
- Emscripten memory: https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#access-memory-from-javascript

---

### Step 10: Web UI — Graph Editor

**Goal**: Visual drag-and-drop graph editor in the browser.

**What to do**:
- Implement `GraphEditor` class (HTML5 Canvas):
  - Component palette sidebar: drag Client, HTTPServer, Database, Cache onto canvas
  - Render nodes as labeled rectangles with input/output port circles
  - Draw edges: click output port → drag → release on compatible input port
  - Type checking: only allow compatible port connections (visual feedback on hover)
  - Select node: highlight, show properties. Double-click: open code editor
  - Delete: select + backspace removes node/edge
  - Pan and zoom on the canvas
  - Visual states: idle (gray border), running (green), error (red)
- Serialize graph to JSON matching the format the engine expects
- Load initial graph from problem definition
- Verify: can build the car registration graph visually, exports valid JSON, loads in engine

**Files**:
```
web/index.html
web/js/graph-editor.js
web/js/node-renderer.js      # draw individual nodes/ports
web/js/edge-renderer.js      # draw edges with animations
web/css/style.css
```

**References**:
- Spec: [05-web-client.md](docs/superpowers/specs/design/05-web-client.md) — UI components, layout
- Spec: [01-component-model.md](docs/superpowers/specs/design/01-component-model.md) — port types, graph structure
- Reference: HTML5 Canvas API — https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API

---

### Step 11: Web UI — Code Editor, Panels & Integration

**Goal**: Complete the UI. Wire everything together. Ship Phase 1.

**What to do**:
- Integrate Monaco Editor for SysLang:
  - Load from CDN
  - Register SysLang language: keywords, operators, comment syntax
  - Per-component code tabs
  - Basic autocomplete for connected APIs (db.query, cache.get, etc.)
- Implement simulation controls panel:
  - Run button → calls `wasm_bridge.runAll()`, displays results
  - Step (Event) button → calls `wasm_bridge.stepEvent()`, animates
  - Step (Line) button → calls `wasm_bridge.stepFiber()`, highlights code
  - Reset button
  - Speed slider for animation
- Implement test results panel: per-test pass/fail list, expandable details, expected vs actual
- Implement console panel: `log()` output, event trace (timestamped)
- Implement problem browser: list 3 problems, click to load graph + description
- Render problem description as markdown
- Implement responsive 4-panel layout (graph | code | description | console)
- Animated mode: during step-through, highlight active edges, show message dots flowing
- Write 3 complete beginner problems:
  - **Car Registration**: POST/GET with HTTPServer + Database
  - **URL Shortener**: generate codes, redirect, HTTPServer + Database + Cache
  - **Key-Value Store**: GET/SET/DELETE API with HTTPServer + Cache
- Bundle problems as static JSON in `web/`
- Verify: open in browser, solve all 3 problems end-to-end, all tests pass, step-through animation works

**Files**:
```
web/js/code-editor.js
web/js/sim-controls.js
web/js/test-panel.js
web/js/console.js
web/js/problem-loader.js
web/js/app.js                # entry point, wires all modules
problems/car-registration/   # manifest, description, tests, solution
problems/url-shortener/
problems/key-value-store/
```

**References**:
- Spec: [05-web-client.md](docs/superpowers/specs/design/05-web-client.md) — all UI components, layout, bridge API
- Spec: [04-problem-format.md](docs/superpowers/specs/design/04-problem-format.md) — problem structure, test format
- Spec: [02-scripting-language.md](docs/superpowers/specs/design/02-scripting-language.md) — syntax for Monaco highlighting
- Monaco Editor: https://microsoft.github.io/monaco-editor/
- Marked.js (markdown rendering): https://marked.js.org/

---

## Phase 1 Exit Criteria

- [ ] `cmake --build` succeeds for both native and WASM targets
- [ ] All C++ unit tests pass (`ctest`)
- [ ] Open `web/index.html` in a browser
- [ ] Select a problem from the browser
- [ ] Drag components onto the canvas, draw edges
- [ ] Write SysLang handler code
- [ ] Click "Run" — all tests pass for a correct solution
- [ ] Click "Step" — animated message flow between components
- [ ] All 3 problems are solvable end-to-end

---

## Tech Stack & Dependencies

| Dependency | Purpose | Link |
|-----------|---------|------|
| Cortex | C++ coroutine/fiber runtime + WASM | https://github.com/GhevondW/cortex |
| Emscripten | C++ → WebAssembly compiler | https://emscripten.org |
| nlohmann/json | JSON parsing (header-only C++) | https://github.com/nlohmann/json |
| Google Test | C++ unit testing | https://github.com/google/googletest |
| Monaco Editor | Code editor (browser, from CDN) | https://microsoft.github.io/monaco-editor |
| Marked.js | Markdown rendering (browser, from CDN) | https://marked.js.org |
