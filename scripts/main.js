const play = document.getElementById('play');
const stop = document.getElementById('stop');
const framePerSec = document.getElementById('frame-per-second');
const commandLineBar = document.getElementById('command-line-input');
const inputBar = document.getElementById('input-bar');
const errorBar = document.getElementById('error-bar');
const progress = document.getElementById('progress');
const timestamp = document.getElementById('timestamp');
const fullviewport = document.getElementById('fullviewport');
const screenviewport = document.getElementById('viewport');
const fitScreen = document.getElementById('fit-screen');
const reset = document.getElementById('reset');
const scaleUp = document.getElementById('scale-up');
const scaleDown = document.getElementById('scale-down');
const moveLeft = document.getElementById('move-left');
const moveRight = document.getElementById('move-right');
const moveUp = document.getElementById('move-up');
const moveDown = document.getElementById('move-down');
const cursorCoordination = document.getElementById('cursor-coordination');


class AffineTransformation {
    constructor(a, b, c, d, tx, ty) {
        this.a = a || 1;
        this.b = b || 0;
        this.c = c || 0;
        this.d = d || 1;
        this.tx = tx || 0;
        this.ty = ty || 0;
    }

    static identity() { return new AffineTransformation(1, 0, 0, 1, 0, 0); }

    static rotate(angle) {
        return new AffineTransformation(
            Math.cos(angle), Math.sin(angle),
            -Math.sin(angle), Math.cos(angle),
            0, 0);
    }

    static translate(tx, ty) {
        return new AffineTransformation(1, 0, 0, 1, tx, ty);
    }

    static scale(sx, sy) {
        return new AffineTransformation(sx, 0, 0, sy, 0, 0);
    }

    concat(other) {
        const a = this.a * other.a + this.b * other.c;
        const b = this.a * other.b + this.b * other.d;
        const c = this.c * other.a + this.d * other.c;
        const d = this.c * other.b + this.d * other.d;
        const tx = this.a * other.tx + this.b * other.ty + this.tx;
        const ty = this.c * other.tx + this.d * other.ty + this.ty;

        return new AffineTransformation(a, b, c, d, tx, ty);
    }

    applyXY(point) {
        const x = this.a * point.x + this.b * point.y + this.tx;
        const y = this.c * point.x + this.d * point.y + this.ty;
        return { x, y };
    }

    revertXY(point) {
        const determinant = this.a * this.d - this.b * this.c;
        if (determinant === 0) {
            throw new Error("Transformation is not invertible.");
        }

        const invDet = 1 / determinant;
        const newA = this.d * invDet;
        const newB = -this.b * invDet;
        const newC = -this.c * invDet;
        const newD = this.a * invDet;
        const newTx = (this.c * this.ty - this.d * this.tx) * invDet;
        const newTy = (this.b * this.tx - this.a * this.ty) * invDet;

        const x = newA * point.x + newB * point.y + newTx;
        const y = newC * point.x + newD * point.y + newTy;

        return { x, y };
    }

    convertToDOMMatrix()
    {
        return new DOMMatrix([this.a, this.b, this.c, this.d, this.tx, this.ty]);
    }
}

class BoundingBox {
    constructor(point1, point2) {
        // Determine the coordinates of the top-left corner (minX, minY)
        // and the bottom-right corner (maxX, maxY)
        this.minX = Math.min(point1.x, point2.x);
        this.minY = Math.min(point1.y, point2.y);
        this.maxX = Math.max(point1.x, point2.x);
        this.maxY = Math.max(point1.y, point2.y);
    }

    // Method to merge another point into the bounding box and return a new bounding box
    mergePoint(point) {
        // Calculate new bounding box coordinates
        const minX = Math.min(this.minX, point.x);
        const minY = Math.min(this.minY, point.y);
        const maxX = Math.max(this.maxX, point.x);
        const maxY = Math.max(this.maxY, point.y);

        // Return a new BoundingBox object with the updated coordinates
        return new BoundingBox({ x: minX, y: minY }, { x: maxX, y: maxY });
    }

    mergeBox(box) {
        return this.mergePoint(box.getBL()).mergePoint(box.getTR());
    }

    move(vec) {
        return new BoundingBox({ x: minX + vec.x, y: minY + vec.y }, { x: maxX + vec.x, y: maxY + vec.y });
    }

    // Method to check if a point is inside the bounding box
    containsPoint(point) {
        return point.x >= this.minX && point.x <= this.maxX &&
            point.y >= this.minY && point.y <= this.maxY;
    }

    // Method to get the width of the bounding box
    getWidth() {
        return this.maxX - this.minX;
    }

    // Method to get the height of the bounding box
    getHeight() {
        return this.maxY - this.minY;
    }

    // Method to get the area of the bounding box
    getArea() {
        return this.getWidth() * this.getHeight();
    }

    getCenter() {
        return {x: (this.getBL().x + this.getTR().x) / 2, y: (this.getBL().y + this.getTR().y) / 2};
    }

    inflate(off) {
        return new BoundingBox(PointSub(this.getBL(), {x: off, y: off}), PointAdd(this.getTR(), {x: off, y: off}));
    }

    getBL() { return {x: this.minX, y: this.minY}; }

    getTR() { return {x: this.maxX, y: this.maxY}; }
}

