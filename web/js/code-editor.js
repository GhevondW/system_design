/**
 * CodeEditor — Monaco Editor wrapper with SysLang syntax highlighting and autocomplete.
 */
export class CodeEditor {
    constructor(container) {
        this.container = container;
        this.editor = null;
        this.currentNodeId = null;
        this.codeMap = new Map(); // nodeId -> code
        this.onCodeChange = null;
        this.connectedComponents = []; // [{alias, type}] — set by app when graph changes
    }

    async init() {
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
            quickSuggestions: true,
            suggestOnTriggerCharacters: true,
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

    /** Update connected components for context-aware autocomplete */
    setConnectedComponents(components) {
        this.connectedComponents = components; // [{alias, type}]
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

        // --- Syntax highlighting ---
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
                        cases: { '@keywords': 'keyword', '@default': 'identifier' }
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

        // --- Theme ---
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

        // --- Autocomplete ---
        monaco.languages.registerCompletionItemProvider('syslang', {
            triggerCharacters: ['.', '"'],
            provideCompletionItems: (model, position) => {
                const textUntilPosition = model.getValueInRange({
                    startLineNumber: position.lineNumber,
                    startColumn: 1,
                    endLineNumber: position.lineNumber,
                    endColumn: position.column,
                });

                const wordInfo = model.getWordUntilPosition(position);
                const range = {
                    startLineNumber: position.lineNumber,
                    endLineNumber: position.lineNumber,
                    startColumn: wordInfo.startColumn,
                    endColumn: wordInfo.endColumn,
                };

                // Check if typing after a dot (e.g. "db." or "cache." or "req.")
                const dotMatch = textUntilPosition.match(/(\w+)\.\s*$/);
                if (dotMatch) {
                    const obj = dotMatch[1];
                    return { suggestions: this._getDotCompletions(obj, range) };
                }

                // Check if inside a db method call for table names: db.Find("
                const tableMatch = textUntilPosition.match(/\w+\.\w+\(\s*"$/);
                if (tableMatch) {
                    return { suggestions: this._getTableNameCompletions(range) };
                }

                // Default: keywords + builtins + connected component aliases
                return { suggestions: this._getTopLevelCompletions(range) };
            }
        });
    }

    _getTopLevelCompletions(range) {
        const suggestions = [];
        const Kind = monaco.languages.CompletionItemKind;
        const SnippetRule = monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet;

        // Keywords
        for (const kw of ['let', 'fn', 'return', 'if', 'else', 'for', 'while', 'match', 'true', 'false', 'null']) {
            suggestions.push({ label: kw, kind: Kind.Keyword, insertText: kw, range });
        }

        // Keyword snippets
        suggestions.push({
            label: 'fn', kind: Kind.Snippet, insertText: 'fn ${1:name}(${2:req}) {\n\t$0\n}',
            insertTextRules: SnippetRule, range, detail: 'function declaration',
            sortText: '0fn',
        });
        suggestions.push({
            label: 'if', kind: Kind.Snippet, insertText: 'if (${1:condition}) {\n\t$0\n}',
            insertTextRules: SnippetRule, range, detail: 'if statement',
            sortText: '0if',
        });
        suggestions.push({
            label: 'ifelse', kind: Kind.Snippet, insertText: 'if (${1:condition}) {\n\t$2\n} else {\n\t$0\n}',
            insertTextRules: SnippetRule, range, detail: 'if/else statement',
        });
        suggestions.push({
            label: 'for', kind: Kind.Snippet, insertText: 'for (let ${1:i} = 0; ${1:i} < ${2:n}; ${1:i} = ${1:i} + 1) {\n\t$0\n}',
            insertTextRules: SnippetRule, range, detail: 'for loop',
            sortText: '0for',
        });
        suggestions.push({
            label: 'handler', kind: Kind.Snippet,
            insertText: 'fn ${1:handle}(req) {\n\t$0\n\treturn { status: 200, body: {} };\n}',
            insertTextRules: SnippetRule, range, detail: 'HTTP handler function',
        });

        // Built-in functions
        for (const fn of ['log', 'print', 'len', 'str', 'int', 'float']) {
            suggestions.push({
                label: fn, kind: Kind.Function,
                insertText: fn + '(${0})', insertTextRules: SnippetRule, range,
                detail: 'built-in function',
            });
        }

        // Connected component aliases (db, cache, etc.)
        for (const comp of this.connectedComponents) {
            suggestions.push({
                label: comp.alias, kind: Kind.Variable, insertText: comp.alias, range,
                detail: comp.type + ' component',
            });
        }

        // Request object
        suggestions.push({ label: 'req', kind: Kind.Variable, insertText: 'req', range, detail: 'request object' });

        return suggestions;
    }

    _getDotCompletions(obj, range) {
        const suggestions = [];
        const Kind = monaco.languages.CompletionItemKind;
        const SnippetRule = monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet;

        // Find the component type for this alias
        const comp = this.connectedComponents.find(c => c.alias === obj);

        if (comp && comp.type === 'Database') {
            const dbMethods = [
                { label: 'FindOne', insert: 'FindOne("${1:table}", {${2}})', detail: '(table, filter) → row or null' },
                { label: 'Find', insert: 'Find("${1:table}", {${2}})', detail: '(table, filter) → list of rows' },
                { label: 'Insert', insert: 'Insert("${1:table}", {${2}})', detail: '(table, row) → {inserted: true}' },
                { label: 'Update', insert: 'Update("${1:table}", {${2:filter}}, {${3:updates}})', detail: '(table, filter, updates) → {updated: N}' },
                { label: 'Delete', insert: 'Delete("${1:table}", {${2}})', detail: '(table, filter) → {deleted: N}' },
                { label: 'All', insert: 'All("${1:table}")', detail: '(table) → all rows' },
                { label: 'Count', insert: 'Count("${1:table}", {${2}})', detail: '(table, filter) → number' },
            ];
            for (const m of dbMethods) {
                suggestions.push({
                    label: m.label, kind: Kind.Method,
                    insertText: m.insert, insertTextRules: SnippetRule, range,
                    detail: m.detail, sortText: '0' + m.label,
                });
            }
        } else if (comp && comp.type === 'Cache') {
            const cacheMethods = [
                { label: 'get', insert: 'get("${1:key}")', detail: '(key) → value or null' },
                { label: 'set', insert: 'set("${1:key}", ${2:value})', detail: '(key, value) → null' },
                { label: 'del', insert: 'del("${1:key}")', detail: '(key) → true/false' },
                { label: 'has', insert: 'has("${1:key}")', detail: '(key) → true/false' },
            ];
            for (const m of cacheMethods) {
                suggestions.push({
                    label: m.label, kind: Kind.Method,
                    insertText: m.insert, insertTextRules: SnippetRule, range,
                    detail: m.detail,
                });
            }
        } else if (obj === 'req') {
            for (const prop of [
                { label: 'body', detail: 'request body (map)' },
                { label: 'method', detail: 'HTTP method (string)' },
                { label: 'path', detail: 'request path (string)' },
                { label: 'params', detail: 'URL parameters (map)' },
            ]) {
                suggestions.push({
                    label: prop.label, kind: Kind.Property,
                    insertText: prop.label, range, detail: prop.detail,
                });
            }
        }

        // Generic list/map/string methods for any object
        const genericMethods = [
            { label: 'size', insert: 'size()', detail: '() → length of list/map/string' },
            { label: 'push', insert: 'push(${0})', detail: '(value) → add to list' },
            { label: 'contains', insert: 'contains("${0}")', detail: '(substr) → bool' },
            { label: 'keys', insert: 'keys()', detail: '() → list of map keys' },
        ];
        for (const m of genericMethods) {
            suggestions.push({
                label: m.label, kind: Kind.Method,
                insertText: m.insert, insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
                range, detail: m.detail, sortText: '9' + m.label, // sort after specific methods
            });
        }

        return suggestions;
    }

    _getTableNameCompletions(range) {
        const suggestions = [];
        // Gather table names from all connected database components
        for (const comp of this.connectedComponents) {
            if (comp.type === 'Database' && comp.tables) {
                for (const tableName of Object.keys(comp.tables)) {
                    suggestions.push({
                        label: tableName,
                        kind: monaco.languages.CompletionItemKind.Value,
                        insertText: tableName,
                        range,
                        detail: 'table in ' + comp.alias,
                    });
                }
            }
        }
        return suggestions;
    }
}
