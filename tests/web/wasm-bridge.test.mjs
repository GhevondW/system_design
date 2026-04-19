import test from 'node:test';
import assert from 'node:assert/strict';

import { setupDom } from './jsdom-setup.mjs';

test('WasmBridge: falls back to mock mode when WASM is unavailable', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const { WasmBridge } = await import('../../web/js/wasm-bridge.js');

    const bridge = new WasmBridge();
    await bridge.init();
    assert.equal(bridge.ready, true, 'ready after init');
    assert.equal(bridge.mock, true, 'should be in mock mode');
    assert.equal(bridge.isWasmLoaded(), false);
});

test('WasmBridge: mock-mode runAll reports every test failed with a clear message', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const { WasmBridge } = await import('../../web/js/wasm-bridge.js');

    const bridge = new WasmBridge();
    await bridge.init();
    const result = bridge.runAll([{ name: 't1' }, { name: 't2' }]);
    assert.equal(result.total, 2);
    assert.equal(result.passed, 0);
    assert.equal(result.failed, 2);
    assert.equal(result.all_passed, false);
    assert.equal(result.results.length, 2);
    for (const r of result.results) {
        assert.equal(r.passed, false);
        assert.match(r.error, /WASM not available/);
    }
});

test('WasmBridge: mock-mode loadGraph/loadCode return ok', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const { WasmBridge } = await import('../../web/js/wasm-bridge.js');

    const bridge = new WasmBridge();
    await bridge.init();
    assert.deepEqual(bridge.loadGraph({ components: [] }), { ok: true });
    assert.deepEqual(bridge.loadCode('comp', 'fn handle() {}'), { ok: true });
});
