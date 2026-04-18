# Scripting Language — SysLang

A small, expressive, dynamically-typed language with modern C++ flavor. Users write handler logic inside components.

## Design Principles

- **Familiar to C++ developers** — braces, semicolons, lambdas, range-for
- **No complexity** — no types, templates, pointers, memory management, headers
- **Functional-leaning** — functions and maps as primary composition tools
- **Connected components as implicit scope** — graph edges inject API variables

## Syntax Reference

### Variables

```cpp
let name = "car_reg";
let count = 0;
let active = true;
let plates = ["ABC123", "XYZ789"];                      // list (square brackets)
let car = { plate: "ABC123", owner: "Alice", year: 2024 }; // map (curly braces + keys)
```

No explicit types. `let` declares, type is inferred from value.

### Functions

```cpp
fn validate_plate(plate) {
    if (plate.length() != 6) {
        return { ok: false, error: "invalid length" };
    }
    return { ok: true };
}
```

No return type declaration. Functions are first-class values.

### Lambdas

```cpp
let is_valid = [](plate) { return plate.length() == 6; };
let add = [](a, b) { return a + b; };

// with capture (simplified — captures by value)
let threshold = 100;
let check = [threshold](val) { return val > threshold; };
```

### Control Flow

```cpp
// if / else
if (count > 10) {
    log("too many");
} else if (count == 0) {
    log("none");
} else {
    log("ok");
}

// range-based for
for (let plate : plates) {
    log(plate);
}

// while
while (queue.size() > 0) {
    let msg = queue.pop();
    process(msg);
}
```

### Pattern Matching

```cpp
fn handle_request(req) {
    return match (req.method) {
        "GET"  => get_car(req.params.plate),
        "POST" => register_car(req.body),
        _      => { status: 405, body: "Method not allowed" }
    };
}
```

### Data Structures

```cpp
// Lists  (square brackets)
let items = [1, 2, 3];
items.push(4);
let first = items[0];
let count = items.size();

// Maps
let user = { name: "Alice", age: 30 };
user.email = "alice@example.com";   // add field
let name = user.name;               // access field
let has_email = user.has("email");  // check field

// Nested
let response = {
    status: 200,
    body: {
        users: [
            { name: "Alice", role: "admin" },
            { name: "Bob",   role: "user" }
        ]
    }
};
```

### String Operations

```cpp
let s = "hello world";
s.length();              // 11
s.contains("world");     // true
s.split(" ");            // ["hello", "world"]
s.upper();               // "HELLO WORLD"
s.substr(0, 5);          // "hello"
s + " !!!";              // "hello world !!!"
```

### Built-in Functions

```cpp
log(value);              // print to simulation console
print(value);            // alias for log
len(collection);         // size of list/map/string
str(value);              // convert to string
int(value);              // convert to integer
float(value);            // convert to float
```

## Connected Component API Access

Variables are injected based on graph edges. If an HTTPServer is connected to a Database via a `db_conn` edge:

```cpp
// Inside HTTPServer handler — `db` is automatically available
fn handle_register(req) {
    let existing = db.query(
        "SELECT * FROM cars WHERE plate = ?",
        [req.body.plate]
    );
    if (existing.size() > 0) {
        return { status: 409, body: "Already registered" };
    }
    db.execute(
        "INSERT INTO cars (plate, owner) VALUES (?, ?)",
        [req.body.plate, req.body.owner]
    );
    cache.set(req.body.plate, req.body, 3600);
    return { status: 201, body: { plate: req.body.plate, registered: true } };
}
```

## Error Handling

Errors propagate as values, not exceptions:

```cpp
fn safe_query(plate) {
    let result = db.query("SELECT * FROM cars WHERE plate = ?", [plate]);
    if (result.error) {
        return { status: 500, body: result.error };
    }
    return { status: 200, body: result.data };
}
```

## What SysLang Does NOT Have

- No classes, structs, or OOP
- No explicit types or type declarations
- No templates or generics
- No pointers, references, or memory management
- No `#include`, headers, or modules
- No exceptions (errors are values)
- No `std::` namespace
- No operator overloading
