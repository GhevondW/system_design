/**
 * SystemForge — Main application entry point.
 * Wires together all UI modules.
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
        canvas.setAttribute('tabindex', '0');
        this.graphEditor = new GraphEditor(canvas);

        this.graphEditor.onNodeSelect = (node) => this._onNodeSelect(node);
        this.graphEditor.onNodeDoubleClick = (node) => this._onNodeDoubleClick(node);

        // Init code editor
        this.codeEditor = new CodeEditor(document.getElementById('editor-container'));
        await this.codeEditor.init();

        // Init WASM bridge
        await this.bridge.init();

        // Populate problem selector
        this._setupProblemSelector();

        // Setup button handlers
        this._setupControls();

        // Setup palette drag
        this._setupPalette();

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
                e.preventDefault();
                this._runTests();
            }
        });

        this._log('info', 'SystemForge initialized');
    }

    _setupProblemSelector() {
        const select = document.getElementById('problem-select');
        const problems = this.problemLoader.getProblems();

        problems.forEach(p => {
            const option = document.createElement('option');
            option.value = p.id;
            option.textContent = `[${p.difficulty}] ${p.name}`;
            select.appendChild(option);
        });

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
                const type = btn.dataset.type;
                this.graphEditor.addNode(type);
            });
        });
    }

    _loadProblem(problemId) {
        const problem = this.problemLoader.getProblem(problemId);
        if (!problem) return;

        this.currentProblem = problem;

        // Load graph
        this.graphEditor.loadGraph(problem.graph);

        // Load starter code
        if (problem.starterCode) {
            for (const [nodeId, code] of Object.entries(problem.starterCode)) {
                this.codeEditor.setCode(nodeId, code);
            }
        }

        // Show description
        this._renderDescription(problem.description);

        // Clear test results
        this._clearTestResults();

        // Setup code tabs
        this._setupCodeTabs(problem);

        // Select the first server node for code editing
        const serverNode = this.graphEditor.nodes.find(n => n.type === 'HttpServer');
        if (serverNode) {
            this.graphEditor.selectNode(serverNode);
            this.codeEditor.setActiveNode(serverNode.id, problem.starterCode?.[serverNode.id]);
        }

        this._log('info', `Loaded problem: ${problem.name}`);
    }

    _setupCodeTabs(problem) {
        const tabs = document.getElementById('code-tabs');
        tabs.innerHTML = '';

        const codeNodes = this.graphEditor.nodes.filter(n => n.type === 'HttpServer');
        codeNodes.forEach(node => {
            const tab = document.createElement('button');
            tab.className = 'code-tab';
            tab.textContent = node.id;
            tab.dataset.nodeId = node.id;
            tab.addEventListener('click', () => {
                tabs.querySelectorAll('.code-tab').forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                this.codeEditor.setActiveNode(node.id, problem.starterCode?.[node.id]);
            });
            tabs.appendChild(tab);
        });

        // Activate first tab
        const firstTab = tabs.querySelector('.code-tab');
        if (firstTab) firstTab.classList.add('active');
    }

    async _runTests() {
        if (!this.currentProblem) {
            this._log('error', 'No problem loaded');
            return;
        }

        this._log('info', 'Running tests...');

        // For now, simulate test running in the browser
        // In full WASM mode, we'd use the bridge
        const results = this._runTestsLocally();
        this._renderTestResults(results);
    }

    _runTestsLocally() {
        // Mock test execution using problem definition
        // In production, this goes through the WASM bridge
        const problem = this.currentProblem;
        const results = {
            total: problem.testCases.length,
            passed: 0,
            failed: 0,
            results: [],
        };

        // Check if the user has written any code
        const serverNode = this.graphEditor.nodes.find(n => n.type === 'HttpServer');
        const code = serverNode ? this.codeEditor.getCode(serverNode.id) : '';
        const hasImplementation = code && !code.includes('Not implemented');

        for (const tc of problem.testCases) {
            const result = {
                name: tc.name,
                passed: hasImplementation, // simplified
                error: hasImplementation ? '' : 'Handler returns "Not implemented"',
                logs: [],
            };
            if (result.passed) results.passed++;
            else results.failed++;
            results.results.push(result);
        }

        results.all_passed = results.failed === 0;
        return results;
    }

    _stepEvent() {
        this._log('info', 'Step event (WASM required for full functionality)');
    }

    _reset() {
        if (this.currentProblem) {
            this._loadProblem(this.currentProblem.id);
        }
        this._log('info', 'Reset');
    }

    _onNodeSelect(node) {
        if (node) {
            document.getElementById('code-title').textContent = `Code — ${node.id}`;

            // Activate corresponding tab
            document.querySelectorAll('.code-tab').forEach(tab => {
                tab.classList.toggle('active', tab.dataset.nodeId === node.id);
            });
        }
    }

    _onNodeDoubleClick(node) {
        if (node && (node.type === 'HttpServer' || node.type === 'Worker')) {
            this.codeEditor.setActiveNode(node.id);
        }
    }

    _renderDescription(markdown) {
        const container = document.getElementById('problem-description');
        // Simple markdown to HTML conversion
        let html = markdown
            .replace(/^### (.*$)/gm, '<h3>$1</h3>')
            .replace(/^## (.*$)/gm, '<h2>$1</h2>')
            .replace(/^# (.*$)/gm, '<h1>$1</h1>')
            .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
            .replace(/`([^`]+)`/g, '<code>$1</code>')
            .replace(/^- (.*$)/gm, '<li>$1</li>')
            .replace(/(<li>.*<\/li>)/s, '<ul>$1</ul>')
            .replace(/\n\n/g, '</p><p>')
            .replace(/\n/g, '<br>');
        html = '<p>' + html + '</p>';
        container.innerHTML = html;
    }

    _renderTestResults(results) {
        const container = document.getElementById('test-results');

        let html = '';

        // Summary
        const summaryClass = results.all_passed ? 'all-pass' : 'has-fail';
        html += `<div class="test-summary ${summaryClass}">`;
        if (results.all_passed) {
            html += `<i class="fas fa-check-circle" style="color: var(--accent-green)"></i>`;
            html += `<span>${results.passed}/${results.total} tests passed</span>`;
        } else {
            html += `<i class="fas fa-times-circle" style="color: var(--accent-red)"></i>`;
            html += `<span>${results.passed}/${results.total} passed, ${results.failed} failed</span>`;
        }
        html += '</div>';

        // Individual results
        for (const r of results.results) {
            const cls = r.passed ? 'pass' : 'fail';
            const icon = r.passed ? 'fa-check' : 'fa-times';
            html += `<div class="test-item ${cls}">`;
            html += `<i class="fas ${icon} test-icon"></i>`;
            html += `<span class="test-name">${r.name}</span>`;
            html += '</div>';
            if (r.error) {
                html += `<div class="test-item fail"><span class="test-error" style="margin-left: 22px">${r.error}</span></div>`;
            }
        }

        container.innerHTML = html;

        this._log(results.all_passed ? 'info' : 'error',
                  `Tests: ${results.passed}/${results.total} passed`);
    }

    _clearTestResults() {
        document.getElementById('test-results').innerHTML = `
            <div class="placeholder-text">
                <i class="fas fa-vial"></i>
                <p>Run tests to see results</p>
            </div>`;
    }

    _log(level, message) {
        const container = document.getElementById('console-output');
        const time = new Date().toLocaleTimeString();
        const cls = level === 'error' ? 'log-error' : 'log-info';
        container.innerHTML += `<div class="log-entry"><span class="log-time">${time}</span><span class="${cls}">${message}</span></div>`;
        container.scrollTop = container.scrollHeight;
    }
}

// Boot
const app = new App();
app.init().catch(err => console.error('App init failed:', err));
