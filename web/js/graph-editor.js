/**
 * GraphEditor — Canvas-based visual graph editor for system components.
 *
 * Interactions:
 *   Left click node:       Select node
 *   Left drag node:        Move node
 *   Left drag empty:       Pan canvas
 *   Left drag from port:   Create edge
 *   Right click node:      Context menu (Edit Code, Delete, Disconnect)
 *   Right click edge:      Context menu (Delete Edge)
 *   Right click empty:     Context menu (Add component)
 *   Double click node:     Open code editor for that component
 *   Mouse wheel:           Zoom in/out
 *   Delete/Backspace:      Delete selected node or edge
 */

const NODE_WIDTH = 160;
const NODE_HEIGHT = 70;
const PORT_RADIUS = 7;
const EDGE_HIT_TOLERANCE = 8;

const COMPONENT_COLORS = {
    Client: '#58a6ff',
    HttpServer: '#3fb950',
    Database: '#d29922',
    Cache: '#bc8cff',
    LoadBalancer: '#f778ba',
};

const CODABLE_TYPES = new Set(['HttpServer', 'Worker']);

export class GraphEditor {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.nodes = [];
        this.edges = [];
        this.selectedNode = null;
        this.selectedEdge = null;
        this.dragNode = null;
        this.dragOffset = { x: 0, y: 0 };
        this.connectingFrom = null;
        this.mousePos = { x: 0, y: 0 };
        this.nextId = 1;

        // Callbacks
        this.onNodeSelect = null;
        this.onNodeDoubleClick = null;
        this.onEditCode = null;

        // Pan & zoom
        this.panX = 0;
        this.panY = 0;
        this.zoom = 1;
        this.isPanning = false;
        this.panStart = { x: 0, y: 0 };

        // Context menu element
        this.contextMenu = this._createContextMenu();

