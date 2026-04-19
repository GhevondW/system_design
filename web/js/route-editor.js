/**
 * Route Editor — modal dialog for editing HttpServer routes.
 *
 * openRouteEditor(editor, node) lets the user add/remove/edit route rows
 * (method + path + handler function name). On save it replaces node.routes
 * and calls editor.draw() to refresh route-count badges on the node.
 */

export function openRouteEditor(editor, node) {
    if (node.type !== 'HttpServer') return;
    if (!node.routes) node.routes = [];

    const existing = document.getElementById('route-editor-modal');
    if (existing) existing.remove();

    const modal = document.createElement('div');
    modal.id = 'route-editor-modal';
    modal.style.cssText = `
        position: fixed; inset: 0; z-index: 2000;
        background: rgba(0,0,0,0.6); display: flex;
        align-items: center; justify-content: center;
        font-family: -apple-system, sans-serif;
    `;

    const dialog = document.createElement('div');
    dialog.style.cssText = `
        background: #1c2128; border: 1px solid #30363d; border-radius: 12px;
        padding: 24px; min-width: 520px; max-width: 700px; max-height: 80vh;
        overflow-y: auto; color: #e6edf3; box-shadow: 0 16px 48px rgba(0,0,0,0.5);
    `;

    const renderDialog = () => {
        dialog.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:16px">
                <h2 style="margin:0; font-size:18px; color:#e6edf3">
                    <i class="fas fa-route" style="color:#3fb950; margin-right:8px"></i>
                    API Routes — ${node.id}
                </h2>
                <button id="route-close" style="background:none; border:none; color:#8b949e; font-size:18px; cursor:pointer; padding:4px 8px">
                    <i class="fas fa-times"></i>
                </button>
            </div>

            <p style="font-size:12px; color:#8b949e; margin:0 0 12px">
                Define the API endpoints this server handles. Each route maps an HTTP method + path to a handler function you write in the code editor.
            </p>

            <div style="display:grid; grid-template-columns:90px 1fr 1fr 32px; gap:6px; font-size:11px; color:#8b949e; padding:0 4px; margin-bottom:6px">
                <span>Method</span><span>Path</span><span>Handler Function</span><span></span>
            </div>

            <div id="route-rows">
                ${node.routes.map((r, idx) => `
                    <div class="route-row" style="display:grid; grid-template-columns:90px 1fr 1fr 32px; gap:6px; margin-bottom:6px">
                        <select class="route-method" style="
                            padding:6px; background:#0d1117; border:1px solid #30363d;
                            border-radius:4px; color:#e6edf3; font-size:13px;
                        ">
                            ${['GET','POST','PUT','DELETE'].map(m =>
                                `<option value="${m}" ${r.method === m ? 'selected' : ''}>${m}</option>`
                            ).join('')}
                        </select>
                        <input class="route-path" type="text" value="${r.path || ''}" placeholder="/endpoint" style="
                            padding:6px 8px; background:#0d1117; border:1px solid #30363d;
                            border-radius:4px; color:#e6edf3; font-size:13px;
                        "/>
                        <input class="route-handler" type="text" value="${r.handler || ''}" placeholder="handle_name" style="
                            padding:6px 8px; background:#0d1117; border:1px solid #30363d;
                            border-radius:4px; color:#e6edf3; font-size:13px; font-family: monospace;
                        "/>
                        <button class="route-delete" data-idx="${idx}" style="
                            background:none; border:none; color:#f85149; cursor:pointer; font-size:12px; padding:6px;
                        "><i class="fas fa-times"></i></button>
                    </div>
                `).join('')}
            </div>

            <button id="route-add" style="
                margin-top:8px; padding:8px 16px; background:#238636; border:none;
                border-radius:6px; color:white; font-size:13px; cursor:pointer;
                display:flex; align-items:center; gap:6px;
            "><i class="fas fa-plus"></i> Add Route</button>

            <div style="display:flex; justify-content:flex-end; gap:8px; margin-top:20px; padding-top:16px; border-top:1px solid #30363d">
                <button id="route-cancel" style="
                    padding:8px 16px; background:#21262d; border:1px solid #30363d;
                    border-radius:6px; color:#e6edf3; font-size:13px; cursor:pointer;
                ">Cancel</button>
                <button id="route-save" style="
                    padding:8px 16px; background:#1f6feb; border:none;
                    border-radius:6px; color:white; font-size:13px; cursor:pointer; font-weight:500;
                ">Save Routes</button>
            </div>
        `;

        dialog.querySelector('#route-close').onclick = () => modal.remove();
        dialog.querySelector('#route-cancel').onclick = () => modal.remove();

        dialog.querySelector('#route-add').onclick = () => {
            node.routes.push({ method: 'POST', path: '/', handler: '' });
            renderDialog();
        };

        dialog.querySelector('#route-save').onclick = () => {
            let hasError = false;
            const newRoutes = [];
            dialog.querySelectorAll('.route-row').forEach(row => {
                const method = row.querySelector('.route-method').value;
                const path = row.querySelector('.route-path').value.trim();
                const handler = row.querySelector('.route-handler').value.trim();

                if (!path) { row.querySelector('.route-path').style.borderColor = '#f85149'; hasError = true; }
                if (!handler) { row.querySelector('.route-handler').style.borderColor = '#f85149'; hasError = true; }

                if (path && handler) newRoutes.push({ method, path, handler });
            });
            if (hasError) return;
            node.routes = newRoutes;
            editor.draw();
            modal.remove();
        };

        dialog.querySelectorAll('.route-delete').forEach(btn => {
            btn.onclick = () => {
                node.routes.splice(parseInt(btn.dataset.idx), 1);
                renderDialog();
            };
        });
    };

    renderDialog();
    modal.appendChild(dialog);
    document.body.appendChild(modal);
    modal.addEventListener('click', (e) => { if (e.target === modal) modal.remove(); });
    const onKey = (e) => {
        if (e.key === 'Escape') {
            modal.remove();
            document.removeEventListener('keydown', onKey);
        }
    };
    document.addEventListener('keydown', onKey);
}
