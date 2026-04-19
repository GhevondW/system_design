/**
 * Shared JSDOM harness for the web-UI unit tests.
 *
 * Each test calls setupDom() in a t.before / top-of-test block and the
 * returned teardown in t.after, so globals don't leak between tests.
 */

import { JSDOM } from 'jsdom';

export function setupDom() {
    const dom = new JSDOM('<!DOCTYPE html><html><body></body></html>', {
        url: 'http://localhost/',
        pretendToBeVisual: true,
    });
    globalThis.window = dom.window;
    globalThis.document = dom.window.document;
    globalThis.HTMLElement = dom.window.HTMLElement;
    globalThis.Element = dom.window.Element;
    globalThis.Node = dom.window.Node;
    globalThis.navigator = dom.window.navigator;
    globalThis.devicePixelRatio = 1;
    // fetch isn't bundled with jsdom — force it to fail so wasm-bridge
    // immediately falls back to mock mode instead of network-timing out.
    globalThis.fetch = async () => { throw new Error('fetch disabled in unit tests'); };
    return () => dom.window.close();
}

/**
 * JSDOM's canvas element has no real 2D context. Replace getContext so
 * the GraphEditor can call draw methods during construction without
 * throwing; every method is a no-op since we aren't verifying pixels.
 */
export function stubCanvas(canvas) {
    const noop = () => {};
    const ctx = {
        canvas,
        save: noop, restore: noop, clearRect: noop, translate: noop, scale: noop,
        beginPath: noop, moveTo: noop, lineTo: noop, arcTo: noop, arc: noop,
        bezierCurveTo: noop, stroke: noop, fill: noop, fillText: noop,
        closePath: noop, setLineDash: noop,
        strokeStyle: '', fillStyle: '', lineWidth: 1, font: '', textAlign: '',
        shadowColor: '', shadowBlur: 0, shadowOffsetY: 0,
    };
    canvas.getContext = () => ctx;
    return ctx;
}

/**
 * Build a canvas element with sized parent, attached to document.body.
 * Returns the canvas — GraphEditor's _resize walks parentElement for
 * dimensions so the parent must have clientWidth/clientHeight stubs.
 */
export function makeCanvas({ width = 800, height = 600 } = {}) {
    const parent = document.createElement('div');
    Object.defineProperty(parent, 'clientWidth', { value: width, configurable: true });
    Object.defineProperty(parent, 'clientHeight', { value: height, configurable: true });
    document.body.appendChild(parent);
    const canvas = document.createElement('canvas');
    parent.appendChild(canvas);
    stubCanvas(canvas);
    return canvas;
}
