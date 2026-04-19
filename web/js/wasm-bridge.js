/**
 * WASM Bridge — wraps the raw C API into a clean async JavaScript API.
 * Manages WASM memory, string conversion, and engine lifecycle.
 */
export class WasmBridge {
    constructor() {
        this.module = null;
        this.handle = null;
        this.ready = false;
    }

    /**
     * Initialize the WASM module
     * @param {string} wasmPath - Path to the systemforge.js loader
     */
    async init(wasmPath = 'systemforge.js') {
        // For development without WASM, use mock mode
        if (typeof window !== 'undefined' && !window.SystemForge) {
            console.log('[WasmBridge] WASM not available, using mock mode');
            this.ready = true;
            this.mock = true;
            return;
        }

        try {
            this.module = await window.SystemForge();
            this.handle = this.module._engine_create();
            this.ready = true;
            console.log('[WasmBridge] Engine created, handle:', this.handle);
        } catch (e) {
            console.error('[WasmBridge] Failed to initialize:', e);
            this.mock = true;
            this.ready = true;
        }
    }

    /**
     * Load a graph definition
     * @param {object} graphJson - Graph definition object
     * @returns {object} Result
     */
    loadGraph(graphJson) {
        if (this.mock) return { ok: true };
        return this._callJson('engine_load_graph', JSON.stringify(graphJson));
    }

    /**
     * Load SysLang code for a component
     * @param {string} componentId
     * @param {string} code
     * @returns {object} Result
     */
    loadCode(componentId, code) {
        if (this.mock) return { ok: true };
        return this._callJsonWithId('engine_load_code', componentId, code);
    }

    /**
     * Run all test cases
     * @param {Array} testCases - Test case definitions
     * @returns {object} Test results
     */
    runAll(testCases) {
        if (this.mock) {
            return {
                total: testCases.length,
                passed: 0,
                failed: testCases.length,
                all_passed: false,
                results: testCases.map(tc => ({
                    name: tc.name || 'test',
                    passed: false,
                    error: 'WASM not available'
                }))
            };
        }
        return this._callJson('engine_run_all', JSON.stringify(testCases));
    }

    /**
     * Step one event forward
     * @returns {object} State snapshot
     */
    stepEvent() {
        if (this.mock) return { finished: true, events_processed: 0 };
        return this._callString('engine_step_event');
    }

    /**
     * Reset the engine to initial state
     */
    reset() {
        if (this.mock) return;
        this.module._engine_reset(this.handle);
    }

    /**
     * Get current engine state
     * @returns {object} State
     */
    getState() {
        if (this.mock) return { current_time: 0, logs: [] };
        return this._callString('engine_get_state');
    }

    /**
     * Get log output
     * @returns {Array<string>} Logs
     */
    getLogs() {
        if (this.mock) return [];
        return this._callString('engine_get_logs');
    }

    /**
     * Destroy the engine instance
     */
    destroy() {
        if (this.module && this.handle) {
            this.module._engine_destroy(this.handle);
            this.handle = null;
        }
    }

    // --- Private helpers ---

    _callJson(funcName, jsonStr) {
        const ptr = this.module.stringToNewUTF8(jsonStr);
        const resultPtr = this.module['_' + funcName](this.handle, ptr);
        this.module._free(ptr);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    _callJsonWithId(funcName, id, str) {
        const idPtr = this.module.stringToNewUTF8(id);
        const strPtr = this.module.stringToNewUTF8(str);
        const resultPtr = this.module['_' + funcName](this.handle, idPtr, strPtr);
        this.module._free(idPtr);
        this.module._free(strPtr);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    _callString(funcName) {
        const resultPtr = this.module['_' + funcName](this.handle);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }
}
