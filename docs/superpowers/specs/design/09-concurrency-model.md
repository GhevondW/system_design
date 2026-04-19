# Concurrency & Load Model

## Client as Load Generator

The Client component in the graph represents **all users**, not a single user. It's a load generator that can simulate:

- **N concurrent users** sending requests simultaneously
- **Request patterns**: burst, steady, ramp-up
- **Different request types**: mixed GET/POST workloads

```
+---------------------------+
|  Client                   |
|---------------------------|
|  concurrent_users: 1000   |
|  requests_per_sec: 500    |
|  pattern: steady          |
|---------------------------|
|  Generates N requests at  |
|  the same virtual time    |
+---------------------------+
```

## How It Works in Simulation

The event queue is time-ordered. Multiple events at the **same timestamp** simulate concurrency:

```
t=0: ClientRequest #1  (user A: POST /register)
t=0: ClientRequest #2  (user B: POST /register)  ← same time = concurrent
t=0: ClientRequest #3  (user C: POST /register)
t=1: DBResponse #1     (for request #1)
t=1: DBResponse #2     (for request #2)
...
```

This exposes real concurrency bugs:
- **Race conditions**: Two users register the same plate at t=0 — does the DB handle it?
- **Resource contention**: All requests hit the DB at once — does caching help?
- **Ordering**: Message queue with 1000 messages — does the worker keep up?

## Test Case Support

Test cases can specify concurrent load:

```yaml
- name: "1000 concurrent registrations"
  load:
    concurrent_users: 1000
    requests:
      - method: POST
        path: /register
        body_template: { plate: "PLATE_{{i}}", owner: "User_{{i}}" }
  expectations:
    - all_status: 201
    - db_check: { table: cars, count: 1000 }
```

## What This Tests

| Scenario | What breaks without proper design |
|----------|-----------------------------------|
| 1000 concurrent writes | DB conflicts, duplicate entries |
| Read-heavy load (90% GET) | Need caching layer |
| Burst traffic | Need rate limiting or queue buffering |
| Mixed read/write | Cache invalidation issues |

## Implementation Notes

- The simulation is deterministic — same inputs always produce same outputs
- Virtual time means 1000 "concurrent" requests process sequentially but at the same timestamp
- The key insight: if your handler checks-then-writes without atomicity, concurrent requests at the same timestamp will expose the race condition
- Advanced problems can use this to require proper locking, CAS operations, or queue-based architectures
