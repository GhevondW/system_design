/**
 * GraphEditor — Canvas-based visual graph editor for system components.
 */

const NODE_WIDTH = 160;
const NODE_HEIGHT = 70;
const PORT_RADIUS = 6;
const COMPONENT_COLORS = {
    Client: '#58a6ff',
    HttpServer: '#3fb950',
    Database: '#d29922',
    Cache: '#bc8cff',
};
const COMPONENT_ICONS = {
    Client: '\uf007',
    HttpServer: '\uf233',
    Database: '\uf1c0',
    Cache: '\uf0e7',
};

export class GraphEditor {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.nodes = [];
        this.edges = [];
        this.selectedNode = null;
        this.dragNode = null;
        this.dragOffset = { x: 0, y: 0 };
        this.connectingFrom = null;
        this.onNodeSelect = null;
        this.onNodeDoubleClick = null;
        this.nextId = 1;

        this._setupEvents();
        this._resize();
        window.addEventListener('resize', () => this._resize());
    }

    addNode(type, x, y) {
        const id = type.toLowerCase() + '_' + this.nextId++;
        const node = {
            id,
            type,
            x: x || 50 + this.nodes.length * 200,
            y: y || 100 + (this.nodes.length % 2) * 120,
            width: NODE_WIDTH,
            height: NODE_HEIGHT,
            code: '',
        };
        this.nodes.push(node);
        this.draw();
        return node;
    }

    addEdge(fromId, toId, alias) {
        if (fromId === toId) return;
        const exists = this.edges.find(e => e.from === fromId && e.to === toId);
        if (exists) return;
        this.edges.push({ from: fromId, to: toId, alias: alias || toId.split('_')[0] });
        this.draw();
    }

    removeSelected() {
        if (!this.selectedNode) return;
        const id = this.selectedNode.id;
        this.nodes = this.nodes.filter(n => n.id !== id);
        this.edges = this.edges.filter(e => e.from !== id && e.to !== id);
        this.selectedNode = null;
        this.draw();
    }

    getGraphJson() {
        return {
            components: this.nodes.map(n => {
                const comp = { id: n.id, type: n.type };
                if (n.type === 'Database') {
                    comp.tables = {};
                }
                return comp;
            }),
            connections: this.edges.map(e => ({
                from: e.from,
                to: e.to,
                alias: e.alias,
            })),
        };
    }

    loadGraph(graphJson) {
        this.nodes = [];
        this.edges = [];
        this.nextId = 1;

        if (graphJson.components) {
            graphJson.components.forEach((comp, i) => {
                this.nodes.push({
                    id: comp.id,
                    type: comp.type,
                    x: comp.x || 50 + i * 200,
                    y: comp.y || 150,
                    width: NODE_WIDTH,
                    height: NODE_HEIGHT,
                    code: comp.code || '',
                    tables: comp.tables || {},
                });
                this.nextId++;
            });
        }

        if (graphJson.connections) {
            graphJson.connections.forEach(conn => {
                this.edges.push({
                    from: conn.from,
                    to: conn.to,
                    alias: conn.alias,
                });
            });
        }

        this.draw();
    }

    selectNode(node) {
        this.selectedNode = node;
        this.draw();
        if (this.onNodeSelect) this.onNodeSelect(node);
    }

    draw() {
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.clearRect(0, 0, w, h);

        // Draw grid
        ctx.strokeStyle = '#1a1f27';
        ctx.lineWidth = 1;
        for (let x = 0; x < w; x += 30) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, h);
            ctx.stroke();
        }
        for (let y = 0; y < h; y += 30) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        // Draw edges
        this.edges.forEach(edge => {
            const from = this.nodes.find(n => n.id === edge.from);
            const to = this.nodes.find(n => n.id === edge.to);
            if (!from || !to) return;

            const fx = from.x + from.width;
            const fy = from.y + from.height / 2;
            const tx = to.x;
            const ty = to.y + to.height / 2;

            ctx.strokeStyle = '#30363d';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(fx, fy);
            const cp = (tx - fx) / 2;
            ctx.bezierCurveTo(fx + cp, fy, tx - cp, ty, tx, ty);
            ctx.stroke();

            // Arrow
            const angle = Math.atan2(ty - fy, tx - fx);
            ctx.fillStyle = '#30363d';
            ctx.beginPath();
            ctx.moveTo(tx, ty);
            ctx.lineTo(tx - 8 * Math.cos(angle - 0.4), ty - 8 * Math.sin(angle - 0.4));
            ctx.lineTo(tx - 8 * Math.cos(angle + 0.4), ty - 8 * Math.sin(angle + 0.4));
            ctx.fill();

            // Label
            const mx = (fx + tx) / 2;
            const my = (fy + ty) / 2 - 8;
            ctx.fillStyle = '#484f58';
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText(edge.alias, mx, my);
        });

        // Draw connection line while dragging
        if (this.connectingFrom) {
            const from = this.connectingFrom.node;
            ctx.strokeStyle = '#58a6ff';
            ctx.lineWidth = 2;
            ctx.setLineDash([5, 3]);
            ctx.beginPath();
            ctx.moveTo(from.x + from.width, from.y + from.height / 2);
            ctx.lineTo(this.connectingFrom.mx, this.connectingFrom.my);
            ctx.stroke();
            ctx.setLineDash([]);
        }

        // Draw nodes
        this.nodes.forEach(node => {
            const color = COMPONENT_COLORS[node.type] || '#8b949e';
            const selected = this.selectedNode === node;

            // Shadow
            ctx.shadowColor = 'rgba(0,0,0,0.3)';
            ctx.shadowBlur = selected ? 12 : 6;
            ctx.shadowOffsetY = 2;

            // Body
            ctx.fillStyle = '#161b22';
            ctx.strokeStyle = selected ? color : '#30363d';
            ctx.lineWidth = selected ? 2 : 1;
            this._roundRect(ctx, node.x, node.y, node.width, node.height, 8);
            ctx.fill();
            ctx.stroke();

            ctx.shadowColor = 'transparent';

            // Header bar
            ctx.fillStyle = color + '20';
            this._roundRectTop(ctx, node.x, node.y, node.width, 28, 8);
            ctx.fill();

            // Type label
            ctx.fillStyle = color;
            ctx.font = 'bold 11px sans-serif';
            ctx.textAlign = 'left';
            ctx.fillText(node.type, node.x + 10, node.y + 18);

            // ID label
            ctx.fillStyle = '#8b949e';
            ctx.font = '10px sans-serif';
            ctx.fillText(node.id, node.x + 10, node.y + 48);

            // Output port (right side)
            ctx.fillStyle = selected ? color : '#30363d';
            ctx.beginPath();
            ctx.arc(node.x + node.width, node.y + node.height / 2, PORT_RADIUS, 0, Math.PI * 2);
            ctx.fill();
            ctx.strokeStyle = '#161b22';
            ctx.lineWidth = 2;
            ctx.stroke();

            // Input port (left side)
            ctx.fillStyle = selected ? color : '#30363d';
            ctx.beginPath();
            ctx.arc(node.x, node.y + node.height / 2, PORT_RADIUS, 0, Math.PI * 2);
            ctx.fill();
            ctx.strokeStyle = '#161b22';
            ctx.lineWidth = 2;
            ctx.stroke();
        });
    }

    // --- Private ---

    _setupEvents() {
        this.canvas.addEventListener('mousedown', e => this._onMouseDown(e));
        this.canvas.addEventListener('mousemove', e => this._onMouseMove(e));
        this.canvas.addEventListener('mouseup', e => this._onMouseUp(e));
        this.canvas.addEventListener('dblclick', e => this._onDblClick(e));
        document.addEventListener('keydown', e => {
            if (e.key === 'Delete' || e.key === 'Backspace') {
                if (this.selectedNode && document.activeElement === this.canvas) {
                    this.removeSelected();
                }
            }
        });
    }

    _getMousePos(e) {
        const rect = this.canvas.getBoundingClientRect();
        const scaleX = this.canvas.width / rect.width;
        const scaleY = this.canvas.height / rect.height;
        return {
            x: (e.clientX - rect.left) * scaleX,
            y: (e.clientY - rect.top) * scaleY,
        };
    }

    _hitTest(pos) {
        for (let i = this.nodes.length - 1; i >= 0; i--) {
            const n = this.nodes[i];
            if (pos.x >= n.x && pos.x <= n.x + n.width &&
                pos.y >= n.y && pos.y <= n.y + n.height) {
                return n;
            }
        }
        return null;
    }

    _hitOutputPort(pos) {
        for (const n of this.nodes) {
            const px = n.x + n.width;
            const py = n.y + n.height / 2;
            const dx = pos.x - px;
            const dy = pos.y - py;
            if (dx * dx + dy * dy <= (PORT_RADIUS + 4) * (PORT_RADIUS + 4)) {
                return n;
            }
        }
        return null;
    }

    _hitInputPort(pos) {
        for (const n of this.nodes) {
            const px = n.x;
            const py = n.y + n.height / 2;
            const dx = pos.x - px;
            const dy = pos.y - py;
            if (dx * dx + dy * dy <= (PORT_RADIUS + 4) * (PORT_RADIUS + 4)) {
                return n;
            }
        }
        return null;
    }

    _onMouseDown(e) {
        const pos = this._getMousePos(e);
        this.canvas.focus();

        // Check output port click for edge creation
        const portNode = this._hitOutputPort(pos);
        if (portNode) {
            this.connectingFrom = { node: portNode, mx: pos.x, my: pos.y };
            return;
        }

        const node = this._hitTest(pos);
        if (node) {
            this.selectNode(node);
            this.dragNode = node;
            this.dragOffset = { x: pos.x - node.x, y: pos.y - node.y };
        } else {
            this.selectedNode = null;
            this.draw();
        }
    }

    _onMouseMove(e) {
        const pos = this._getMousePos(e);

        if (this.connectingFrom) {
            this.connectingFrom.mx = pos.x;
            this.connectingFrom.my = pos.y;
            this.draw();
            return;
        }

        if (this.dragNode) {
            this.dragNode.x = pos.x - this.dragOffset.x;
            this.dragNode.y = pos.y - this.dragOffset.y;
            this.draw();
        }
    }

    _onMouseUp(e) {
        const pos = this._getMousePos(e);

        if (this.connectingFrom) {
            const target = this._hitInputPort(pos) || this._hitTest(pos);
            if (target && target !== this.connectingFrom.node) {
                const alias = target.type.toLowerCase().replace('httpserver', 'server');
                this.addEdge(this.connectingFrom.node.id, target.id, alias);
            }
            this.connectingFrom = null;
            this.draw();
        }

        this.dragNode = null;
    }

    _onDblClick(e) {
        const pos = this._getMousePos(e);
        const node = this._hitTest(pos);
        if (node && this.onNodeDoubleClick) {
            this.onNodeDoubleClick(node);
        }
    }

    _resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        const headerHeight = this.canvas.parentElement.querySelector('.panel-header')?.offsetHeight || 36;
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = (rect.height - headerHeight) * window.devicePixelRatio;
        this.canvas.style.width = rect.width + 'px';
        this.canvas.style.height = (rect.height - headerHeight) + 'px';
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.draw();
    }

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
}
