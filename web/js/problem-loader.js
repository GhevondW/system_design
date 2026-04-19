/**
 * ProblemLoader — loads bundled problem definitions.
 */

const PROBLEMS = [
    {
        id: 'car-registration',
        name: 'Car Registration',
        difficulty: 'Beginner',
        description: `# Car Registration System

Design a simple car registration service.

## Requirements

- **POST /register** — Register a new car with \`plate\` and \`owner\`
  - If the plate already exists, return **409** (conflict)
  - Otherwise, insert into the database and return **201**

## Your Task

1. **Design the database schema** — double-click the Database node to define tables and columns
2. **Write the handler** — implement the registration logic in the server

## Database API

- \`db.FindOne("table", { column: value })\` — find first matching row or null
- \`db.Find("table", { column: value })\` — find all matching rows (list)
- \`db.Insert("table", { col1: val1, col2: val2 })\` — insert a row
- \`db.Delete("table", { filter })\` — delete matching rows
- \`db.All("table")\` — get all rows

## Example

\`\`\`
// Check if plate exists
let existing = db.FindOne("cars", { plate: req.body.plate });

// Insert new record
db.Insert("cars", { plate: "ABC123", owner: "Alice" });
\`\`\`

## Hints

- You need a table to store cars (think about what columns you need)
- Use \`db.FindOne()\` to check for duplicates before inserting
- Return \`{ status: 201, body: { registered: true } }\` on success
`,
        graph: {
            components: [
                { id: 'client', type: 'Client', x: 50, y: 150 },
                { id: 'server', type: 'HttpServer', x: 280, y: 150 },
                { id: 'db', type: 'Database', x: 520, y: 150 },
            ],
            connections: [
                { from: 'server', to: 'db', alias: 'db' },
            ],
        },
        starterCode: {
            server: `fn handle_register(req) {
    // TODO: Implement car registration
    // 1. Check if plate exists: db.FindOne("cars", { plate: req.body.plate })
    // 2. If exists (not null), return { status: 409, body: "Already registered" }
    // 3. Otherwise, insert and return 201
    return { status: 500, body: "Not implemented" };
}`,
        },
        routes: [
            { componentId: 'server', method: 'POST', path: '/register', handler: 'handle_register' },
        ],
        testCases: [
            {
                name: 'Register a new car',
                steps: [
                    { client: 'client', target: 'server', method: 'POST', path: '/register',
                      body: { plate: 'ABC123', owner: 'Alice' }, timestamp: 0 },
                ],
                expectations: [{ status: 201, body: { registered: true } }],
                db_checks: { cars: 1 },
            },
            {
                name: 'Duplicate registration returns 409',
                steps: [
                    { client: 'client', target: 'server', method: 'POST', path: '/register',
                      body: { plate: 'XYZ789', owner: 'Bob' }, timestamp: 0 },
                    { client: 'client', target: 'server', method: 'POST', path: '/register',
                      body: { plate: 'XYZ789', owner: 'Charlie' }, timestamp: 10 },
                ],
                expectations: [{ status: 201 }, { status: 409 }],
            },
            {
                name: 'Multiple cars can be registered',
                steps: [
                    { client: 'client', target: 'server', method: 'POST', path: '/register',
                      body: { plate: 'AAA111', owner: 'Dave' }, timestamp: 0 },
                    { client: 'client', target: 'server', method: 'POST', path: '/register',
                      body: { plate: 'BBB222', owner: 'Eve' }, timestamp: 10 },
                ],
                expectations: [{ status: 201 }, { status: 201 }],
                db_checks: { cars: 2 },
            },
        ],
    },
    {
        id: 'key-value-store',
        name: 'Key-Value Store',
        difficulty: 'Beginner',
        description: `# Key-Value Store

Build a simple in-memory key-value store API.

## Requirements

- **POST /set** — Set a key-value pair (body: \`{ key, value }\`)
  - Always returns **200** with \`{ ok: true }\`
- **POST /get** — Get a value by key (body: \`{ key }\`)
  - If found, return **200** with \`{ value: ... }\`
  - If not found, return **404**
- **POST /del** — Delete a key (body: \`{ key }\`)
  - Returns **200** with \`{ deleted: true/false }\`

## Components

- **Client** — sends test requests
- **HTTP Server** — handles routes
- **Cache** — stores key-value pairs

## Cache API

- \`cache.get(key)\` — returns value or null
- \`cache.set(key, value)\` — store a value
- \`cache.del(key)\` — delete a key, returns true/false
- \`cache.has(key)\` — returns true/false
`,
        graph: {
            components: [
                { id: 'client', type: 'Client', x: 50, y: 150 },
                { id: 'server', type: 'HttpServer', x: 280, y: 150 },
                { id: 'cache', type: 'Cache', x: 520, y: 150 },
            ],
            connections: [
                { from: 'server', to: 'cache', alias: 'cache' },
            ],
        },
        starterCode: {
            server: `fn handle_set(req) {
    // TODO: cache.set(key, value)
    return { status: 500, body: "Not implemented" };
}

fn handle_get(req) {
    // TODO: cache.get(key)
    return { status: 500, body: "Not implemented" };
}

fn handle_del(req) {
    // TODO: cache.del(key)
    return { status: 500, body: "Not implemented" };
}`,
        },
        routes: [
            { componentId: 'server', method: 'POST', path: '/set', handler: 'handle_set' },
            { componentId: 'server', method: 'POST', path: '/get', handler: 'handle_get' },
            { componentId: 'server', method: 'POST', path: '/del', handler: 'handle_del' },
        ],
        testCases: [
            {
                name: 'Set and get a value',
                steps: [
                    { client: 'client', target: 'server', method: 'POST', path: '/set',
                      body: { key: 'name', value: 'Alice' }, timestamp: 0 },
                    { client: 'client', target: 'server', method: 'POST', path: '/get',
                      body: { key: 'name' }, timestamp: 10 },
                ],
                expectations: [{ status: 200 }, { status: 200, body: { value: 'Alice' } }],
            },
            {
                name: 'Get missing key returns 404',
                steps: [
                    { client: 'client', target: 'server', method: 'POST', path: '/get',
                      body: { key: 'nonexistent' }, timestamp: 0 },
                ],
                expectations: [{ status: 404 }],
            },
        ],
    },
];

export class ProblemLoader {
    getProblems() {
        return PROBLEMS.map(p => ({ id: p.id, name: p.name, difficulty: p.difficulty }));
    }

    getProblem(id) {
        return PROBLEMS.find(p => p.id === id) || null;
    }
}
