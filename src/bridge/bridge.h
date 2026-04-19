#pragma once

// C API for the WASM bridge
// All data crosses as JSON strings (allocated on the C++ side)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

// Engine lifecycle
EMSCRIPTEN_KEEPALIVE int engine_create();
EMSCRIPTEN_KEEPALIVE void engine_destroy(int handle);

// Graph and code loading
EMSCRIPTEN_KEEPALIVE const char* engine_load_graph(int handle, const char* graph_json);
EMSCRIPTEN_KEEPALIVE const char* engine_load_code(int handle, const char* component_id,
                                                   const char* code);

// Execution
EMSCRIPTEN_KEEPALIVE const char* engine_run_all(int handle, const char* test_cases_json);
EMSCRIPTEN_KEEPALIVE const char* engine_step_event(int handle);
EMSCRIPTEN_KEEPALIVE void engine_reset(int handle);

// State queries
EMSCRIPTEN_KEEPALIVE const char* engine_get_state(int handle);
EMSCRIPTEN_KEEPALIVE const char* engine_get_logs(int handle);

// Memory management
EMSCRIPTEN_KEEPALIVE void engine_free_string(const char* ptr);

// Version
EMSCRIPTEN_KEEPALIVE int engine_version();

#ifdef __cplusplus
}
#endif
