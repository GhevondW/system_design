# Backend Architecture

## Overview

C++ REST API server handling auth, storage, validation, and leaderboards.

```
+-------------------------------------------------------+
|                    C++ Backend                         |
|                                                        |
|  +------------+  +------------+  +-----------------+  |
|  |  REST API  |  |    Auth    |  |  Simulation     |  |
|  |  Router    |  |  (JWT)     |  |  Runner         |  |
|  +-----+------+  +-----+------+  |  (native C++)   |  |
|        |               |         +---------+--------+  |
|        v               v                   |           |
|  +------------+  +------------+            |           |
|  |  Problem   |  |  Solution  |            |           |
|  |  Service   |  |  Service   |<-----------+           |
|  +-----+------+  +-----+------+                       |
|        |               |                               |
|        v               v                               |
|  +--------------------------------------------+       |
|  |            PostgreSQL Database              |       |
|  +--------------------------------------------+       |
+-------------------------------------------------------+
```

## API Endpoints

### Auth
```
POST   /api/auth/register     { username, email, password }
POST   /api/auth/login        { email, password } -> { token }
GET    /api/auth/me           -> { user profile }
```

### Problems
```
GET    /api/problems                    -> list (with filters)
GET    /api/problems/:id                -> problem detail + manifest
GET    /api/problems/:id/description    -> markdown content
GET    /api/problems/:id/initial-graph  -> starting graph JSON
GET    /api/problems/:id/tests          -> test case definitions (for local run)
```

### Solutions
```
POST   /api/solutions/submit           { problem_id, graph, code } -> validation result
GET    /api/solutions/:problem_id      -> user's solutions for this problem
GET    /api/solutions/:id/detail       -> full solution with test results
```

### Leaderboards
```
GET    /api/leaderboard/global         -> top users overall
GET    /api/leaderboard/:problem_id    -> fastest solvers for a problem
GET    /api/stats/me                   -> user's personal stats
```

### Community (Phase 3)
```
POST   /api/problems/submit            { manifest, description, tests, solution }
GET    /api/problems/community          -> community-submitted problems
POST   /api/problems/:id/review         { approved, feedback }
```

## Database Schema

### Users
```sql
CREATE TABLE users (
    id          UUID PRIMARY KEY,
    username    VARCHAR(50) UNIQUE NOT NULL,
    email       VARCHAR(255) UNIQUE NOT NULL,
    password    VARCHAR(255) NOT NULL,  -- bcrypt hash
    created_at  TIMESTAMP DEFAULT NOW()
);
```

### Problems
```sql
CREATE TABLE problems (
    id          VARCHAR(100) PRIMARY KEY,  -- slug: "car-registration"
    title       VARCHAR(200) NOT NULL,
    difficulty  VARCHAR(20) NOT NULL,      -- beginner, intermediate, advanced
    tags        TEXT[],
    manifest    JSONB NOT NULL,
    description TEXT NOT NULL,
    initial_graph JSONB,
    is_official BOOLEAN DEFAULT true,
    author_id   UUID REFERENCES users(id),
    created_at  TIMESTAMP DEFAULT NOW()
);
```

### Test Cases
```sql
CREATE TABLE test_cases (
    id          UUID PRIMARY KEY,
    problem_id  VARCHAR(100) REFERENCES problems(id),
    name        VARCHAR(200) NOT NULL,
    category    VARCHAR(50),  -- basic, edge_case, performance
    definition  JSONB NOT NULL,
    sort_order  INT DEFAULT 0
);
```

### Solutions
```sql
CREATE TABLE solutions (
    id          UUID PRIMARY KEY,
    user_id     UUID REFERENCES users(id),
    problem_id  VARCHAR(100) REFERENCES problems(id),
    graph       JSONB NOT NULL,
    code        JSONB NOT NULL,   -- { component_id: code_string, ... }
    status      VARCHAR(20),      -- pending, passed, failed
    score       INT,              -- passed_tests / total_tests
    exec_time   INT,              -- milliseconds
    created_at  TIMESTAMP DEFAULT NOW()
);
```

### Solution Test Results
```sql
CREATE TABLE solution_results (
    id           UUID PRIMARY KEY,
    solution_id  UUID REFERENCES solutions(id),
    test_case_id UUID REFERENCES test_cases(id),
    passed       BOOLEAN NOT NULL,
    expected     JSONB,
    actual       JSONB,
    logs         TEXT,
    exec_time    INT
);
```

### Leaderboard (materialized view)
```sql
CREATE MATERIALIZED VIEW leaderboard AS
SELECT
    u.id AS user_id,
    u.username,
    COUNT(DISTINCT s.problem_id) AS problems_solved,
    SUM(s.score) AS total_score
FROM users u
JOIN solutions s ON s.user_id = u.id
WHERE s.status = 'passed'
GROUP BY u.id, u.username
ORDER BY problems_solved DESC, total_score DESC;
```

## Server-Side Validation Flow

```
  POST /api/solutions/submit
         |
         v
  +------------------+
  | Parse graph JSON  |
  | Parse handler code|
  +--------+---------+
           |
           v
  +------------------+
  | Load problem's   |
  | test cases       |
  +--------+---------+
           |
           v
  +------------------+
  | Instantiate       |
  | Simulation Engine |  <-- same C++ engine as WASM, compiled natively
  | Load user's graph |
  +--------+---------+
           |
           v
  +------------------+
  | Run each test    |     timeout: 10s per test, 60s total
  | Collect results  |
  +--------+---------+
           |
           v
  +------------------+
  | Store results    |
  | Update score     |
  | Return response  |
  +------------------+
```

## Technology Choices

- **HTTP framework**: Crow or Drogon (C++ HTTP frameworks with async support)
- **Database driver**: libpq (PostgreSQL C client) or pqxx (C++ wrapper)
- **Auth**: JWT tokens with bcrypt password hashing
- **JSON**: nlohmann/json or simdjson
- **Build**: CMake
