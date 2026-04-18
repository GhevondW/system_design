# Component Model

## Concept

Each component in the graph is a **node** with:
- **Typed ports** — input/output connection points
- **Public API** — methods that connected components can call
- **User code** — handler functions written in the scripting language

Graph **edges** connect an output port of one component to an input port of another. An edge means "this component can call the connected component's API." Only compatible port types can be connected.

## Component Structure

```
+------------------------------------------+
|  Component: HTTPServer                   |
|------------------------------------------|
|  Ports (typed connections):              |
|    IN:  request (HttpRequest)            |
|    OUT: response (HttpResponse)          |
|    OUT: db_conn (DatabaseConnection)     |
|    OUT: cache_conn (CacheConnection)     |
|------------------------------------------|
|  Public API:                             |
|    .addRoute(method, path, handler)      |
|    .listen(port)                         |
|------------------------------------------|
|  User Code:                              |
|    handler functions written in the      |
|    scripting language                    |
+------------------------------------------+
```

## MVP Component Types (8)

### Client
Sends requests, triggers test scenarios.
```
Ports:  OUT -> request (HttpRequest)
API:    send(method, url, body) -> Response
        expect(status, body)
```

### HTTP Server
Handles HTTP requests, routes to user-defined handlers.
```
Ports:  IN  <- request (HttpRequest)
        OUT -> db_conn (DatabaseConnection)
        OUT -> cache_conn (CacheConnection)
        OUT -> queue_conn (QueueConnection)
API:    addRoute(method, path, handler)
        listen(port)
```

### Database
SQL-like relational storage.
```
Ports:  IN <- db_conn (DatabaseConnection)
API:    query(sql, params) -> List<Map>
        execute(sql, params) -> Result
        createTable(name, schema)
```

### Message Queue
Async message passing with configurable delivery guarantees.
```
Ports:  IN  <- queue_conn (QueueConnection)
        OUT -> subscriber (MessageHandler)
API:    publish(topic, message)
        subscribe(topic, handler)
        ack(message)
Config: delivery: at_least_once | at_most_once | exactly_once
        ordering: fifo | unordered
```

### Cache
Key-value store with TTL.
```
Ports:  IN <- cache_conn (CacheConnection)
API:    get(key) -> Value | null
        set(key, value, ttl)
        del(key)
        has(key) -> bool
```

### Load Balancer
Distributes requests across target servers.
```
Ports:  IN  <- request (HttpRequest)
        OUT -> targets (HttpRequest)  [multiple]
API:    addTarget(server)
        removeTarget(server)
Config: strategy: round_robin | random | least_connections
```

### Worker
Background job processor, pulls from queues.
```
Ports:  IN  <- queue_conn (QueueConnection)
        OUT -> db_conn (DatabaseConnection)
API:    process(handler)
        onMessage(handler)
```

### DNS / Service Registry
Name resolution and service discovery.
```
Ports:  IN <- lookup (LookupRequest)
API:    register(name, address)
        resolve(name) -> address
        deregister(name)
```

## Example Graph: Car Registration

```
                         +--------+
                         | Client |
                         +---+----+
                             |
                        request (POST /register)
                             |
                             v
                      +-----------+
                      | HTTPServer|
                      +--+-----+--+
                         |     |
               db_conn   |     |  cache_conn
                         v     v
                   +------+  +-------+
                   |  DB  |  | Cache |
                   +------+  +-------+
```

## Port Type Compatibility

Edges enforce type safety:

| Output Port Type      | Compatible Input Port Types |
|-----------------------|-----------------------------|
| HttpRequest           | HttpRequest                 |
| DatabaseConnection    | DatabaseConnection          |
| CacheConnection       | CacheConnection             |
| QueueConnection       | QueueConnection             |
| MessageHandler        | MessageHandler              |
| LookupRequest         | LookupRequest               |

A component can have **multiple outgoing edges** of the same type (e.g., an HTTP Server connected to both a Database and a Cache). Each connection injects a named variable into the component's scripting scope.

## Connection Variable Injection

When a component is connected to another via an edge, the target's API becomes available as a variable in the source's scripting scope:

```
HTTPServer --db_conn--> Database      =>  `db` variable available
HTTPServer --cache_conn--> Cache      =>  `cache` variable available
HTTPServer --queue_conn--> Queue      =>  `queue` variable available
```

If multiple connections of the same type exist (e.g., two databases), the user names them in the edge properties:
```
HTTPServer --db_conn("users_db")--> UsersDB    =>  `users_db` variable
HTTPServer --db_conn("orders_db")--> OrdersDB  =>  `orders_db` variable
```