function Box2boxTransformation(box1, box2)
{
    const s = Math.min(box2.getHeight() / box1.getHeight(), box2.getWidth() / box1.getWidth());
    const c1 = box1.getCenter();
    const c2 = box2.getCenter();
    return new AffineTransformation(1, 0, 0, 1, c2.x, c2.y)
        .concat(new AffineTransformation(s * 0.95, 0, 0, s * 0.95, 0, 0))
        .concat(new AffineTransformation(1, 0, 0, 1, -c1.x, -c1.y));
}

function invertColor(color) {
    // Convert named colors to hexadecimal
    const colors = {
        "black": "#000000",
        "white": "#ffffff",
        "red": "#ff0000",
        "lime": "#00ff00",
        "blue": "#0000ff",
        "yellow": "#ffff00",
        "cyan": "#00ffff",
        "magenta": "#ff00ff",
        "silver": "#c0c0c0",
        "gray": "#808080",
        "maroon": "#800000",
        "olive": "#808000",
        "green": "#008000",
        "purple": "#800080",
        "teal": "#008080",
        "navy": "#000080",
        // Add more named colors if needed
    };

    if (colors[color.toLowerCase()]) {
        color = colors[color.toLowerCase()];
    }

    // Convert hex format to RGB
    if (color[0] === "#") {
        color = color.slice(1); // Remove '#'
        if (color.length === 3) {
            color = color[0] + color[0] + color[1] + color[1] + color[2] + color[2]; // Convert 3-digit hex to 6-digit
        }
        const r = parseInt(color.substr(0, 2), 16);
        const g = parseInt(color.substr(2, 2), 16);
        const b = parseInt(color.substr(4, 2), 16);
        color = `rgb(${r},${g},${b})`; // Convert to RGB
    }

    // Invert RGB (and optionally RGBA)
    if (color.includes("rgba")) {
        let [r, g, b, a] = color.match(/\d+/g).map(Number);
        return `rgba(${255 - r},${255 - g},${255 - b},${a})`;
    } else if (color.includes("rgb")) {
        let [r, g, b] = color.match(/\d+/g).map(Number);
        return `rgb(${255 - r},${255 - g},${255 - b})`;
    }

    // If the format is not recognized, return the original
    return color;
}

function findLineSegmentIntersection(A, B, C, D) {
    // Calculate differences
    let diffBA = { x: B.x - A.x, y: B.y - A.y };
    let diffDC = { x: D.x - C.x, y: D.y - C.y };
    let diffCA = { x: C.x - A.x, y: C.y - A.y };

    // Determinant
    let det = diffBA.x * diffDC.y - diffBA.y * diffDC.x;
    // If determinant is zero, lines are parallel or coincident
    if (det === 0) {
        return null; // No intersection
    }

    let t = (diffCA.x * diffDC.y - diffCA.y * diffDC.x) / det;
    let u = (diffCA.x * diffBA.y - diffCA.y * diffBA.x) / det;

    // Check if 0 <= t <= 1 and 0 <= u <= 1
    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        // Intersection point
        return {
            x: A.x + t * diffBA.x,
            y: A.y + t * diffBA.y
        };
    }

    return null; // No intersection
}

// Tokenize function (handles comments)
function tokenize(inputString) {
    const noComments = inputString.replace(/;.*$/gm, '');
    const regex = /"[^"]*"|\(|\)|-?\d+\.\d+|-?\d+|\S+/g;
    return noComments.match(regex) || [];
}

