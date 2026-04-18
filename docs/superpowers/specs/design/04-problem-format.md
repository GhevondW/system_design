# Problem Format & Validation

## Problem Directory Structure

Each problem is a self-contained directory:

```
problems/
  car-registration/
    manifest.yaml           # metadata, difficulty, tags
    description.md          # problem statement shown to user
    initial_graph.json      # starting graph (pre-placed components, if any)
    test_cases/
      basic.yaml            # basic correctness tests
      edge_cases.yaml       # edge case tests
      performance.yaml      # throughput/latency (optional)
    solution/               # reference solution (not shown to user)
      graph.json
      handlers/
        server.syslang      # reference handler code per component
```

## Manifest

```yaml
id: "car-registration"
title: "Car Registration Service"
difficulty: beginner        # beginner | intermediate | advanced
tags: ["crud", "database", "rest-api"]
components_required:        # which component types the user must use
  - HTTPServer
  - Database
components_available:       # which types appear in the palette
  - HTTPServer
  - Database
  - Cache
  - Client
estimated_time: "20min"
description_file: "description.md"
initial_graph: "initial_graph.json"
```

## Problem Description (Markdown)

Shown to the user. Includes:
- What to build (high-level description)
- API contract (endpoints, request/response formats)
- Constraints (ordering, consistency, performance)
- Hints (optional, unlockable)

Example:
```markdown
# Car Registration Service

Build a simple car registration service.

## Requirements

- `POST /register` — Register a new car. Body: `{ plate, owner }`
  - Returns 201 if new, 409 if plate already exists
- `GET /car/{plate}` — Look up a car by plate
  - Returns 200 with car data, or 404 if not found

## Constraints

- Plate numbers are unique
- Data must persist across requests (use a database)

## Bonus

- Add caching for frequently looked-up plates
```

## Initial Graph

Some problems start with a partial graph (e.g., Client already connected to a Load Balancer) that the user must complete. Others start with an empty canvas.

```json
{
  "nodes": [
    { "id": "client", "type": "Client", "position": { "x": 50, "y": 200 }, "locked": true }
  ],
  "edges": []
}
```

`locked: true` means the user cannot remove or modify this component — it's part of the test fixture.

## Test Case Format

```yaml
- name: "Register a new car"
  steps:
    - action: client.send("POST", "/register", { plate: "ABC123", owner: "Alice" })
      expect:
        status: 201
        body.registered: true

    - action: client.send("GET", "/car/ABC123")
      expect:
        status: 200
        body.plate: "ABC123"
        body.owner: "Alice"

  assertions:
    - db.table("cars").count() == 1
    - cache.has("ABC123") == true          # only if cache is connected

- name: "Reject duplicate registration"
  steps:
    - action: client.send("POST", "/register", { plate: "ABC123", owner: "Alice" })
      expect:
        status: 201

    - action: client.send("POST", "/register", { plate: "ABC123", owner: "Bob" })
      expect:
        status: 409

  assertions:
    - db.table("cars").count() == 1        # still only one entry
```

## Validation Flow

```
  User clicks "Submit"
         |
         v
  +------------------+
  | Browser (WASM)   |  <-- optional local pre-check
  | Quick smoke test  |
  +--------+---------+
           |
           v
  +------------------+
  | Backend (native) |  <-- authoritative validation
  | Run all tests    |
  +--------+---------+
           |
    +------+------+
    |             |
  PASS          FAIL
    |             |
  Score +      Show which tests failed,
  Leaderboard  expected vs actual output,
  update       component logs
```

1. User clicks "Submit" — solution (graph JSON + handler code) is sent to backend
2. Backend instantiates the simulation engine with the user's graph
3. Runs each test case: execute steps, check expectations, run assertions
4. Returns per-test pass/fail, overall score, execution time, component logs

## Assertion Types

| Assertion | Description |
|-----------|-------------|
| `response.status == N` | HTTP status code check |
| `response.body.field == value` | Response body field check |
| `db.table(name).count() == N` | Row count in a table |
| `db.table(name).where(cond)` | Query table state |
| `cache.has(key)` | Cache key existence |
| `queue.pending(topic) == N` | Messages pending in queue |
| `component.state == "running"` | Component lifecycle check |

## Scoring

For MVP, scoring is binary per test case: pass or fail. Total score = passed / total.

Future extensions:
- Performance scoring (response time under load)
- Efficiency scoring (fewer components = bonus)
- Code quality hints (not graded, just suggestions)

## Difficulty Tiers — Example Problems

### Beginner
- **Car Registration** — CRUD with HTTP Server + Database
- **URL Shortener** — Generate short codes, redirect
- **Key-Value Store** — Simple cache API

### Intermediate
- **Notification System** — Server + Queue + Worker + Database
- **Task Scheduler** — Delayed job execution with queues
- **API Rate Limiter** — Count requests, enforce limits with cache

### Advanced
- **Distributed Counter** — Multiple servers, eventual consistency
- **Event Sourcing** — Append-only log, materialized views
- **Circuit Breaker** — Handle downstream failures gracefully
- **Chat System** — Multiple clients, message ordering, delivery guarantees