        this._setupEvents();
        this._resize();
        window.addEventListener('resize', () => this._resize());
    }

    // --- Public API ---

    addNode(type, x, y) {
        const id = type.toLowerCase().replace('httpserver', 'server') + '_' + this.nextId++;
        const node = {
            id, type,
            x: x ?? ((-this.panX + this.canvas.width / 2) / this.zoom - NODE_WIDTH / 2 + (this.nodes.length % 3) * 40),
            y: y ?? ((-this.panY + this.canvas.height / 2) / this.zoom - NODE_HEIGHT / 2 + (this.nodes.length % 3) * 40),
            width: NODE_WIDTH,
            height: NODE_HEIGHT,
        };
        if (type === 'Database') node.tables = {};
        if (type === 'HttpServer') node.routes = [];
        this.nodes.push(node);
        this.selectNode(node);
        this.draw();
        // Auto-open editors for new nodes
        if (type === 'Database') this.openSchemaEditor(node);
        if (type === 'HttpServer') this.openRouteEditor(node);
        return node;
    }

    addEdge(fromId, toId, alias) {
        if (fromId === toId) return;
        if (this.edges.find(e => e.from === fromId && e.to === toId)) return;
        this.edges.push({ from: fromId, to: toId, alias: alias || toId.split('_')[0] });
        this.draw();
    }

    removeNode(node) {
        this.nodes = this.nodes.filter(n => n !== node);
        this.edges = this.edges.filter(e => e.from !== node.id && e.to !== node.id);
        if (this.selectedNode === node) { this.selectedNode = null; }
        this.draw();
        if (this.onNodeSelect) this.onNodeSelect(null);
    }

    removeEdge(edge) {
        this.edges = this.edges.filter(e => e !== edge);
        if (this.selectedEdge === edge) { this.selectedEdge = null; }
        this.draw();
    }

    disconnectNode(node) {
        this.edges = this.edges.filter(e => e.from !== node.id && e.to !== node.id);
        this.draw();
    }

    selectNode(node) {
        this.selectedNode = node;
        this.selectedEdge = null;
        this.draw();
        if (this.onNodeSelect) this.onNodeSelect(node);
    }

    selectEdge(edge) {
        this.selectedEdge = edge;
        this.selectedNode = null;
        this.draw();
    }

    getGraphJson() {
        return {
            components: this.nodes.map(n => {
                const comp = { id: n.id, type: n.type };
                if (n.type === 'Database') comp.tables = n.tables || {};
                if (n.type === 'HttpServer') comp.routes = n.routes || [];
                return comp;
            }),
            connections: this.edges.map(e => ({ from: e.from, to: e.to, alias: e.alias })),
        };
    }

    loadGraph(graphJson) {
        this.nodes = [];
        this.edges = [];
        this.nextId = 1;
        this.panX = 0;
        this.panY = 0;
        this.zoom = 1;
        this.selectedNode = null;
        this.selectedEdge = null;

        if (graphJson.components) {
            graphJson.components.forEach((comp, i) => {
                this.nodes.push({
                    id: comp.id, type: comp.type,
                    x: comp.x ?? (80 + i * 220), y: comp.y ?? 120,
                    width: NODE_WIDTH, height: NODE_HEIGHT,
                    tables: comp.tables || {},
                    routes: comp.routes || [],
                });
                this.nextId++;
            });
        }
        if (graphJson.connections) {
            graphJson.connections.forEach(c => {
                this.edges.push({ from: c.from, to: c.to, alias: c.alias });
            });
        }
        this.draw();
    }

    // --- Drawing ---

    draw() {
        const ctx = this.ctx;
        const dpr = this.dpr || 1;
        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        ctx.save();
        ctx.scale(dpr, dpr);
        ctx.translate(this.panX, this.panY);
        ctx.scale(this.zoom, this.zoom);

        this._drawGrid(ctx);
        this.edges.forEach(e => this._drawEdge(ctx, e));
        if (this.connectingFrom) this._drawConnectionLine(ctx);
        this.nodes.forEach(n => this._drawNode(ctx, n));

        ctx.restore();
    }

    _drawGrid(ctx) {
        const gs = 40;
        const dpr = this.dpr || 1;
        const cssW = this.canvas.width / dpr;
        const cssH = this.canvas.height / dpr;
        const sx = Math.floor(-this.panX / this.zoom / gs) * gs - gs;
        const sy = Math.floor(-this.panY / this.zoom / gs) * gs - gs;
        const ex = sx + cssW / this.zoom + gs * 2;
        const ey = sy + cssH / this.zoom + gs * 2;
        ctx.strokeStyle = '#1a1f27';
        ctx.lineWidth = 0.5;
        for (let x = sx; x < ex; x += gs) { ctx.beginPath(); ctx.moveTo(x, sy); ctx.lineTo(x, ey); ctx.stroke(); }
        for (let y = sy; y < ey; y += gs) { ctx.beginPath(); ctx.moveTo(sx, y); ctx.lineTo(ex, y); ctx.stroke(); }
    }

    _drawEdge(ctx, edge) {
        const from = this.nodes.find(n => n.id === edge.from);
        const to = this.nodes.find(n => n.id === edge.to);
        if (!from || !to) return;

        const pts = this._edgePoints(from, to);
        const selected = this.selectedEdge === edge;

        ctx.strokeStyle = selected ? '#58a6ff' : '#484f58';
        ctx.lineWidth = selected ? 3 : 2;
        ctx.beginPath();
        ctx.moveTo(pts.fx, pts.fy);
        ctx.bezierCurveTo(pts.fx + pts.cpx, pts.fy, pts.tx - pts.cpx, pts.ty, pts.tx, pts.ty);
        ctx.stroke();

        // Arrow
        ctx.fillStyle = ctx.strokeStyle;
        ctx.beginPath();
        ctx.moveTo(pts.tx - 2, pts.ty);
        ctx.lineTo(pts.tx - 10, pts.ty - 5);
        ctx.lineTo(pts.tx - 10, pts.ty + 5);
        ctx.fill();

        // Label
        ctx.fillStyle = selected ? '#58a6ff' : '#6e7681';
        ctx.font = '11px -apple-system, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(edge.alias, (pts.fx + pts.tx) / 2, Math.min(pts.fy, pts.ty) - 10);
    }

    _edgePoints(from, to) {
        const fx = from.x + from.width;
        const fy = from.y + from.height / 2;
        const tx = to.x;
        const ty = to.y + to.height / 2;
        return { fx, fy, tx, ty, cpx: Math.max(Math.abs(tx - fx) * 0.5, 40) };
    }

    _drawConnectionLine(ctx) {
        const from = this.connectingFrom.node;
        const mp = this._screenToWorld(this.mousePos.x, this.mousePos.y);
        ctx.strokeStyle = '#58a6ff';
        ctx.lineWidth = 2;
        ctx.setLineDash([6, 4]);
        ctx.beginPath();
        ctx.moveTo(from.x + from.width, from.y + from.height / 2);
        ctx.lineTo(mp.x, mp.y);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    _drawNode(ctx, node) {
        const color = COMPONENT_COLORS[node.type] || '#8b949e';
        const selected = this.selectedNode === node;

        ctx.shadowColor = 'rgba(0,0,0,0.4)';
        ctx.shadowBlur = selected ? 16 : 8;
        ctx.shadowOffsetY = 3;

        ctx.fillStyle = '#161b22';
        ctx.strokeStyle = selected ? color : '#30363d';
        ctx.lineWidth = selected ? 2.5 : 1.5;
        this._roundRect(ctx, node.x, node.y, node.width, node.height, 10);
        ctx.fill();
        ctx.stroke();

        ctx.shadowColor = 'transparent'; ctx.shadowBlur = 0; ctx.shadowOffsetY = 0;

        // Header tint
        ctx.fillStyle = color + '18';
        this._roundRectTop(ctx, node.x + 1, node.y + 1, node.width - 2, 30, 10);
        ctx.fill();

        // Separator
        ctx.strokeStyle = '#30363d'; ctx.lineWidth = 0.5;
        ctx.beginPath(); ctx.moveTo(node.x + 8, node.y + 31); ctx.lineTo(node.x + node.width - 8, node.y + 31); ctx.stroke();

        // Type label
        ctx.fillStyle = color;
        ctx.font = 'bold 12px -apple-system, sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText(node.type, node.x + 12, node.y + 21);

        // ID
        ctx.fillStyle = '#8b949e';
        ctx.font = '11px -apple-system, sans-serif';
        ctx.fillText(node.id, node.x + 12, node.y + 50);

        // HttpServer: show route count or warning
        if (node.type === 'HttpServer') {
            const routeCount = node.routes ? node.routes.length : 0;
            ctx.textAlign = 'right';
            if (routeCount === 0) {
                ctx.fillStyle = '#f85149';
                ctx.font = '9px -apple-system, sans-serif';
                ctx.fillText('⚠ no routes', node.x + node.width - 10, node.y + 50);
            } else {
                ctx.fillStyle = '#8b949e';
                ctx.font = '9px -apple-system, sans-serif';
                ctx.fillText(routeCount + ' route' + (routeCount > 1 ? 's' : ''), node.x + node.width - 10, node.y + 50);
            }
        } else if (CODABLE_TYPES.has(node.type)) {
            ctx.fillStyle = '#484f58';
            ctx.font = '9px -apple-system, sans-serif';
            ctx.textAlign = 'right';
            ctx.fillText('{ }', node.x + node.width - 10, node.y + 50);
        }

        // Database: show table count or warning
        if (node.type === 'Database') {
            const tableCount = node.tables ? Object.keys(node.tables).length : 0;
            ctx.textAlign = 'right';
            if (tableCount === 0) {
                // Warning: no schema defined
                ctx.fillStyle = '#f85149';
                ctx.font = '9px -apple-system, sans-serif';
                ctx.fillText('⚠ no schema', node.x + node.width - 10, node.y + 50);
            } else {
                ctx.fillStyle = '#8b949e';
                ctx.font = '9px -apple-system, sans-serif';
                const names = Object.keys(node.tables).join(', ');
                const display = names.length > 16 ? names.slice(0, 14) + '…' : names;
                ctx.fillText(display, node.x + node.width - 10, node.y + 50);
            }
        }

        // Ports
        this._drawPort(ctx, node.x + node.width, node.y + node.height / 2, color, selected);
        this._drawPort(ctx, node.x, node.y + node.height / 2, color, selected);
    }

    _drawPort(ctx, x, y, color, active) {
        ctx.fillStyle = active ? color : '#30363d';
        ctx.beginPath(); ctx.arc(x, y, PORT_RADIUS, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = '#0d1117'; ctx.lineWidth = 2; ctx.stroke();
        ctx.fillStyle = '#0d1117';
        ctx.beginPath(); ctx.arc(x, y, 2.5, 0, Math.PI * 2); ctx.fill();
    }

    // --- Events ---

    _setupEvents() {
        this.canvas.addEventListener('mousedown', e => this._onMouseDown(e));
        this.canvas.addEventListener('mousemove', e => this._onMouseMove(e));
        this.canvas.addEventListener('mouseup', e => this._onMouseUp(e));
        this.canvas.addEventListener('dblclick', e => this._onDblClick(e));
        this.canvas.addEventListener('wheel', e => this._onWheel(e), { passive: false });
        this.canvas.addEventListener('contextmenu', e => {
            e.preventDefault();
            this._onRightClick(e);
        });
        document.addEventListener('mousedown', e => {
            if (!this.contextMenu.contains(e.target)) this._hideContextMenu();
        });
        document.addEventListener('keydown', e => {
            if (e.key === 'Delete' || e.key === 'Backspace') {
                const a = document.activeElement;
                if (a === this.canvas || a === document.body) {
                    e.preventDefault();
                    if (this.selectedEdge) this.removeEdge(this.selectedEdge);
                    else if (this.selectedNode) this.removeNode(this.selectedNode);
                }
            }
            if (e.key === 'Escape') {
                this.connectingFrom = null;
                this._hideContextMenu();
                this.draw();
            }
        });
    }

    _getMousePos(e) {
        const r = this.canvas.getBoundingClientRect();
        return { x: e.clientX - r.left, y: e.clientY - r.top };
    }

    _screenToWorld(sx, sy) {
        return { x: (sx - this.panX) / this.zoom, y: (sy - this.panY) / this.zoom };
    }

    _hitNode(wp) {
        for (let i = this.nodes.length - 1; i >= 0; i--) {
            const n = this.nodes[i];
            if (wp.x >= n.x && wp.x <= n.x + n.width && wp.y >= n.y && wp.y <= n.y + n.height) return n;
        }
        return null;
    }

    _hitPort(wp, side) {
        for (const n of this.nodes) {
            const px = side === 'output' ? n.x + n.width : n.x;
            const py = n.y + n.height / 2;
            const dx = wp.x - px, dy = wp.y - py;
            if (dx * dx + dy * dy <= (PORT_RADIUS + 6) ** 2) return n;
        }
        return null;
    }

    _hitEdge(wp) {
        for (const edge of this.edges) {
            const from = this.nodes.find(n => n.id === edge.from);
            const to = this.nodes.find(n => n.id === edge.to);
            if (!from || !to) continue;
            const pts = this._edgePoints(from, to);
            if (this._pointNearBezier(wp.x, wp.y, pts, EDGE_HIT_TOLERANCE)) return edge;
        }
        return null;
    }

    _pointNearBezier(px, py, pts, tol) {
        // Sample the bezier and check distance to each segment
        const steps = 20;
        for (let i = 0; i <= steps; i++) {
            const t = i / steps;
            const bx = this._bezierPoint(t, pts.fx, pts.fx + pts.cpx, pts.tx - pts.cpx, pts.tx);
            const by = this._bezierPoint(t, pts.fy, pts.fy, pts.ty, pts.ty);
            const dx = px - bx, dy = py - by;
            if (dx * dx + dy * dy <= tol * tol) return true;
        }
        return false;
    }

    _bezierPoint(t, p0, p1, p2, p3) {
        const mt = 1 - t;
        return mt * mt * mt * p0 + 3 * mt * mt * t * p1 + 3 * mt * t * t * p2 + t * t * t * p3;
    }

    _onMouseDown(e) {
        if (e.button === 2) return; // right click handled by contextmenu
        this._hideContextMenu();
        const sp = this._getMousePos(e);
        const wp = this._screenToWorld(sp.x, sp.y);
        this.canvas.focus();

        // Middle mouse or alt+click -> pan
        if (e.button === 1 || (e.button === 0 && e.altKey)) {
            this.isPanning = true;
            this.panStart = { x: sp.x - this.panX, y: sp.y - this.panY };
            this.canvas.style.cursor = 'grabbing';
            return;
        }

        // Output port -> connect
        const portNode = this._hitPort(wp, 'output');
        if (portNode) {
            this.connectingFrom = { node: portNode };
            this.canvas.style.cursor = 'crosshair';
            return;
        }

        // Node -> select + drag
        const node = this._hitNode(wp);
        if (node) {
            this.selectNode(node);
            this.dragNode = node;
            this.dragOffset = { x: wp.x - node.x, y: wp.y - node.y };
            this.canvas.style.cursor = 'move';
            return;
        }

        // Edge -> select
        const edge = this._hitEdge(wp);
        if (edge) {
            this.selectEdge(edge);
            return;
        }

        // Empty -> deselect + pan
        this.selectedNode = null;
        this.selectedEdge = null;
        this.draw();
        if (this.onNodeSelect) this.onNodeSelect(null);
        this.isPanning = true;
        this.panStart = { x: sp.x - this.panX, y: sp.y - this.panY };
        this.canvas.style.cursor = 'grabbing';
    }

    _onMouseMove(e) {
        const sp = this._getMousePos(e);
        this.mousePos = sp;

        if (this.isPanning) {
            this.panX = sp.x - this.panStart.x;
            this.panY = sp.y - this.panStart.y;
            this.draw();
            return;
        }
        if (this.connectingFrom) { this.draw(); return; }
        if (this.dragNode) {
            const wp = this._screenToWorld(sp.x, sp.y);
            this.dragNode.x = wp.x - this.dragOffset.x;
            this.dragNode.y = wp.y - this.dragOffset.y;
            this.draw();
            return;
        }

        // Hover cursor
        const wp = this._screenToWorld(sp.x, sp.y);
        if (this._hitPort(wp, 'output')) this.canvas.style.cursor = 'crosshair';
        else if (this._hitNode(wp)) this.canvas.style.cursor = 'pointer';
        else if (this._hitEdge(wp)) this.canvas.style.cursor = 'pointer';
        else this.canvas.style.cursor = 'grab';
    }

    _onMouseUp(e) {
        if (this.connectingFrom) {
            const sp = this._getMousePos(e);
            const wp = this._screenToWorld(sp.x, sp.y);
            const target = this._hitPort(wp, 'input') || this._hitNode(wp);
            if (target && target !== this.connectingFrom.node) {
                this.addEdge(this.connectingFrom.node.id, target.id, target.id.split('_')[0]);
            }
            this.connectingFrom = null;
            this.draw();
        }
        this.isPanning = false;
        this.dragNode = null;
        this.canvas.style.cursor = 'grab';
    }

    _onDblClick(e) {
        const sp = this._getMousePos(e);
        const wp = this._screenToWorld(sp.x, sp.y);
        const node = this._hitNode(wp);
        if (node) {
            if (CODABLE_TYPES.has(node.type) && this.onEditCode) {
                this.onEditCode(node);
            } else if (node.type === 'Database') {
                this.openSchemaEditor(node);
            } else if (this.onNodeDoubleClick) {
                this.onNodeDoubleClick(node);
            }
        }
    }

    _onWheel(e) {
        e.preventDefault();
        const sp = this._getMousePos(e);
        const wBefore = this._screenToWorld(sp.x, sp.y);
        this.zoom = Math.max(0.3, Math.min(3, this.zoom * (e.deltaY < 0 ? 1.1 : 0.9)));
        const wAfter = this._screenToWorld(sp.x, sp.y);
        this.panX += (wAfter.x - wBefore.x) * this.zoom;
        this.panY += (wAfter.y - wBefore.y) * this.zoom;
        this.draw();
    }

    // --- Right-click Context Menu ---

    _onRightClick(e) {
        const sp = this._getMousePos(e);
        const wp = this._screenToWorld(sp.x, sp.y);

        const node = this._hitNode(wp);
        const edge = this._hitEdge(wp);

        const items = [];

        if (node) {
            this.selectNode(node);
            if (node.type === 'HttpServer') {
                items.push({ label: 'Edit Routes', icon: 'fa-route', action: () => this.openRouteEditor(node) });
                items.push({ label: 'Edit Code', icon: 'fa-code', action: () => {
                    if (this.onEditCode) this.onEditCode(node);
                }});
                items.push({ separator: true });
            } else if (CODABLE_TYPES.has(node.type)) {
                items.push({ label: 'Edit Code', icon: 'fa-code', action: () => {
                    if (this.onEditCode) this.onEditCode(node);
                }});
                items.push({ separator: true });
            }
            if (node.type === 'Database') {
                items.push({ label: 'Edit Schema', icon: 'fa-table', action: () => this.openSchemaEditor(node) });
                items.push({ separator: true });
            }
            items.push({ label: 'Disconnect All', icon: 'fa-unlink', action: () => this.disconnectNode(node) });
            items.push({ label: 'Delete Component', icon: 'fa-trash', danger: true, action: () => this.removeNode(node) });
        } else if (edge) {
            this.selectEdge(edge);
            const fromNode = this.nodes.find(n => n.id === edge.from);
            const toNode = this.nodes.find(n => n.id === edge.to);
            const label = `${fromNode?.id || '?'} → ${toNode?.id || '?'}`;
            items.push({ label: label, icon: 'fa-arrow-right', disabled: true });
            items.push({ separator: true });
            items.push({ label: 'Delete Edge', icon: 'fa-trash', danger: true, action: () => this.removeEdge(edge) });
        } else {
            items.push({ label: 'Add Client', icon: 'fa-user', action: () => this.addNode('Client', wp.x, wp.y) });
            items.push({ label: 'Add HTTP Server', icon: 'fa-server', action: () => this.addNode('HttpServer', wp.x, wp.y) });
            items.push({ label: 'Add Database', icon: 'fa-database', action: () => this.addNode('Database', wp.x, wp.y) });
            items.push({ label: 'Add Cache', icon: 'fa-bolt', action: () => this.addNode('Cache', wp.x, wp.y) });
            items.push({ label: 'Add Load Balancer', icon: 'fa-balance-scale', action: () => this.addNode('LoadBalancer', wp.x, wp.y) });
        }

        this._showContextMenu(e.clientX, e.clientY, items);
    }

    _createContextMenu() {
        const menu = document.createElement('div');
        menu.className = 'graph-context-menu';
        menu.style.cssText = `
            display: none; position: fixed; z-index: 1000;
            background: #1c2128; border: 1px solid #30363d; border-radius: 8px;
            padding: 4px 0; min-width: 180px; box-shadow: 0 8px 24px rgba(0,0,0,0.4);
            font-family: -apple-system, sans-serif; font-size: 13px;
        `;
        document.body.appendChild(menu);
        return menu;
    }

    _showContextMenu(x, y, items) {
        const menu = this.contextMenu;
        menu.innerHTML = '';

        for (const item of items) {
            if (item.separator) {
                const sep = document.createElement('div');
                sep.style.cssText = 'height:1px; background:#30363d; margin:4px 8px;';
                menu.appendChild(sep);
                continue;
            }
            const row = document.createElement('div');
            row.style.cssText = `
                display: flex; align-items: center; gap: 10px;
                padding: 6px 14px; cursor: ${item.disabled ? 'default' : 'pointer'};
                color: ${item.danger ? '#f85149' : item.disabled ? '#484f58' : '#e6edf3'};
                transition: background 0.1s;
            `;
            if (!item.disabled) {
                row.addEventListener('mouseenter', () => row.style.background = '#30363d');
                row.addEventListener('mouseleave', () => row.style.background = 'none');
                row.addEventListener('click', () => { this._hideContextMenu(); item.action(); });
            }
            row.innerHTML = `<i class="fas ${item.icon}" style="width:16px;text-align:center;font-size:12px"></i><span>${item.label}</span>`;
            menu.appendChild(row);
        }

        // Position (keep on screen)
        menu.style.display = 'block';
        const mw = menu.offsetWidth, mh = menu.offsetHeight;
        menu.style.left = Math.min(x, window.innerWidth - mw - 8) + 'px';
        menu.style.top = Math.min(y, window.innerHeight - mh - 8) + 'px';
    }

    _hideContextMenu() {
        this.contextMenu.style.display = 'none';
    }

    // --- Resize ---

    _resize() {
        const parent = this.canvas.parentElement;
        const header = parent.querySelector('.panel-header');
        const headerH = header ? header.offsetHeight : 0;
        const dpr = window.devicePixelRatio || 1;
        const w = parent.clientWidth;
        const h = parent.clientHeight - headerH;

        // Set the canvas internal size to account for HiDPI
        this.canvas.width = w * dpr;
        this.canvas.height = h * dpr;
        this.canvas.style.width = w + 'px';
        this.canvas.style.height = h + 'px';

        // Scale the context so drawing coordinates match CSS pixels
        this.dpr = dpr;
        this.draw();
    }

    // --- Geometry helpers ---

    _roundRect(ctx, x, y, w, h, r) {
        ctx.beginPath();
        ctx.moveTo(x + r, y);
        ctx.arcTo(x + w, y, x + w, y + h, r);
        ctx.arcTo(x + w, y + h, x, y + h, r);
        ctx.arcTo(x, y + h, x, y, r);
        ctx.arcTo(x, y, x + w, y, r);
        ctx.closePath();
    }

    _roundRectTop(ctx, x, y, w, h, r) {
        ctx.beginPath();
        ctx.moveTo(x + r, y);
        ctx.arcTo(x + w, y, x + w, y + h, r);
        ctx.lineTo(x + w, y + h);
        ctx.lineTo(x, y + h);
        ctx.arcTo(x, y, x + w, y, r);
        ctx.closePath();
    }

    // --- Schema Editor Dialog ---

    openSchemaEditor(node) {
        if (node.type !== 'Database') return;

        // Initialize tables if not present
        if (!node.tables) node.tables = {};

        // Remove existing dialog if any
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
                    ${Object.entries(node.tables).map(([tableName, columns]) => this._renderTableEditor(tableName, columns)).join('')}
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

            // Wire up events
            dialog.querySelector('#schema-close').onclick = () => modal.remove();
            dialog.querySelector('#schema-cancel').onclick = () => modal.remove();
            dialog.querySelector('#schema-add-table').onclick = () => {
                const name = 'table_' + (Object.keys(node.tables).length + 1);
                node.tables[name] = [{ name: 'id', type: 'int' }];
                renderDialog();
            };

            dialog.querySelector('#schema-save').onclick = () => {
                // Validate
                let hasError = false;
                const nodeIdInput = dialog.querySelector('#schema-node-id');
                const newId = nodeIdInput.value.trim();
                if (!newId) {
                    nodeIdInput.style.borderColor = '#f85149';
                    hasError = true;
                } else {
                    nodeIdInput.style.borderColor = '#30363d';
                }

                // Validate table names and columns
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

                if (hasError) return;  // Don't save with errors

                // Update node ID
                if (newId !== node.id) {
                    this.edges.forEach(e => {
                        if (e.from === node.id) e.from = newId;
                        if (e.to === node.id) { e.to = newId; e.alias = newId; }
                    });
                    node.id = newId;
                }

                // Read tables from the form
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
                this.draw();
                modal.remove();
            };

            // Wire delete table buttons
            dialog.querySelectorAll('.schema-delete-table').forEach(btn => {
                btn.onclick = () => {
                    const tableName = btn.dataset.table;
                    delete node.tables[tableName];
                    renderDialog();
                };
            });

            // Wire add column buttons
            dialog.querySelectorAll('.schema-add-col').forEach(btn => {
                btn.onclick = () => {
                    const tableName = btn.dataset.table;
                    node.tables[tableName].push({ name: '', type: 'string' });
                    renderDialog();
                };
            });

            // Wire delete column buttons
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

        // Close on backdrop click
        modal.addEventListener('click', (e) => {
            if (e.target === modal) modal.remove();
        });

        // Close on Escape
        const onKey = (e) => {
            if (e.key === 'Escape') { modal.remove(); document.removeEventListener('keydown', onKey); }
        };
        document.addEventListener('keydown', onKey);
    }

    _renderTableEditor(tableName, columns) {
        // Normalize columns — support both ["col"] and [{name, type}] formats
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

    // --- Route Editor Dialog ---

    openRouteEditor(node) {
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
                this.draw();
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
        const onKey = (e) => { if (e.key === 'Escape') { modal.remove(); document.removeEventListener('keydown', onKey); } };
        document.addEventListener('keydown', onKey);
    }
}
