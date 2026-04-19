/**
 * SystemForge — Main application entry point.
 */
import { GraphEditor } from './graph-editor.js';
import { CodeEditor } from './code-editor.js';
import { ProblemLoader } from './problem-loader.js';
import { WasmBridge } from './wasm-bridge.js';

class App {
    constructor() {
        this.graphEditor = null;
        this.codeEditor = null;
        this.problemLoader = new ProblemLoader();
        this.bridge = new WasmBridge();
        this.currentProblem = null;
    }

    async init() {
        // Init graph editor
        const canvas = document.getElementById('graph-canvas');
        this.graphEditor = new GraphEditor(canvas);
        this.graphEditor.onNodeSelect = (node) => this._onNodeSelect(node);
        this.graphEditor.onNodeDoubleClick = (node) => this._onNodeDoubleClick(node);
        this.graphEditor.onEditCode = (node) => this._onNodeSelect(node);

        // Init code editor
        this.codeEditor = new CodeEditor(document.getElementById('editor-container'));
        await this.codeEditor.init();

        // Init WASM bridge
        await this.bridge.init();

        this._setupProblemSelector();
        this._setupControls();
        this._setupPalette();
        this._setupResizers();

        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
                e.preventDefault();
                this._runTests();
            }
        });

        this._log('info', 'SystemForge ready');
    }

    _setupProblemSelector() {
        const select = document.getElementById('problem-select');
        for (const p of this.problemLoader.getProblems()) {
            const option = document.createElement('option');
            option.value = p.id;
            option.textContent = `[${p.difficulty}] ${p.name}`;
            select.appendChild(option);
        }
        select.addEventListener('change', () => {
            if (select.value) this._loadProblem(select.value);
        });
    }

    _setupControls() {
        document.getElementById('btn-run').addEventListener('click', () => this._runTests());
        document.getElementById('btn-step').addEventListener('click', () => this._stepEvent());
        document.getElementById('btn-reset').addEventListener('click', () => this._reset());
        document.getElementById('btn-clear-console').addEventListener('click', () => {
            document.getElementById('console-output').innerHTML = '';
        });
    }

    _setupPalette() {
        document.querySelectorAll('.palette-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                this.graphEditor.addNode(btn.dataset.type);
            });
        });
    }

    _setupResizers() {
        // Left vertical splitter (description | graph)
        this._makeResizableV('resize-left', 'description-panel', null);
        // Right vertical splitter (graph | code)
        this._makeResizableV('resize-right', null, 'code-panel');
        // Horizontal splitter (top | bottom)
        this._makeResizableH('resize-bottom', 'bottom-panel');
    }

    _makeResizableV(handleId, leftPanelId, rightPanelId) {
        const handle = document.getElementById(handleId);
        if (!handle) return;

        let startX, startWidth;
        const panel = leftPanelId
            ? document.getElementById(leftPanelId)
            : document.getElementById(rightPanelId);

        handle.addEventListener('mousedown', (e) => {
            e.preventDefault();
            startX = e.clientX;
            startWidth = panel.offsetWidth;
            handle.classList.add('active');

            const onMove = (e) => {
                const dx = e.clientX - startX;
                const newWidth = leftPanelId
                    ? Math.max(150, startWidth + dx)
                    : Math.max(150, startWidth - dx);
                panel.style.width = newWidth + 'px';
                this.graphEditor._resize();
            };

            const onUp = () => {
                handle.classList.remove('active');
                document.removeEventListener('mousemove', onMove);
                document.removeEventListener('mouseup', onUp);
            };

            document.addEventListener('mousemove', onMove);
            document.addEventListener('mouseup', onUp);
        });
    }

    _makeResizableH(handleId, bottomPanelId) {
        const handle = document.getElementById(handleId);
        const panel = document.getElementById(bottomPanelId);
        if (!handle || !panel) return;

        let startY, startHeight;

        handle.addEventListener('mousedown', (e) => {
            e.preventDefault();
            startY = e.clientY;
            startHeight = panel.offsetHeight;
            handle.classList.add('active');

            const onMove = (e) => {
                const dy = startY - e.clientY;
                panel.style.height = Math.max(60, startHeight + dy) + 'px';
                this.graphEditor._resize();
            };

            const onUp = () => {
                handle.classList.remove('active');
                document.removeEventListener('mousemove', onMove);
                document.removeEventListener('mouseup', onUp);
            };

            document.addEventListener('mousemove', onMove);
            document.addEventListener('mouseup', onUp);
        });
    }

    _loadProblem(problemId) {
        const problem = this.problemLoader.getProblem(problemId);
        if (!problem) return;
        this.currentProblem = problem;

        this.graphEditor.loadGraph(problem.graph);
        if (problem.starterCode) {
            for (const [nodeId, code] of Object.entries(problem.starterCode)) {
                this.codeEditor.setCode(nodeId, code);
            }
        }
        this._renderDescription(problem.description);
        this._clearTestResults();
        this._setupCodeTabs(problem);

        const serverNode = this.graphEditor.nodes.find(n => n.type === 'HttpServer');
        if (serverNode) {
            this.graphEditor.selectNode(serverNode);
            this.codeEditor.setActiveNode(serverNode.id, problem.starterCode?.[serverNode.id]);
        }

        this._log('info', `Loaded: ${problem.name}`);
    }

    _setupCodeTabs(problem) {
        const tabs = document.getElementById('code-tabs');
        tabs.innerHTML = '';
        const codeNodes = this.graphEditor.nodes.filter(n =>
            n.type === 'HttpServer' || n.type === 'Worker');
        codeNodes.forEach(node => {
            const tab = document.createElement('button');
            tab.className = 'code-tab';
            tab.textContent = node.id;
            tab.dataset.nodeId = node.id;
            tab.addEventListener('click', () => {
                tabs.querySelectorAll('.code-tab').forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                this.codeEditor.setActiveNode(node.id, problem.starterCode?.[node.id]);
                document.getElementById('code-title').textContent = `Code — ${node.id}`;
            });
            tabs.appendChild(tab);
        });
        const firstTab = tabs.querySelector('.code-tab');
        if (firstTab) firstTab.classList.add('active');
    }

    async _runTests() {
        if (!this.currentProblem) {
            this._log('error', 'No problem loaded');
            return;
        }
        this._log('info', 'Running tests...');

        try {
            // Reset the WASM engine and load the current graph + code
            this.bridge.destroy();
            await this.bridge.init();

            if (!this.bridge.isWasmLoaded()) {
                this._log('error', 'WASM not loaded — run: ./dev.sh serve');
                this._renderTestResults(this.bridge.runAll(this.currentProblem.testCases));
                return;
            }

            // 1. Load the graph from the editor
            const graphJson = this.graphEditor.getGraphJson();
            const loadResult = this.bridge.loadGraph(graphJson);
            if (loadResult.error) {
                this._log('error', 'Graph load failed: ' + loadResult.error);
                return;
            }

            // 2. Load user code for each codable component
            for (const node of this.graphEditor.nodes) {
                if (node.type === 'HttpServer' || node.type === 'Worker') {
                    const code = this.codeEditor.getCode(node.id);
                    if (code) {
                        const codeResult = this.bridge.loadCode(node.id, code);
                        if (codeResult.error) {
                            this._log('error', `Code error in ${node.id}: ${codeResult.error}`);
                            this._renderTestResults({
                                total: this.currentProblem.testCases.length,
                                passed: 0, failed: this.currentProblem.testCases.length,
                                all_passed: false,
                                results: this.currentProblem.testCases.map(tc => ({
                                    name: tc.name, passed: false,
                                    error: `Compile error: ${codeResult.error}`
                                }))
                            });
                            return;
                        }
                    }
                }
            }

            // 3. Run test cases through the engine
            const results = this.bridge.runAll(this.currentProblem.testCases);
            this._renderTestResults(results);

            // 4. Show logs
            const logs = this.bridge.getLogs();
            for (const log of logs) {
                this._log('info', log);
            }
        } catch (e) {
            this._log('error', 'Test execution failed: ' + e.message);
        }
    }

    _stepEvent() {
        this._log('info', 'Step (requires WASM build)');
    }

    _reset() {
        if (this.currentProblem) this._loadProblem(this.currentProblem.id);
        this._log('info', 'Reset');
    }

    _onNodeSelect(node) {
        if (node && (node.type === 'HttpServer' || node.type === 'Worker')) {
            document.getElementById('code-title').textContent = `Code — ${node.id}`;
            this.codeEditor.setActiveNode(node.id);
            document.querySelectorAll('.code-tab').forEach(t => {
                t.classList.toggle('active', t.dataset.nodeId === node.id);
            });
        }
    }

    _onNodeDoubleClick(node) {
        if (node) this._onNodeSelect(node);
    }

    _renderDescription(md) {
        const el = document.getElementById('problem-description');
        let html = md
            .replace(/^### (.*$)/gm, '<h3>$1</h3>')
            .replace(/^## (.*$)/gm, '<h2>$1</h2>')
            .replace(/^# (.*$)/gm, '<h1>$1</h1>')
            .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
            .replace(/`([^`]+)`/g, '<code>$1</code>')
            .replace(/^- (.*$)/gm, '<li>$1</li>')
            .replace(/\n\n/g, '</p><p>')
            .replace(/\n/g, '<br>');
        el.innerHTML = '<p>' + html + '</p>';
    }

    _renderTestResults(results) {
        const el = document.getElementById('test-results');
        let html = '';
        const cls = results.all_passed ? 'all-pass' : 'has-fail';
        const icon = results.all_passed ? 'fa-check-circle' : 'fa-times-circle';
        const color = results.all_passed ? 'var(--accent-green)' : 'var(--accent-red)';
        html += `<div class="test-summary ${cls}">
            <i class="fas ${icon}" style="color:${color}"></i>
            <span>${results.passed}/${results.total} passed</span></div>`;
        for (const r of results.results) {
            const c = r.passed ? 'pass' : 'fail';
            const i = r.passed ? 'fa-check' : 'fa-times';
            html += `<div class="test-item ${c}"><i class="fas ${i} test-icon"></i><span class="test-name">${r.name}</span></div>`;
            if (r.error) {
                html += `<div class="test-item fail" style="padding-left:28px"><span class="test-error">${r.error}</span></div>`;
            }
        }
        el.innerHTML = html;
        this._log(results.all_passed ? 'info' : 'error', `Tests: ${results.passed}/${results.total} passed`);
    }

    _clearTestResults() {
        document.getElementById('test-results').innerHTML = `
            <div class="placeholder-text"><i class="fas fa-vial"></i><p>Run tests to see results</p></div>`;
    }

    _log(level, msg) {
        const el = document.getElementById('console-output');
        const t = new Date().toLocaleTimeString();
        const cls = level === 'error' ? 'log-error' : 'log-info';
        el.innerHTML += `<div class="log-entry"><span class="log-time">${t}</span><span class="${cls}">${msg}</span></div>`;
        el.scrollTop = el.scrollHeight;
    }
}

const app = new App();
app.init().catch(err => console.error('Init failed:', err));
