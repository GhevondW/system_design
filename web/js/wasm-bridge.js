/**
 * WASM Bridge — wraps the C API into a clean JavaScript API.
 */
export class WasmBridge {
    constructor() {
        this.module = null;
        this.handle = null;
        this.ready = false;
        this.mock = false;
    }

    async init() {
        try {
            // Try loading the WASM module
            const response = await fetch('systemforge.wasm');
            if (!response.ok) throw new Error('WASM file not found');

            // Load the Emscripten JS glue
            const script = document.createElement('script');
            script.src = 'systemforge.js';
            await new Promise((resolve, reject) => {
                script.onload = resolve;
                script.onerror = reject;
                document.head.appendChild(script);
            });

            this.module = await SystemForge();
            this.handle = this.module._engine_create();
            this.ready = true;
            console.log('[WasmBridge] Engine created, handle:', this.handle);
        } catch (e) {
            console.warn('[WasmBridge] WASM not available, using mock mode:', e.message);
            this.mock = true;
            this.ready = true;
        }
    }

    loadGraph(graphJson) {
        if (this.mock) return { ok: true };
        const str = JSON.stringify(graphJson);
        const ptr = this.module.stringToNewUTF8(str);
        const resultPtr = this.module._engine_load_graph(this.handle, ptr);
        this.module._free(ptr);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    loadCode(componentId, code) {
        if (this.mock) return { ok: true };
        const idPtr = this.module.stringToNewUTF8(componentId);
        const codePtr = this.module.stringToNewUTF8(code);
        const resultPtr = this.module._engine_load_code(this.handle, idPtr, codePtr);
        this.module._free(idPtr);
        this.module._free(codePtr);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    runAll(testCases) {
        if (this.mock) {
            return {
                total: testCases.length, passed: 0, failed: testCases.length,
                all_passed: false,
                results: testCases.map(tc => ({
                    name: tc.name || 'test', passed: false,
                    error: 'WASM not available — run ./dev.sh serve'
                }))
            };
        }
        const str = JSON.stringify(testCases);
        const ptr = this.module.stringToNewUTF8(str);
        const resultPtr = this.module._engine_run_all(this.handle, ptr);
        this.module._free(ptr);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    stepEvent() {
        if (this.mock) return { finished: true, events_processed: 0 };
        const resultPtr = this.module._engine_step_event(this.handle);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    reset() {
        if (this.mock) return;
        this.module._engine_reset(this.handle);
    }

    getState() {
        if (this.mock) return { current_time: 0, logs: [] };
        const resultPtr = this.module._engine_get_state(this.handle);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    getLogs() {
        if (this.mock) return [];
        const resultPtr = this.module._engine_get_logs(this.handle);
        const result = this.module.UTF8ToString(resultPtr);
        this.module._engine_free_string(resultPtr);
        return JSON.parse(result);
    }

    destroy() {
        if (this.module && this.handle) {
            this.module._engine_destroy(this.handle);
            this.handle = null;
        }
    }

    isWasmLoaded() {
        return !this.mock;
    }
}
