/**
 * Schema Editor — modal dialog for editing Database component tables.
 *
 * openSchemaEditor(editor, node) opens a dialog that lets the user rename the
 * node, add/remove tables, and edit each table's columns. On save it mutates
 * `node.tables` (and `node.id` + any touching edges if the id changed), then
 * calls editor.draw() to refresh the canvas.
 */

export function openSchemaEditor(editor, node) {
    if (node.type !== 'Database') return;

    if (!node.tables) node.tables = {};

    // Remove any existing schema dialog — dialogs are singletons per node type.
    const existing = document.getElementById('schema-editor-modal');
    if (existing) existing.remove();

    const modal = document.createElement('div');
    modal.id = 'schema-editor-modal';
    modal.style.cssText = `
        position: fixed; inset: 0; z-index: 2000;
        background: rgba(0,0,0,0.6); display: flex;
        align-items: center; justify-content: center;
        font-family: -apple-system, sans-serif;
    `;

    const dialog = document.createElement('div');
    dialog.style.cssText = `
        background: #1c2128; border: 1px solid #30363d; border-radius: 12px;
        padding: 24px; min-width: 500px; max-width: 700px; max-height: 80vh;
        overflow-y: auto; color: #e6edf3; box-shadow: 0 16px 48px rgba(0,0,0,0.5);
    `;

    const renderDialog = () => {
        dialog.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:16px">
                <h2 style="margin:0; font-size:18px; color:#e6edf3">
                    <i class="fas fa-database" style="color:#d29922; margin-right:8px"></i>
                    Database Schema — ${node.id}
                </h2>
                <button id="schema-close" style="background:none; border:none; color:#8b949e; font-size:18px; cursor:pointer; padding:4px 8px">
                    <i class="fas fa-times"></i>
                </button>
            </div>

            <div style="margin-bottom:16px">
                <label style="display:block; font-size:12px; color:#8b949e; margin-bottom:4px">Node ID (used as variable name in code)</label>
                <input id="schema-node-id" type="text" value="${node.id}" style="
                    width:100%; padding:8px 12px; background:#0d1117; border:1px solid #30363d;
                    border-radius:6px; color:#e6edf3; font-size:14px; box-sizing:border-box;
                "/>
            </div>

            <div id="schema-tables">
                ${Object.entries(node.tables).map(([tableName, columns]) => renderTableEditor(tableName, columns)).join('')}
            </div>

            <button id="schema-add-table" style="
                margin-top:12px; padding:8px 16px; background:#238636; border:none;
                border-radius:6px; color:white; font-size:13px; cursor:pointer;
                display:flex; align-items:center; gap:6px;
            "><i class="fas fa-plus"></i> Add Table</button>

            <div style="display:flex; justify-content:flex-end; gap:8px; margin-top:20px; padding-top:16px; border-top:1px solid #30363d">
                <button id="schema-cancel" style="
                    padding:8px 16px; background:#21262d; border:1px solid #30363d;
                    border-radius:6px; color:#e6edf3; font-size:13px; cursor:pointer;
                ">Cancel</button>
                <button id="schema-save" style="
                    padding:8px 16px; background:#1f6feb; border:none;
                    border-radius:6px; color:white; font-size:13px; cursor:pointer; font-weight:500;
                ">Save Schema</button>
            </div>
        `;

        dialog.querySelector('#schema-close').onclick = () => modal.remove();
        dialog.querySelector('#schema-cancel').onclick = () => modal.remove();
        dialog.querySelector('#schema-add-table').onclick = () => {
            const name = 'table_' + (Object.keys(node.tables).length + 1);
            node.tables[name] = [{ name: 'id', type: 'int' }];
            renderDialog();
        };

        dialog.querySelector('#schema-save').onclick = () => {
            let hasError = false;
            const nodeIdInput = dialog.querySelector('#schema-node-id');
            const newId = nodeIdInput.value.trim();
            if (!newId) {
                nodeIdInput.style.borderColor = '#f85149';
                hasError = true;
            } else {
                nodeIdInput.style.borderColor = '#30363d';
            }

            dialog.querySelectorAll('.schema-table-name').forEach(input => {
                if (!input.value.trim()) {
                    input.style.borderColor = '#f85149';
                    hasError = true;
                } else {
                    input.style.borderColor = '#30363d';
                }
            });
            dialog.querySelectorAll('.schema-col-name').forEach(input => {
                if (!input.value.trim()) {
                    input.style.borderColor = '#f85149';
                    hasError = true;
                } else {
                    input.style.borderColor = '#30363d';
                }
            });

            if (hasError) return;

            // Propagate an id rename to every edge referencing this node, so
            // connections survive the save.
            if (newId !== node.id) {
                editor.edges.forEach(e => {
                    if (e.from === node.id) e.from = newId;
                    if (e.to === node.id) { e.to = newId; e.alias = newId; }
                });
                node.id = newId;
            }

            const newTables = {};
            dialog.querySelectorAll('.schema-table-block').forEach(block => {
                const tName = block.querySelector('.schema-table-name').value.trim();
                if (!tName) return;
                const cols = [];
                block.querySelectorAll('.schema-col-row').forEach(row => {
                    const colName = row.querySelector('.schema-col-name').value.trim();
                    const colType = row.querySelector('.schema-col-type').value;
                    if (colName) cols.push({ name: colName, type: colType });
                });
                newTables[tName] = cols;
            });
            node.tables = newTables;
            editor.draw();
            modal.remove();
        };

        dialog.querySelectorAll('.schema-delete-table').forEach(btn => {
            btn.onclick = () => {
                delete node.tables[btn.dataset.table];
                renderDialog();
            };
        });

        dialog.querySelectorAll('.schema-add-col').forEach(btn => {
            btn.onclick = () => {
                node.tables[btn.dataset.table].push({ name: '', type: 'string' });
                renderDialog();
            };
        });

        dialog.querySelectorAll('.schema-delete-col').forEach(btn => {
            btn.onclick = () => {
                const tableName = btn.dataset.table;
                const colIdx = parseInt(btn.dataset.colIdx);
                node.tables[tableName].splice(colIdx, 1);
                renderDialog();
            };
        });
    };

    renderDialog();
    modal.appendChild(dialog);
    document.body.appendChild(modal);

    modal.addEventListener('click', (e) => {
        if (e.target === modal) modal.remove();
    });

    const onKey = (e) => {
        if (e.key === 'Escape') {
            modal.remove();
            document.removeEventListener('keydown', onKey);
        }
    };
    document.addEventListener('keydown', onKey);
}

function renderTableEditor(tableName, columns) {
    // Columns may come from older saved graphs as plain strings — normalize
    // them to { name, type } so the editor always renders a type dropdown.
    const cols = Array.isArray(columns) ? columns.map(c =>
        typeof c === 'string' ? { name: c, type: 'any' } : c
    ) : [];

    return `
        <div class="schema-table-block" style="
            background:#0d1117; border:1px solid #30363d; border-radius:8px;
            padding:12px; margin-bottom:12px;
        ">
            <div style="display:flex; align-items:center; gap:8px; margin-bottom:10px">
                <i class="fas fa-table" style="color:#d29922; font-size:12px"></i>
                <input class="schema-table-name" type="text" value="${tableName}" style="
                    flex:1; padding:6px 10px; background:#161b22; border:1px solid #30363d;
                    border-radius:4px; color:#e6edf3; font-size:14px; font-weight:600;
                "/>
                <button class="schema-delete-table" data-table="${tableName}" style="
                    background:none; border:none; color:#f85149; cursor:pointer; padding:4px 8px; font-size:12px;
                "><i class="fas fa-trash"></i></button>
            </div>

            <div style="display:grid; grid-template-columns:1fr 100px 32px; gap:4px; font-size:11px; color:#8b949e; padding:0 4px; margin-bottom:4px">
                <span>Column</span><span>Type</span><span></span>
            </div>

            ${cols.map((col, idx) => `
                <div class="schema-col-row" style="display:grid; grid-template-columns:1fr 100px 32px; gap:4px; margin-bottom:4px">
                    <input class="schema-col-name" type="text" value="${col.name}" placeholder="column name" style="
                        padding:5px 8px; background:#161b22; border:1px solid #30363d;
                        border-radius:4px; color:#e6edf3; font-size:13px;
                    "/>
                    <select class="schema-col-type" style="
                        padding:5px 6px; background:#161b22; border:1px solid #30363d;
                        border-radius:4px; color:#e6edf3; font-size:12px;
                    ">
                        <option value="string" ${col.type === 'string' ? 'selected' : ''}>string</option>
                        <option value="int" ${col.type === 'int' ? 'selected' : ''}>int</option>
                        <option value="float" ${col.type === 'float' ? 'selected' : ''}>float</option>
                        <option value="bool" ${col.type === 'bool' ? 'selected' : ''}>bool</option>
                        <option value="any" ${col.type === 'any' ? 'selected' : ''}>any</option>
                    </select>
                    <button class="schema-delete-col" data-table="${tableName}" data-col-idx="${idx}" style="
                        background:none; border:none; color:#f85149; cursor:pointer; font-size:11px; padding:4px;
                    "><i class="fas fa-times"></i></button>
                </div>
            `).join('')}

            <button class="schema-add-col" data-table="${tableName}" style="
                margin-top:6px; padding:4px 10px; background:#21262d; border:1px solid #30363d;
                border-radius:4px; color:#8b949e; font-size:12px; cursor:pointer;
                display:flex; align-items:center; gap:4px;
            "><i class="fas fa-plus" style="font-size:10px"></i> Add Column</button>
        </div>
    `;
}
