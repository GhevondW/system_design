# Simulation Engine — Hybrid Model

## Architecture

The engine uses a **hybrid event queue + Cortex fiber** model:
- **Event queue** drives simulation time and cross-component communication
- **Cortex fibers** handle per-request concurrency within components

```
+-------------------------------------------------------------+
|                    SIMULATION ENGINE                        |
|                                                             |
|  +------------------+    +-----------------------------+    |
|  |   Event Queue    |    |    Component Runtime        |    |
|  |  (time-ordered)  |    |                             |    |
|  |                  |    |  [HTTPServer fiber pool]    |    |
|  |  t=0: ClientReq  |--->|  [Database fiber pool]      |    |
|  |  t=1: DbResponse |    |  [Queue fiber pool]         |    |
|  |  t=2: CacheHit   |    |  [... per component]        |    |
|  +------------------+    +-----------------------------+    |
|           |                         |                       |
|  +------------------+    +-----------------------------+    |
|  | Simulation Clock |    |   Script Interpreter        |    |
|  | (virtual time)   |    |   (runs inside fibers)      |    |
|  +------------------+    +-----------------------------+    |
+-------------------------------------------------------------+
```

## Execution Flow

1. **Test case** creates initial events (e.g., "Client sends POST /register at t=0")
2. **Event queue** pops the lowest-time event, delivers it to the target component
3. **Component runtime** spawns a Cortex fiber for the incoming request
4. **Script interpreter** runs the user's handler code inside the fiber
5. When handler calls a connected component (e.g., `db.query(...)`):
   - Current fiber **yields**
   - A new event is enqueued for the target component (with simulated latency)
6. Target component processes the event in its own fiber, enqueues a response event
7. Original fiber **resumes** with the result
8. Repeat until all events are drained and all fibers are complete

```
  Client          HTTPServer          Database
    |                  |                 |
    |-- POST /reg ---->|                 |
    |   [t=0]          |                 |
    |                  |--- db.query --->|
    |                  |   [t=1]         |
    |                  |                 |--- executes query
    |                  |<-- result ------|
    |                  |   [t=2]         |
    |                  |--- cache.set -->|  (to Cache)
    |<-- 201 Created --|                 |
    |   [t=3]          |                 |
```

## Event Types

```
NetworkRequest   { from, to, method, path, body, timestamp }
NetworkResponse  { from, to, status, body, timestamp }
QueueMessage     { from, to, topic, payload, timestamp }
QueueAck         { from, to, message_id, timestamp }
CacheOp          { from, to, operation, key, value, timestamp }
TimerFired       { component, callback, timestamp }
Failure          { target, type, timestamp }  // network drop, crash, etc.
```

## Simulated Time

- All events have a **virtual timestamp**
- Cross-component calls add configurable latency (e.g., network: 1-5ms, DB query: 5-50ms)
- The clock only advances when the event queue is popped — no real-time waiting
- This makes the simulation deterministic and fast

## Execution Modes

### Run Mode (Fast)
Drain the event queue at maximum speed. No rendering between events. Collect results, compare against test expectations, report pass/fail.

### Step-Through Mode (Animated)
Two granularity levels:

**Event-level stepping** (coarse):
- `engine.stepEvent()` — pop one event, execute it fully, return updated state
- UI renders the message flow between components
- Good for understanding the "big picture" of data flow

**Fiber-level stepping** (fine):
- `engine.stepFiber()` — advance one fiber by one yield point
- UI highlights the current line of user code
- Good for debugging handler logic

The UI lets users switch between granularities at any time.

## Failure Injection

Problems can define failures as events in the simulation:

```
// Network partition between server and database at t=500
Failure { target: "db_conn", type: "network_drop", timestamp: 500 }

// Server crash at t=1000
Failure { target: "server", type: "crash", timestamp: 1000 }

// Slow response (adds 200ms latency) starting at t=300
Failure { target: "cache", type: "slow", extra_latency: 200, timestamp: 300 }
```

This lets advanced problems test fault tolerance, retry logic, circuit breakers, etc.

## Deterministic Replay

Because all scheduling decisions go through the event queue:
- Record the event processing order
- Replay produces identical execution
- Useful for debugging: "step back" by replaying up to event N-1

## Component Lifecycle

```
  CREATED  --->  INITIALIZED  --->  RUNNING  --->  STOPPED
                     |                  |
                     |    (crash)       |
                     +------<-----------+
```

- **CREATED**: Component exists in the graph but hasn't been set up
- **INITIALIZED**: User code's `init()` function has run (create tables, set routes, etc.)
- **RUNNING**: Actively processing events
- **STOPPED**: Crashed or shut down (can be restarted by certain problem scenarios)
