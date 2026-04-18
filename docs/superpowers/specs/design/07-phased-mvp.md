# Phased MVP Plan

## Phase 1 — Core Engine (Ship First)

**Goal**: A working simulation that runs in the browser. Users can solve problems locally.

### Deliverables
- Simulation engine (event queue + Cortex fibers)
- SysLang interpreter (variables, functions, lambdas, control flow, lists, maps)
- 4 component types: Client, HTTP Server, Database, Cache
- 3-5 beginner problems (car registration, URL shortener, key-value store)
- Web UI: graph editor, code editor, run/step buttons, test output panel
- Everything runs in browser via WASM — no backend needed
- Problems bundled as static JSON files

### Architecture Scope
```
  [Browser Only]

  JS UI  <-->  WASM Engine
                  |
           [Simulation Engine]
           [SysLang Interpreter]
           [Bundled Problems]
```

### Exit Criteria
- User can open the app, pick a problem, wire components, write handlers, run tests, and see pass/fail
- Step-through mode animates message flow between components
- At least 3 problems are solvable end-to-end

---

## Phase 2 — Backend & Accounts

**Goal**: Persistent user experience. Server-side validation prevents cheating.

### Deliverables
- C++ REST API backend (Crow or Drogon)
- PostgreSQL database with schema from 06-backend.md
- User registration and login (JWT auth)
- Save/load solutions
- Server-side validation (same engine, native build)
- Problem browser with difficulty filters
- Add Message Queue and Worker components
- 5-10 more problems across beginner and intermediate tiers

### Architecture Scope
```
  [Browser]  <---HTTP--->  [C++ Backend]  <--->  [PostgreSQL]
      |
  WASM Engine (local debugging still works)
```

### Exit Criteria
- Users can register, log in, and their progress persists
- Submitting a solution validates server-side and records results
- Problem browser shows solve status per user

---

## Phase 3 — Full Platform

**Goal**: Community and competition features. Self-sustaining content pipeline.

### Deliverables
- Leaderboards (global, per-problem)
- Community problem submission with review workflow
- Analytics dashboard (solve rates, common failure patterns)
- Load Balancer and DNS/Service Registry components
- Advanced problems with failure injection (network partitions, crashes)
- Performance scoring (not just correctness)
- Solution sharing / discussion threads

### Architecture Scope
```
  [Browser]  <---HTTP/WS--->  [C++ Backend]  <--->  [PostgreSQL]
                                    |
                              [Admin Panel]
                              [Analytics]
```

### Exit Criteria
- Leaderboard displays rankings
- Users can submit problems that go through a review pipeline
- At least 3 advanced problems with failure injection are solvable

---

## Cross-Phase Concerns

### Testing Strategy
- **Engine**: Unit tests for each component type, event queue, fiber scheduler
- **Interpreter**: Test suite for every SysLang feature
- **Integration**: End-to-end tests that load a problem and validate a known solution
- **WASM**: Same test suite compiled to WASM and run in a headless browser
- **Backend**: API integration tests with test database

### Build System
- CMake for all C++ code
- Emscripten for WASM compilation
- Single codebase, two build targets (native + WASM)
- CI: build both targets, run both test suites

### Directory Structure (Proposed)
```
system_design/
  docs/                      # design docs (you are here)
  src/
    engine/                  # simulation engine (event queue, scheduler)
    components/              # component type implementations
    lang/                    # SysLang interpreter (lexer, parser, VM)
    bridge/                  # JS <-> WASM bridge (C API)
    server/                  # backend REST API
  web/                       # frontend JS/HTML/CSS
  problems/                  # problem definitions
  tests/                     # test suites
  CMakeLists.txt
```
