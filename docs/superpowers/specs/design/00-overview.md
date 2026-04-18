# SystemForge — LeetCode for System Design

## What Is This?

An interactive platform where engineers learn system design by **building real systems**, not drawing boxes on a whiteboard. Users solve problems by wiring together components (servers, databases, queues, caches) in a visual graph editor and writing handler logic in a simple scripting language. The system simulates execution and validates correctness via test cases.

## Why?

System design interviews test distributed systems thinking, but there's no hands-on practice tool. LeetCode works for algorithms because you write real code. SystemForge does the same for system design — you wire real-ish components and write real logic.

## Target Audience

Engineers at all levels, with tiered difficulty:
- **Beginner**: Single server + DB (URL shortener, car registration)
- **Intermediate**: Multi-service with caching and queues (notification system, task scheduler)
- **Advanced**: Distributed systems with failure handling (rate limiter, event sourcing, distributed lock)

## Tech Stack

- **Frontend**: Thin JavaScript (graph editor canvas, UI chrome)
- **WASM Engine**: C++ compiled to WebAssembly via Cortex (simulation engine, scripting interpreter, local test runner)
- **Backend**: C++ REST API (auth, storage, validation, leaderboards)
- **Database**: PostgreSQL

The simulation engine is the **same C++ codebase** compiled two ways: WASM for the browser (local debugging) and native for the server (grading/validation).

## High-Level Architecture

```
+-----------------------------------------------------+
|                    WEB CLIENT                        |
|  [Graph Editor] [Code Editor] [Problem Browser]     |
|         thin JS  <---->  WASM Engine                 |
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

## Document Structure

- [01-component-model.md](01-component-model.md) — Graph nodes, ports, APIs
- [02-scripting-language.md](02-scripting-language.md) — The in-platform language
- [03-simulation-engine.md](03-simulation-engine.md) — Hybrid event queue + fiber engine
- [04-problem-format.md](04-problem-format.md) — Problem definitions and validation
- [05-web-client.md](05-web-client.md) — Frontend architecture
- [06-backend.md](06-backend.md) — Server architecture
- [07-phased-mvp.md](07-phased-mvp.md) — Delivery phases
- [08-code-standards.md](08-code-standards.md) — Code style, patterns, and quality standards
