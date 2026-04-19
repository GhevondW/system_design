/**
 * CodeEditor — Monaco Editor wrapper with SysLang syntax highlighting.
 */
export class CodeEditor {
    constructor(container) {
        this.container = container;
        this.editor = null;
        this.currentNodeId = null;
        this.codeMap = new Map(); // nodeId -> code
        this.onCodeChange = null;
    }

    async init() {
        // Load Monaco from CDN
        await this._loadMonaco();
        this._registerSysLang();

        this.editor = monaco.editor.create(this.container, {
            value: '// Select a component to edit its code',
            language: 'syslang',
            theme: 'systemforge-dark',
            minimap: { enabled: false },
            fontSize: 13,
            fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
            lineNumbers: 'on',
            renderLineHighlight: 'line',
            scrollBeyondLastLine: false,
            automaticLayout: true,
            tabSize: 4,
            padding: { top: 8 },
        });

        this.editor.onDidChangeModelContent(() => {
            if (this.currentNodeId) {
                this.codeMap.set(this.currentNodeId, this.editor.getValue());
                if (this.onCodeChange) {
                    this.onCodeChange(this.currentNodeId, this.editor.getValue());
                }
            }
        });
    }

    setActiveNode(nodeId, defaultCode) {
        // Save current
        if (this.currentNodeId) {
            this.codeMap.set(this.currentNodeId, this.editor.getValue());
        }

        this.currentNodeId = nodeId;
        const code = this.codeMap.get(nodeId) || defaultCode || '';
        this.editor.setValue(code);
    }

    getCode(nodeId) {
        if (nodeId === this.currentNodeId) {
            return this.editor.getValue();
        }
        return this.codeMap.get(nodeId) || '';
    }

    setCode(nodeId, code) {
        this.codeMap.set(nodeId, code);
        if (nodeId === this.currentNodeId) {
            this.editor.setValue(code);
        }
    }

    _loadMonaco() {
        return new Promise((resolve) => {
            if (window.monaco) { resolve(); return; }

            const script = document.createElement('script');
            script.src = 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0/min/vs/loader.min.js';
            script.onload = () => {
                require.config({
                    paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0/min/vs' }
                });
                require(['vs/editor/editor.main'], () => resolve());
            };
            document.head.appendChild(script);
        });
    }

    _registerSysLang() {
        monaco.languages.register({ id: 'syslang' });

        monaco.languages.setMonarchTokensProvider('syslang', {
            keywords: ['let', 'fn', 'return', 'if', 'else', 'for', 'while', 'match', 'true', 'false', 'null'],
            operators: ['+', '-', '*', '/', '%', '==', '!=', '<', '<=', '>', '>=', '&&', '||', '!', '=', '=>'],

            tokenizer: {
                root: [
                    [/\/\/.*$/, 'comment'],
                    [/\/\*/, 'comment', '@comment'],
                    [/"([^"\\]|\\.)*"/, 'string'],
                    [/\d+\.\d+/, 'number.float'],
                    [/\d+/, 'number'],
                    [/[a-zA-Z_]\w*/, {
                        cases: {
                            '@keywords': 'keyword',
                            '@default': 'identifier'
                        }
                    }],
                    [/[{}()\[\]]/, '@brackets'],
                    [/[=><!+\-*/%&|]+/, 'operator'],
                    [/[;,.:?]/, 'delimiter'],
                ],
                comment: [
                    [/[^/*]+/, 'comment'],
                    [/\*\//, 'comment', '@pop'],
                    [/[/*]/, 'comment'],
                ],
            }
        });

        monaco.editor.defineTheme('systemforge-dark', {
            base: 'vs-dark',
            inherit: true,
            rules: [
                { token: 'keyword', foreground: 'ff7b72', fontStyle: 'bold' },
                { token: 'string', foreground: 'a5d6ff' },
                { token: 'number', foreground: '79c0ff' },
                { token: 'number.float', foreground: '79c0ff' },
                { token: 'comment', foreground: '8b949e', fontStyle: 'italic' },
                { token: 'identifier', foreground: 'e6edf3' },
                { token: 'operator', foreground: 'ff7b72' },
                { token: 'delimiter', foreground: '8b949e' },
            ],
            colors: {
                'editor.background': '#0d1117',
                'editor.foreground': '#e6edf3',
                'editor.lineHighlightBackground': '#161b22',
                'editorCursor.foreground': '#58a6ff',
                'editor.selectionBackground': '#264f78',
            }
        });

        // Basic autocomplete
        monaco.languages.registerCompletionItemProvider('syslang', {
            provideCompletionItems: () => {
                const suggestions = [
                    ...['let', 'fn', 'return', 'if', 'else', 'for', 'while', 'match', 'true', 'false', 'null'].map(kw => ({
                        label: kw,
                        kind: monaco.languages.CompletionItemKind.Keyword,
                        insertText: kw,
                    })),
                    ...['log', 'print', 'len', 'str', 'int', 'float'].map(fn => ({
                        label: fn,
                        kind: monaco.languages.CompletionItemKind.Function,
                        insertText: fn + '($0)',
                        insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
                    })),
                    ...['db.query', 'db.execute', 'db.createTable', 'cache.get', 'cache.set', 'cache.del', 'cache.has'].map(fn => ({
                        label: fn,
                        kind: monaco.languages.CompletionItemKind.Method,
                        insertText: fn + '($0)',
                        insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
                    })),
                ];
                return { suggestions };
            }
        });
    }
}
