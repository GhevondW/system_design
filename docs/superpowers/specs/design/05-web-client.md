# Web Client Architecture

## Overview

The browser client has two layers:
- **Thin JS layer** — UI rendering, graph editor canvas, user interaction
- **WASM engine** — simulation, scripting interpreter, local test runner (C++ via Cortex)

```
+---------------------------------------------------------+
|                      BROWSER                            |
|                                                         |
|  +---------------------------------------------------+ |
|  |                  JS UI Layer                       | |
|  |                                                    | |
|  |  +-----------+  +-----------+  +---------------+  | |
|  |  |  Graph    |  |  Code     |  |  Problem      |  | |
|  |  |  Editor   |  |  Editor   |  |  Browser      |  | |
|  |  |  (Canvas) |  |  (Monaco) |  |  (List/View)  |  | |
|  |  +-----------+  +-----------+  +---------------+  | |
|  |                                                    | |
|  |  +-----------+  +-----------+  +---------------+  | |
|  |  | Sim       |  |  Test     |  |  Console /    |  | |
|  |  | Controls  |  |  Results  |  |  Logs Panel   |  | |
|  |  | (Run/Step)|  |  Panel    |  |               |  | |
|  |  +-----------+  +-----------+  +---------------+  | |
|  +---------------------------------------------------+ |
|           |          JS  <-->  WASM  Bridge             |
|  +---------------------------------------------------+ |
|  |               WASM Engine (C++)                    | |
|  |                                                    | |
|  |  [Simulation Engine] [Script Interpreter]          | |
|  |  [Graph Validator]   [Local Test Runner]           | |
|  +---------------------------------------------------+ |
+---------------------------------------------------------+
```

## UI Components

### Graph Editor
- HTML5 Canvas or SVG-based
- Drag components from a palette onto the canvas
- Draw edges between ports by clicking and dragging
- Select a node to open its code editor
- Visual feedback: running (green), error (red), idle (gray)
- Animated mode: highlight active edges, show messages flowing

### Code Editor
- Embed Monaco Editor (VS Code's editor component) for SysLang
- Syntax highlighting for SysLang keywords
- Each component has its own code tab
- Auto-complete for connected component APIs (e.g., type `db.` and see `query`, `execute`)

### Problem Browser
- List of problems with difficulty tags
- Filter by difficulty, topic, component types
- Show solve rate, user's status (unsolved / attempted / solved)

### Simulation Controls
- **Run** — execute all tests, show pass/fail
- **Step (Event)** — advance one event, animate the message
- **Step (Line)** — advance one line in current handler
- **Reset** — restart simulation
- **Speed slider** — control animation speed in step-through mode

### Test Results Panel
- Per-test pass/fail with expandable details
- Expected vs actual for failed assertions
- Execution timeline

### Console / Logs Panel
- `log()` output from user code
- Event trace: timestamped list of all events
- Per-component logs (filterable)

## JS <--> WASM Bridge

The WASM module exposes a C API that JS calls:

```
// Lifecycle
engine_create()          -> EngineHandle
engine_destroy(handle)
engine_load_graph(handle, graph_json)
engine_load_code(handle, component_id, code_string)

// Execution
engine_run_all(handle)   -> TestResults (JSON)
engine_step_event(handle)-> SimState (JSON)
engine_step_fiber(handle)-> SimState (JSON)
engine_reset(handle)

// Queries
engine_get_state(handle) -> SimState (JSON)
engine_get_logs(handle)  -> Logs (JSON)
engine_validate_graph(handle) -> ValidationResult (JSON)
```

Data crosses the bridge as JSON strings (simple, debuggable). The WASM module owns all simulation state; JS only renders.

## Responsive Layout

```
+---------------------------+----------------------------+
|                           |                            |
|     Graph Editor          |     Code Editor            |
|     (drag & drop)         |     (Monaco / SysLang)     |
|                           |                            |
+---------------------------+----------------------------+
|                           |                            |
|   Problem Description     |   Console / Test Results   |
|   (markdown rendered)     |   (logs, pass/fail)        |
|                           |                            |
+---------------------------+----------------------------+
```

Panels are resizable. Graph editor and code editor are the primary views, with problem description and results below.
