import test from 'node:test';
import assert from 'node:assert/strict';

import { setupDom, makeCanvas } from './jsdom-setup.mjs';

async function makeEditor() {
    const { GraphEditor } = await import('../../web/js/graph-editor.js');
    return new GraphEditor(makeCanvas());
}

test('GraphEditor: loadGraph → getGraphJson round-trips components and connections', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const editor = await makeEditor();

    const graph = {
        components: [
            { id: 'client', type: 'Client', x: 10, y: 10 },
            { id: 'server', type: 'HttpServer', x: 100, y: 10,
              routes: [{ method: 'POST', path: '/r', handler: 'h' }] },
            { id: 'db', type: 'Database', x: 200, y: 10,
              tables: { cars: [{ name: 'id', type: 'int' }] } },
        ],
        connections: [{ from: 'server', to: 'db', alias: 'db' }],
    };
    editor.loadGraph(graph);

    const json = editor.getGraphJson();
    assert.equal(json.components.length, 3);
    assert.deepEqual(json.connections, [{ from: 'server', to: 'db', alias: 'db' }]);

    const server = json.components.find(c => c.id === 'server');
    assert.equal(server.routes[0].handler, 'h');
    const db = json.components.find(c => c.id === 'db');
    assert.ok(db.tables?.cars, 'tables should survive round-trip');
});

test('GraphEditor: addNode appends a new component', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const editor = await makeEditor();

    const node = editor.addNode('Client', 50, 50);
    assert.equal(node.type, 'Client');
    assert.equal(editor.nodes.length, 1);
    assert.equal(editor.getGraphJson().components[0].id, node.id);
});

test('GraphEditor: removeNode also drops edges that reference it', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const editor = await makeEditor();

    editor.loadGraph({
        components: [
            { id: 'a', type: 'Client' },
            { id: 'b', type: 'HttpServer' },
        ],
        connections: [{ from: 'a', to: 'b', alias: 'b' }],
    });
    assert.equal(editor.edges.length, 1);

    editor.removeNode(editor.nodes.find(n => n.id === 'a'));
    assert.equal(editor.nodes.length, 1);
    assert.equal(editor.edges.length, 0, 'edge referencing removed node should be gone');
});

test('GraphEditor: addEdge rejects self-loops and duplicates', async (t) => {
    const teardown = setupDom();
    t.after(() => teardown());
    const editor = await makeEditor();

    editor.loadGraph({
        components: [
            { id: 'a', type: 'Client' },
            { id: 'b', type: 'HttpServer' },
        ],
        connections: [],
    });

    editor.addEdge('a', 'a');
    assert.equal(editor.edges.length, 0, 'self-loop should be rejected');

    editor.addEdge('a', 'b', 'b');
    editor.addEdge('a', 'b', 'b');
    assert.equal(editor.edges.length, 1, 'duplicate edge should be rejected');
});