// Parse tokens into shape objects
function parseTokens(tokens) {
    const shapes = [];
    let currentTokenIndex = 0;

    function nextToken() {
        return tokens[currentTokenIndex++];
    }

    function parseShape(type) {
        const shape = { type };
        while (currentTokenIndex < tokens.length && tokens[currentTokenIndex] !== ')') {
            nextToken();
            let key = nextToken();
            if (key === 'point' && (shape['type'] === 'line' || shape['type'] === 'cline')) {
                key = shape['point1'] ? 'point2' : 'point1';
            }
            if (key === 'point' || key === 'point1' || key === 'point2' || key === 'center') {
                const x = parseFloat(nextToken());
                const y = parseFloat(nextToken());
                const value = { x, y };
                if (shape.type === 'polygon') {
                    shape.points = shape.points || [];
                    shape.points.push(value);
                } else {
                    shape[key] = value;
                }
            } else if (key === 'radius' || key === 'width') {
                shape[key] = parseFloat(nextToken());
            } else if (key === 'color' || key === 'comment' || key === 'layer') {
                shape[key] = nextToken().replace(/"/g, '');
            }
            nextToken();
        }
        currentTokenIndex++; // Skip closing parenthesis
        return shape;
    }

    while (currentTokenIndex < tokens.length) {
        const token = nextToken();
        if (token === '(') {
            const k = nextToken(); // Skip 'scene' or shape type token, handled in parseShape
            if (k !== 'scene') {
                shapes.push(parseShape(k));
            }
        }
    }

    return shapes;
}

// Serialize shape object to Lisp-like string
function serializeShape(shape) {
    let serialized = `(${shape["type"]}`;
    if (shape["type"] === "polygon") {
        shape["points"].forEach(point => {
            serialized += ` (point ${point["x"]} ${point["y"]})`;
        });
    } else {
        if ("point1" in shape) {
            serialized += ` (point ${shape["point1"]["x"]} ${shape["point1"]["y"]})`;
            serialized += ` (point ${shape["point2"]["x"]} ${shape["point2"]["y"]})`;
        }
        if ("center" in shape) {
            serialized += ` (center ${shape["center"]["x"]} ${shape["center"]["y"]})`;
            serialized += ` (radius ${shape["radius"]})`;
        }
    }
    if ("width" in shape) {
        serialized += ` (width ${shape["width"]})`;
    }
    if ("color" in shape) {
        serialized += ` (color "${shape["color"]}")`;
    }
    serialized += ')';
    return serialized;
}

function serializeShapes(shapes) {
    let serialized = "(scene\n";
    shapes.forEach(shape => {
        serialized += "  " + serializeShape(shape) + "\n";
    });
    serialized += ")";
    return serialized;
}

function PointAdd(p1, p2)
{
    return {x: p1.x + p2.x, y: p1.y + p2.y };
}

function PointSub(p1, p2)
{
    return {x: p1.x - p2.x, y: p1.y - p2.y };
}

class DrawItem {
    static CreateCircle(center, radius) {
        let ans = new DrawItem("circle");
        ans.center = center;
        ans.radius = radius;
        return ans;
    }
    static CreateLine(ptA, ptB, width) {
        let ans = new DrawItem("line");
        ans.point1 = ptA;
        ans.point2 = ptB;
        ans.width = width;
        return ans;
    }
    static CreateCLine(ptA, ptB, width) {
        let ans = new DrawItem("cline");
        ans.point1 = ptA;
        ans.point2 = ptB;
        ans.width = width;
        return ans;
    }
    static CreatePolygon(points) {
        let ans = new DrawItem("polygon");
        ans.points = points;
        return ans;
    }

    constructor(type) {
        this.type = type;
    }

    setColor(color) {
        this.color = color;
    } 

    getBox() {
        if (this.type == "circle") {
            const r = this.radius;
            const c = this.center;
            return new BoundingBox(PointSub(c , {x: r, y: r}), PointAdd(c , {x: r, y: r}));
        } else if (this.type == "line") {
            return  new BoundingBox(this.point1, this.point2).inflate(this.width / 2);
        } else if (this.type == "cline") {
            return  new BoundingBox(this.point1, this.point2).inflate(this.width / 2);
        } else if (this.type == "polygon") {
            let box = null;
            for (let pt of this.points) {
                if (box == null) {
                    box = new BoundingBox(pt, pt);
                } else {
                    box = box.mergePoint(pt);
                }
            }
            return box;
        } else {
            return null;
        }
    }
}

function showError(msg)
{
    errorBar.classList.add("error-bar-show");
    errorBar.innerText = msg;
    setTimeout(() => errorBar.classList.remove("error-bar-show"), 2000);
}

function splitString(input)
{
  const regex = /[^\s"']+|"([^"]*)"|'([^']*)'/g;
  const result = [];
  let match;

  while ((match = regex.exec(input)) !== null) {
    if (match[1]) {
      result.push(match[1]);
    } else if (match[2]) {
      result.push(match[2]);
    } else {
      result.push(match[0]);
    }
  }

  return result;
}

const RTREE_ITEM_ID = Symbol("RTREE_ITEM_ID");
class Viewport
{
    constructor(canvasId)
    {
        this.m_viewportEl = document.getElementById(canvasId)
        /** @type HTMLCanvasElement */
        this.m_canvas = this.m_viewportEl.querySelector("canvas.drawing");
        /** @type HTMLCanvasElement */
        this.m_selectionBox = this.m_viewportEl.querySelector("canvas.selection-box");
        /** @type HTMLCanvasElement */
        this.m_selectedItemsCanvas = this.m_viewportEl.querySelector("canvas.selection");
        /** @type HTMLCanvasElement */
        this.m_coordinationBox = this.m_viewportEl.querySelector("canvas.coordination");
		this.m_transform = AffineTransformation.identity();
        this.m_objectList = [];
        this.m_objectRTree = rbush();
        this.m_viewportConfig = {
            "default_width": 1,
            "default_color": "rgba(99, 99, 99, 0.99)",
            "default_background": "#2c2929",
        };
        window.addEventListener("resize", () => this.fitCanvas());
        this.fitCanvas();
    }

    addDrawingObject(obj) {
        this.m_objectList.push(obj);
        if (obj.color == null) {
            obj.color = this.m_viewportConfig.default_color;
        }
        if ((obj.type == 'cline' || obj.type == 'line') && obj.width == null) {
            obj.width = this.m_viewportConfig.default_width;
        }
        const box = obj.getBox();
        const item = {
            minX: box.getBL().x,
            minY: box.getBL().y,
            maxX: box.getTR().x,
            maxY: box.getTR().y,
            object: obj
        };
        this.m_objectRTree.insert(item);
        obj[RTREE_ITEM_ID] = item;
    }

    removeDrawingObject(obj) {
        const idx = this.m_objectList.indexOf(obj);
        if (idx != -1) {
            this.m_objectList.splice(idx, 1);
            const item = obj[RTREE_ITEM_ID];
            if (item != null) {
                this.m_objectRTree.remove(item);
            }
        }
    }

    moveDrawingObject(obj) {
        const idx = this.m_objectList.indexOf(obj);
        if (idx != -1) {
            const item = obj[RTREE_ITEM_ID];
            if (item == null) {
                this.m_objectRTree.remove(item);
                const box = obj.getBox();
                const item = {
                    minX: box.getBL().x,
                    minY: box.getBL().y,
                    maxX: box.getTR().x,
                    maxY: box.getTR().y,
                    object: obj
                };
                this.m_objectRTree.insert(item);
                obj[RTREE_ITEM_ID] = item;
            }
        }
    }

    DrawCircle(center, radius, color) {
        let circle = DrawItem.CreateCircle(center, radius);
        circle.setColor(color);
        this.addDrawingObject(circle);
    }

    DrawLine(start, end, width, color) {
        let line = DrawItem.CreateLine(start, end, width);
        line.setColor(color);
        this.addDrawingObject(line);
    }

    DrawCLine(start, end, width, color) {
        let cline = DrawItem.CreateCLine(start, end, width);
        cline.setColor(color);
        this.addDrawingObject(cline);
    }

    DrawPolygon(points, color) {
        let polygon = DrawItem.CreatePolygon(points);
        polygon.setColor(color);
        this.addDrawingObject(polygon);
    }

    cmdZoom() {
        this.fitScreen();
    }

    cmdClear() {
        this.m_objectList = [];
        this.m_objectRTree.clear();
    }

    cmdSet(args) {
        const argv = splitString(args);
        if (argv.length == 0) {
            showError("set nothing");
            return;
        }

        if (argv[0] == 'color') {
            if (argv.length != 2) {
                showError("fail to set default color");
                return;
            }
            this.m_viewportConfig.default_color = argv[1];
        } else if (argv[0] == 'background') {
            if (argv.length != 2) {
                showError("fail to set default background");
                return;
            }
            this.m_viewportConfig.default_background = argv[1];
            this.m_viewportEl.style.background = argv[1];
        } else if (argv[0] == 'width') {
            if (argv.length != 2) {
                showError("fail to set default width");
                return;
            }
            this.m_viewportConfig.default_width = argv[1];
        } else {
            showError(`set nothing '${argv[0]}'`);
        }
    }

    cmdDraw(args) {
        let addn = 0;
        const kregex = /\s*[({]\s*(?:x\s*=\s*)?(-?\d+|-?\d+\.\d+)\s*,\s*(?:y\s*=\s*)?(-?\d+|-?\d+\.\d+)\s*[})]/g;
        const pts = [];
        let match;
        while ((match = kregex.exec(args)) !== null) {
            pts.push({ x: parseInt(match[1]), y: parseInt(match[2]) });
        }

        if (pts.length > 1) {
            for (let i=0;i+1<pts.length;i++) {
                const drawItem = new DrawItem("cline")
                drawItem.point1 = pts[i];
                drawItem.point2 = pts[i+1];
                this.addDrawingObject(drawItem);
                addn++;
            }
        } else {
            try {
                const tokens = tokenize(args);
                const items = parseTokens(tokens);
                for (let item of items) {
                    const drawItem = new DrawItem(item.type)
                    Object.assign(drawItem, item);
                    this.addDrawingObject(drawItem);
                    addn++;
                }
            } catch (err) { showError(err); }
        }
        if (addn > 0) {
            this.refreshDrawingCanvas();
        }
    }

    executeCommand(cmd) {
        const c = cmd.split(' ')[0];
        if (c === 'draw') {
            this.cmdDraw(cmd.substr(5));
        } else if (c === 'clear') {
            this.cmdClear();
            this.refreshDrawingCanvas();
            this.clearSelection();
        } else if (c === 'zoom') {
            this.cmdZoom();
        } else if (c === 'set') {
            this.cmdSet(cmd.substr(4));
        }else {
            showError(`cann't not execute '${cmd}'`);
        }
    }

    /**
     * @param ctx { CanvasRenderingContext2D }
     * @param from { {x: float, y: float } }
     * @param to { {x: float, y: float } }
     * @param height { float }
     * @param text { string }
     * @param ratio { float }
     * @param ignoreLength { boolean }
     */
    static drawTextAtLine(ctx, from, to, height, text, ratio, ignoreLength)
    {
        ctx.save();
        ctx.textBaseline = "bottom";
        const expectedHeight = height;
        ctx.font = "48px serif";
        const m = ctx.measureText(text);
        const c = PointAdd(from, to);
        const diff = PointSub(from, to);
        const atanv = Math.atan(diff.y / (diff.x == 0 ? 1 : diff.x));
        const angle = diff.x == 0 ? (diff.y > 0 ? Math.PI / 2 : Math.PI * 1.5) : (diff.x > 0 ? atanv : atanv + Math.PI);
        const textheight = m.actualBoundingBoxAscent - m.actualBoundingBoxDescent;
        const len = Math.sqrt(diff.x * diff.x + diff.y * diff.y);
        const s = Math.min(ignoreLength ? expectedHeight /textheight : len / m.width, expectedHeight / textheight) * ratio;
        const t = 
            AffineTransformation.translate(c.x / 2, c.y / 2)
                .concat(AffineTransformation.rotate(-angle + Math.PI))
                .concat(AffineTransformation.scale(s, s))
                .concat(AffineTransformation.translate(-m.width/2, -textheight/2))
                .concat(new AffineTransformation(1, 0, 0, -1, 0, 0));
        ctx.setTransform(ctx.getTransform().multiply(t.convertToDOMMatrix()));
        ctx.fillText(text, 0, 0);
        ctx.restore();
    }

    /**
     * @param ctx { CanvasRenderingContext2D }
     * @param item { DrawItem }
     */
    static drawItemInCanvas(ctx, item) {
        if (item.type == "line") {
            ctx.strokeStyle = item.color;
            ctx.lineWidth = item.width;
            const path = new Path2D();
            path.lineTo(item.point1.x, item.point1.y);
            path.lineTo(item.point2.x, item.point2.y);
            ctx.stroke(path);
            if (item.comment) {
                ctx.fillStyle = 'white';
                this.drawTextAtLine(ctx, item.point1, item.point2, item.width, item.comment, 0.95, false);
            }
        } else if (item.type == "circle") {
            ctx.fillStyle = item.color;
            const path = new Path2D();
            path.ellipse(item.center.x, item.center.y, item.radius,
                item.radius, 0, 0, 360);;
            ctx.fill(path);
            if (item.comment) {
                ctx.fillStyle = 'white';
                this.drawTextAtLine(ctx, PointSub(item.center, {x: item.radius*0.6, y:0}), PointAdd(item.center, {x: item.radius*0.6, y:0}), item.radius * 1.2, item.comment, 0.95, false);
            }
        } else if (item.type == "cline") {
            ctx.strokeStyle = item.color;
            ctx.lineWidth = item.width;
            {
                const path = new Path2D();
                path.lineTo(item.point1.x, item.point1.y);
                path.lineTo(item.point2.x, item.point2.y);
                ctx.stroke(path);
            }
            {
                ctx.fillStyle = item.color;
                const path = new Path2D();
                path.ellipse(item.point1.x, item.point1.y, item.width / 2,
                    item.width / 2, 0, 0, 360);;
                ctx.fill(path);
            }
            {
                ctx.fillStyle = item.color;
                const path = new Path2D();
                path.ellipse(item.point2.x, item.point2.y, item.width / 2,
                    item.width / 2, 0, 0, 360);;
                ctx.fill(path);
            }
            if (item.comment) {
                ctx.fillStyle = 'white';
                this.drawTextAtLine(ctx, item.point1, item.point2, item.width, item.comment, 0.95, false);
            }
        } else if (item.type == "polygon") {
            ctx.fillStyle = item.color;
            {
                const path = new Path2D();
                for (let p of item.points) {
                    path.lineTo(p.x, p.y);
                }
                path.closePath();
                ctx.fill(path);
            }
        }
    }

    refreshDrawingCanvas() {
        let ctx = this.m_canvas.getContext("2d");
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.clearRect(0, 0, this.m_canvas.width, this.m_canvas.height);

        const baseTrans = new AffineTransformation(1, 0, 0, -1, this.m_canvas.width / 2, this.m_canvas.height / 2);
        ctx.setTransform(baseTrans.concat(this.m_transform).convertToDOMMatrix());
        for (let item of this.m_objectList) {
            Viewport.drawItemInCanvas(ctx, item);
        }

        this.refreshCoordination();
    }

    refreshCoordination() {
        let ctx = this.m_coordinationBox.getContext("2d");
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.clearRect(0, 0, this.m_coordinationBox.width, this.m_coordinationBox.height);

        const baseTrans = new AffineTransformation(1, 0, 0, -1, this.m_coordinationBox.width / 2, this.m_coordinationBox.height / 2);
        const t = baseTrans.concat(this.m_transform);
        ctx.setTransform(t.a, t.c, t.b, t.d, t.tx, t.ty);
        const w1 = this.m_coordinationBox.width / 2;
        const h1 = this.m_coordinationBox.height / 2;
        const a = this.m_transform.revertXY({x: -w1, y: -h1});
        const b = this.m_transform.revertXY({x: -w1, y: h1});
        const c = this.m_transform.revertXY({x: w1, y: h1});
        const d = this.m_transform.revertXY({x: w1, y: -h1});
        const un = 2 ** 30;
        const k1 = { x: -un, y: 0 };
        const k2 = { x: un, y: 0 };
        const m1 = { x: 0, y: 0 - un };
        const m2 = { x: 0, y: un };

        const fn = (s1, s2) => {
            const ans = [];
            const u1 = findLineSegmentIntersection(s1, s2, a, b);
            const u2 = findLineSegmentIntersection(s1, s2, b, c);
            const u3 = findLineSegmentIntersection(s1, s2, c, d);
            const u4 = findLineSegmentIntersection(s1, s2, d, a);
            if (u1) ans.push(u1);
            if (u2) ans.push(u2);
            if (u3) ans.push(u3);
            if (u4) ans.push(u4);
            if (ans.length >= 2) {
                return ans.slice(0, 2);
            }
            return null;
        };
        const s1 = fn(k1, k2);
        const s2 = fn(m1, m2);
        const segs = [];
        if (s1) segs.push(s1);
        if (s2) segs.push(s2);

        for (let seg of segs) {
            const path = new Path2D();
            path.lineTo(seg[0].x, seg[0].y);
            path.lineTo(seg[1].x, seg[1].y);
            ctx.strokeStyle = "rgba(60, 60, 60, 0.5)";
            ctx.lineWidth = 1;
            ctx.stroke(path);
        }
    }

    drawSelection(start, to) {
        let ctx = this.m_selectionBox.getContext("2d");
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.clearRect(0, 0, this.m_selectionBox.width, this.m_selectionBox.height);
        ctx.fillStyle = "rgba(93, 93, 255, 0.3)";
        const width = Math.abs(to.x - start.x);
        const height = Math.abs(to.y - start.y);
        const minX = Math.min(start.x, to.x);
        const minY = Math.min(start.y, to.y)
        ctx.fillRect(minX, minY, width, height);

        ctx.strokeStyle = "rgba(80, 80, 255, 0.8)";
        ctx.lineWidth = 2;
        const path = new Path2D();
        path.lineTo(start.x, start.y);
        path.lineTo(start.x, to.y);
        path.lineTo(to.x, to.y);
        path.lineTo(to.x, start.y);
        path.closePath();
        ctx.stroke(path);
        this.m_selectionStart = start;
        this.m_selectionTo = to;
        this.m_selectedItems = [];
        this.drawSelectedItem(this.m_selectionStart, this.m_selectionTo);
    }

    drawSelectedItem(start, to) {
        if (start == null || to == null) {
            this.m_selectedItems = [];
        } else {
            const baseTrans = new AffineTransformation(1, 0, 0, -1, this.m_coordinationBox.width / 2, this.m_coordinationBox.height / 2);
            const t = baseTrans.concat(this.m_transform);
            const box = new BoundingBox(t.revertXY(start), t.revertXY(to));
            this.m_selectedItems = this.m_objectRTree.search({
                minX: box.getBL().x, minY: box.getBL().y,
                maxX: box.getTR().x, maxY: box.getTR().y
            });
        }
        this.refreshSelection();
    }

    RemoveSelectedItems() {
        for (let obj of this.m_selectedItems) {
            this.removeDrawingObject(obj['object']);
        }
        this.m_selectedItems = [];
        this.refreshSelection();
        this.refreshDrawingCanvas();
    }

    refreshSelection() {
        let ctx = this.m_selectedItemsCanvas.getContext("2d");
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.clearRect(0, 0, this.m_selectedItemsCanvas.width, this.m_selectedItemsCanvas.height);
        const baseTrans = new AffineTransformation(1, 0, 0, -1, this.m_coordinationBox.width / 2, this.m_coordinationBox.height / 2);
        const t = baseTrans.concat(this.m_transform);
        ctx.setTransform(t.a, t.c, t.b, t.d, t.tx, t.ty);
        for (let item of this.m_selectedItems) {
            const obj = item["object"];
            const oldColor = obj.color;
            try {
                obj.color = "rgba(200, 200, 230, 0.3)";
                Viewport.drawItemInCanvas(ctx, item["object"]);
            } finally {
                obj.color = oldColor;
            }
        }
    }

    clearSelection() {
        let ctx = this.m_selectionBox.getContext("2d");
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.clearRect(0, 0, this.m_selectionBox.width, this.m_selectionBox.height);
        this.drawSelectedItem(this.m_selectionStart, this.m_selectionTo);
    }

    fitCanvas()
    {
        this.m_canvas.width = this.m_viewportEl.clientWidth;
        this.m_canvas.height = this.m_viewportEl.clientHeight;
        this.m_selectionBox.width = this.m_viewportEl.clientWidth;
        this.m_selectionBox.height = this.m_viewportEl.clientHeight;
        this.m_selectedItemsCanvas.width = this.m_viewportEl.clientWidth;
        this.m_selectedItemsCanvas.height = this.m_viewportEl.clientHeight;
        this.m_coordinationBox.width = this.m_viewportEl.clientWidth;
        this.m_coordinationBox.height = this.m_viewportEl.clientHeight;
        this.refreshDrawingCanvas();
    }

    get paused() { return this.m_paused; }
    get totalFrames() { return this.m_totalFrames; }
    get currentFrame() { return this.m_currentFrame; }

    canvasCoordToReal(point) {
        const baseTrans = new AffineTransformation(1, 0, 0, -1, this.m_canvas.width / 2, this.m_canvas.height / 2);
        const t = baseTrans.concat(this.m_transform);
        const ans = t.revertXY(point);
        return {x: Math.round(ans.x), y: Math.round(ans.y)};
    }

    reset()
    {
        this.m_transform = AffineTransformation.identity();
        viewport.refreshDrawingCanvas();
    }
    fitScreen()
    {
        let box = null;
        for (let obj of this.m_objectList) {
            const kbox = obj.getBox();
            if (box == null) {
                box = kbox;
            } else {
                if (kbox) {
                    box = box.mergeBox(kbox);
                }
            }
        }

        if (box == null)
        {
            return;
        }
        box = box.inflate(10);

        const boxviewport = new BoundingBox({
            x: -this.m_canvas.width / 2,
            y: -this.m_canvas.height/2
        }, {
            x: this.m_canvas.width / 2,
            y: this.m_canvas.height/2
        });
        this.m_transform = Box2boxTransformation(box, boxviewport);
        viewport.refreshDrawingCanvas();
    }
    scaleUp(X, Y)
    {
        this.scale(1.1, 1.1, X, Y);
        this.refreshDrawingCanvas();
        this.refreshSelection();
    }
    scaleDown(X, Y)
    {
        this.scale(1/1.1, 1/1.1, X, Y);
        this.refreshDrawingCanvas();
        this.refreshSelection();
    }
    moveLeft()
    {
        this.translate(-50, 0);
        this.refreshDrawingCanvas(); 
        this.refreshSelection();
    }
    moveRight()
    {
        this.translate(50, 0);
        this.refreshDrawingCanvas(); 
        this.refreshSelection();
    }
    moveUp()
    {
        this.translate(0, 50);
        this.refreshDrawingCanvas(); 
        this.refreshSelection();
    }
    moveDown()
    {
        this.translate(0, -50);
        this.refreshDrawingCanvas(); 
        this.refreshSelection();
    }

    scale(scaleX, scaleY, _X, _Y)
    {
        const X = _X || 0
        const Y = _Y || 0
        const xy = this.m_transform.applyXY({x: X, y: Y});
        const translation1 = new AffineTransformation(1, 0, 0, 1, -xy.x, -xy.y);
        const scaling = new AffineTransformation(scaleX, 0, 0, scaleY, 0, 0);
        const translation2 = new AffineTransformation(1, 0, 0, 1, xy.x, xy.y);

        const scaleAt = translation2.concat(scaling.concat(translation1));
        this.m_transform = scaleAt.concat(this.m_transform);
    }

    translate(X, Y)
	{
        const v1 = this.m_transform.revertXY({x: X, y: Y});
        const v2 = this.m_transform.revertXY({x: 0, y: 0});
        this.m_transform = this.m_transform.concat(new AffineTransformation(1, 0, 0, 1, v1.x - v2.x, v1.y - v2.y));
	}

    rotate(clockDegree)
    {
        const c = Math.cos(clockDegree / 180 * Math.PI);
        const s = Math.sin(clockDegree / 180 * Math.PI);
        this.m_transform = this.m_transform.concat(new AffineTransformation(c, s, -s, c, 0, 0));
    }

    play()
    {
        this.m_paused = false;
    }

    pause()
    {
        this.m_paused = true;
    }

    async setFrame(n)
    {
        if (n > this.m_totalFrames - 1) return;
        this.m_currentFrame = Math.max(Math.min(n, this.m_totalFrames - 1), 0);
        const text = await this.m_loader(this.m_currentFrame);
        this.cmdClear();
        const objlist = JSON.parse(text);
        for (let obj of objlist) {
            const drawItem = new DrawItem(obj.type)
            Object.assign(drawItem, obj);
            this.addDrawingObject(drawItem);
        }
        this.refreshDrawingCanvas();
    }

    init(box, totalFrames, loader)
    {
        this.m_currentFrame = 0;
        this.m_totalFrames = totalFrames;
        if (box) {
            const boxviewport = new BoundingBox({
                x: -this.m_canvas.width / 2,
                y: -this.m_canvas.height/2
            }, {
                x: this.m_canvas.width / 2,
                y: this.m_canvas.height/2
            });
            this.m_transform = Box2boxTransformation(box, boxviewport);
        } else {
            this.m_transform = AffineTransformation.identity();
        }
        this.m_loader = loader;
        this.setFrame(0);
    }

    m_paused = true;
    m_currentFrame = 0;
    m_totalFrames = 30;
    m_loader;
    m_transform;
    m_viewportEl;
    m_objectList;
}
const viewport = new Viewport('viewport');

// Play & pause player
function toggleViewportStatus() {
    if (viewport.paused) {
        viewport.play();
    } else {
        viewport.pause();
    }
    updatePlayIcon();
}

// update play/pause icon
function updatePlayIcon() {
    if (viewport.paused) {
        play.classList.add("playbtn")
    } else {
        play.classList.remove("playbtn")
    }
}

// Update progress & timestamp
function updateProgress() {
    const totalFrames = viewport.totalFrames;
    const currentFrame = viewport.currentFrame;
    if (totalFrames == 1) {
        progress.value = 100;
    } else {
        progress.value = (currentFrame / Math.max(totalFrames - 1, 1)) * 100;
    }

    timestamp.innerHTML = `${Math.min(currentFrame + 1, totalFrames)}/${totalFrames}`;
}

// Set viewport frame progress
function setViewportProgress() {
    viewport.setFrame(Math.round(progress.value * Math.max(viewport.totalFrames - 1, 0) / 100));
    updateProgress()
}

// Stop player
function stopViewport() {
    viewport.setFrame(0);
    viewport.pause();
    updatePlayIcon();
    updateProgress();
}


play.addEventListener('click', toggleViewportStatus);

stop.addEventListener('click', stopViewport);

progress.addEventListener('change', setViewportProgress);

reset.addEventListener('click', () => viewport.reset());
fitScreen.addEventListener('click', () => viewport.fitScreen());
scaleUp.addEventListener('click', () => viewport.scaleUp());
scaleDown.addEventListener('click', () => viewport.scaleDown());
moveLeft.addEventListener('click', () => viewport.moveLeft());
moveRight.addEventListener('click', () => viewport.moveRight());
moveUp.addEventListener('click', () => viewport.moveUp());
moveDown.addEventListener('click', () => viewport.moveDown());

fullviewport.addEventListener('wheel', (e) => {
    const pt = viewport.canvasCoordToReal({x: e.offsetX, y: e.offsetY});
    if (e.deltaY < 0) {
        viewport.scaleUp(pt.x, pt.y);
    } else if (e.deltaY > 0) {
        viewport.scaleDown(pt.x, pt.y);
    }
});

let isInDragMode = false;
let dragModePrevPt = {};
let isInSelectionMode = false;
let selectionStart = {};
function enterDragMode(pt)
{
    isInDragMode = true;
    dragModePrevPt = pt
    fullviewport.classList.add("drag-mode");
}
function leaveDragMode()
{
    isInDragMode = false;
    fullviewport.classList.remove("drag-mode");
}
function enterSelectionMode(pt)
{
    isInSelectionMode = true;
    selectionStart = pt
    fullviewport.classList.add("selection-mode");
    viewport.drawSelection(pt, pt);
}
function leaveSelectionMode()
{
    isInSelectionMode = false;
    fullviewport.classList.remove("selection-mode");
    viewport.clearSelection();
}

fullviewport.addEventListener('mousemove', (e) => {
    if (isInDragMode) {
        viewport.translate(e.offsetX - dragModePrevPt.x, dragModePrevPt.y - e.offsetY);
        viewport.refreshDrawingCanvas();
        dragModePrevPt = {x: e.offsetX, y: e.offsetY};
    } else {
        const pt = viewport.canvasCoordToReal({x: e.offsetX, y: e.offsetY});
        cursorCoordination.innerHTML = `(${pt.x}, ${pt.y})`;

        if (isInSelectionMode) {
            viewport.drawSelection(selectionStart, {x: e.offsetX, y: e.offsetY});
        }
    }
});
fullviewport.addEventListener('mousedown', (e) => {
    if ((e.buttons & 4) != 0) {
        enterDragMode({x: e.offsetX, y: e.offsetY});
    }
    if ((e.buttons & 1) != 0) {
        enterSelectionMode({x: e.offsetX, y: e.offsetY});
    }
});
fullviewport.addEventListener('mouseleave', () => {
    leaveDragMode();
    leaveSelectionMode();
});
fullviewport.addEventListener('mouseup', (e) => {
    if ((e.buttons & 4) == 0) {
        leaveDragMode();
    }
    if ((e.buttons & 1) == 0) {
        leaveSelectionMode();
    }
});
fullviewport.addEventListener("contextmenu", (e) => {
    e.preventDefault();
    e.stopPropagation();
});

function hideInputBar() {
    inputBar.classList.remove("input-bar-show");
}
function showInputBar() {
    inputBar.classList.add("input-bar-show");
    commandLineBar.focus();
    commandLineBar.value = "";
}

commandLineBar.addEventListener("keyup", e => {
    e.stopPropagation();
});
const commandHistory = [];
let historyIndex = -1;
let tempCommand = '';
commandLineBar.addEventListener("keydown", e => {
    e.stopPropagation();
    if (e.key == 'Escape') {
        hideInputBar();
    } else if (e.key == 'Enter') {
        viewport.executeCommand(commandLineBar.value.trim());
        commandHistory.push(commandLineBar.value.trim());
        hideInputBar();
    } else if (e.key == 'ArrowUp' || (e.key == 'p' && e.ctrlKey)) {
        if (historyIndex == -1 && commandHistory.length > 0) {
            historyIndex = commandHistory.length - 1;
            tempCommand = commandLineBar.value;
            commandLineBar.value = commandHistory[historyIndex];
        } else if (historyIndex > 0) {
            if (commandLineBar.value != commandHistory[historyIndex]) {
                tempCommand = commandLineBar.value;
            }
            historyIndex--;
            commandLineBar.value = commandHistory[historyIndex];
        }
        e.preventDefault();
    } else if (e.key == 'ArrowDown') {
        if (historyIndex >= 0) {
            if (historyIndex + 1 == commandHistory.length) {
                historyIndex = -1;
                commandLineBar.value = tempCommand;
            } else {
                if (commandLineBar.value != commandHistory[historyIndex]) {
                    tempCommand = commandLineBar.value;
                }
                historyIndex++;
                commandLineBar.value = commandHistory[historyIndex];
            }
        }
    }
});

window.addEventListener("keyup", (e) => {
    if (e.key == 'c') {
        showInputBar();
        historyIndex = -1;
        tempCommand = '';
    } else if (e.key == 'Escape') {
        viewport.clearSelection();
        viewport.drawSelectedItem();
        hideInputBar();
    } else if (e.key == 'Delete') {
        viewport.RemoveSelectedItems();
    } else if (e.key == ' ') {
        toggleViewportStatus();
    }
});

async function setupConnection()
{
    const INFOAPI = location.protocol + "//" + location.host + "/data-info";
    const resp = await fetch(INFOAPI);
    const data = await resp.json();
    let box = null;
    let nframes = 0;
    if (data["minxy"]) {
        box = new BoundingBox(data["minxy"], data["maxxy"]);
        nframes = data["nframes"];
    }

    viewport.init(box, nframes, async (n) => {
        const API = location.protocol + "//" + location.host + "/frame/" + n;
        const resp = await fetch(API);
        const data = await resp.json();
        return JSON.stringify(data["drawings"] || []);
    });
    updateProgress();
    updatePlayIcon();

    framePerSec.addEventListener("change", () => {
        framePerSecondValue = framePerSec.valueAsNumber;
    });
    let framePerSecondValue = 1;
    let prevFresh = Date.now();
    while (true) {
        const now = Date.now();
        const nextTimeout = Math.max(0, prevFresh + 1000 / framePerSecondValue - now);
        await new Promise(r => setTimeout(r, nextTimeout));
        prevFresh = Date.now();
        if (!viewport.paused) {
            try {
                await viewport.setFrame(viewport.currentFrame+1);
                updateProgress();
            } catch {}
        }
    };
}

setupConnection();
