#!/usr/bin/env python3


from bottle import get, run, static_file
import argparse
import re


def tokenize(input_string):
    # Pattern to match tokens, improved to ignore comments
    token_pattern = r'\"[^\"]*\"|\(|\)|-?\d+\.\d+|-?\d+|\S+'
    # Split the input string into lines to handle comments
    lines = input_string.split('\n')
    tokens = []
    for line in lines:
        # Ignore content after a semicolon (comment)
        line = line.split(';', 1)[0]
        tokens.extend(re.findall(token_pattern, line))
    return tokens

def parse_tokens(tokens):
    def next_token():
        return tokens.pop(0) if tokens else None

    def parse_shape():
        shape = {"type": next_token()}
        while tokens and tokens[0] != ')':
            if tokens[0] == '(':
                next_token()  # Consume '('
                key = next_token()
                if shape["type"] in ["line", "cline"] and key == "point":
                    key = "point2" if "point1" in shape else "point1"
                if key in ['point', 'point1', 'point2', 'center']:
                    x = float(next_token())
                    y = float(next_token())
                    value = {'x': x, 'y': y}
                elif key in ['radius', 'width']:
                    value = float(next_token())
                elif key in ['color', 'comment', 'layer']:
                    value = next_token().strip('"')
                next_token()  # Consume ')'
                if shape["type"] == "polygon" and key == "point":
                    shape.setdefault("points", []).append(value)
                else:
                    shape[key] = value
            else:
                break  # Unexpected token
        next_token()  # Consume closing ')'
        return shape

    def parse_scene():
        shapes = []
        next_token()  # Consume 'scene'
        while tokens and tokens[0] != ')':
            if tokens[0] == '(':
                next_token()  # Consume opening '('
                if tokens[0] in ['cline', 'line', 'circle', 'polygon']:
                    shapes.append(parse_shape())
                else:
                    print(f"Unknown shape type: {tokens[0]}")
            else:
                next_token()  # Skip unexpected tokens
        next_token()  # Consume closing ')' for 'scene'
        return shapes

    # Start parsing from the 'scene' keyword
    if tokens and tokens[0] == '(' and tokens[1] == 'scene':
        return parse_scene()
    else:
        print("Input does not start with a scene.")
        return []


def serialize_shape(shape):
    serialized = f'({shape["type"]}'
    if shape["type"] == "polygon":
        for point in shape["points"]:
            serialized += f' (point {point["x"]} {point["y"]})'
    else:
        if "point1" in shape:  # For lines
            serialized += f' (point {shape["point1"]["x"]} {shape["point1"]["y"]})'
            serialized += f' (point {shape["point2"]["x"]} {shape["point2"]["y"]})'
        if "center" in shape:  # For circles
            serialized += f' (center {shape["center"]["x"]} {shape["center"]["y"]})'
            serialized += f' (radius {shape["radius"]})'
    if "width" in shape:  # For lines with width
        serialized += f' (width {shape["width"]})'
    if "color" in shape:  # Color for any shape
        serialized += f' (color "{shape["color"]}")'
    serialized += ')'
    return serialized

def serialize_shapes(shapes):
    serialized = "(scene\n"
    for shape in shapes:
        serialized += "  " + serialize_shape(shape) + "\n"
    serialized += ")"
    return serialized


@get("/")
def getroot():
    return static_file("index.html", root="./")

@get("/<filepath:re:.*\\.ico>")
def getfavicon(filepath):
    return static_file(filepath, root="./")

@get("/<filepath:re:.*\\.html>")
def gethtml(filepath):
    return static_file(filepath, root="./")

@get("/<filepath:re:.*\\.js>")
def getjavascript(filepath):
    return static_file(filepath, root="./")

@get("/css/<filepath:re:.*\\.css>")
def getcss(filepath):
    return static_file(filepath, root="css")

@get("/fonts/<filepath:re:.*>")
def getfonts(filepath):
    return static_file(filepath, root="fonts")

global minCoord, maxCoord, frameOffset, openInput
minCoord = {}
maxCoord = {}
frameOffset = []
openInput = None

def mergePoint(p):
    global minCoord, maxCoord
    if len(minCoord) == 0:
        minCoord = p
        maxCoord = {"x": p["x"], "y": p["y"]}
    else:
        minCoord["x"] = min(minCoord["x"], p["x"])
        minCoord["y"] = min(minCoord["y"], p["y"])
        maxCoord["x"] = max(maxCoord["x"], p["x"])
        maxCoord["y"] = max(maxCoord["y"], p["y"])

@get("/data-info")
def datainfo():
    if len(frameOffset) == 0 or len(minCoord) == 0 or len(maxCoord) == 0:
        return {}
    else:
        return { "minxy": minCoord, "maxxy": maxCoord, "nframes": len(frameOffset) }

@get("/frame/<nx>")
def getFrame(nx):
    assert(openInput is not None)
    n = int(nx)
    if n >= len(frameOffset):
        return { "drawings": [] }
    openInput.seek(frameOffset[n])
    text = openInput.readline().strip()
    tokens = tokenize(text)
    shapes = parse_tokens(tokens)
    return { "drawings": shapes }

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="serve 2DataViewer")
    parser.add_argument("--input", "-i", type=str, required=True, help="Input file containing drawings")
    parser.add_argument("--host",        type=str, default="0.0.0.0", help="listening host address")
    parser.add_argument("--port", "-p",  type=int, default=3527, help="listening port")
    args = parser.parse_args()

    openInput = open(args.input, "r")
    while True:
        off = openInput.tell()
        n = openInput.readline().strip()
        if n == '':
            break
        frameOffset.append(off)
        if (len(frameOffset) > 1):
            continue
        try:
            shapes = parse_tokens(tokenize(n))
            for s in shapes:
                if s["type"] == "circle":
                    c = s["center"]
                    r = s["radius"]
                    mergePoint({"x": c["x"] - r, "y": c["y"] - r})
                    mergePoint({"x": c["x"] - r, "y": c["y"] + r})
                    mergePoint({"x": c["x"] + r, "y": c["y"] + r})
                    mergePoint({"x": c["x"] + r, "y": c["y"] - r})
                elif s["type"] == "line":
                    mergePoint(s["point1"])
                    mergePoint(s["point2"])
                elif s["type"] == "cline":
                    mergePoint(s["point1"])
                    mergePoint(s["point2"])
                elif s["type"] == "polygon":
                    for pt in s["points"]:
                        mergePoint(pt)
        except:
            pass

    run(host=args.host, port=args.port)
