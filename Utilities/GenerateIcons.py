"""Generates the editor icons of the XR Toolkit components.

Every icon is drawn once as a list of strokes on a 24 unit grid (white, 2 units wide, round caps and joins) and written
as the type icon (EditorIcons/TypeIcons/<Type>.svg) and as the viewport shape icon (Assets/Editor/ShapeIcons/<Type>.svg,
.png and .dds).

    python Utilities/GenerateIcons.py [path to TexConv.exe]

Without TexConv the .dds files are left as they are. Needs Pillow.
"""
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TYPE_ICONS = os.path.join(ROOT, "EditorIcons", "TypeIcons")
SHAPE_ICONS = os.path.join(ROOT, "Assets", "Editor", "ShapeIcons")


def line(x1, y1, x2, y2):
    return ("poly", [(x1, y1), (x2, y2)], False)


def poly(*points, closed=False):
    return ("poly", list(points), closed)


def circle(cx, cy, r):
    return ("circle", cx, cy, r)


def ellipse(cx, cy, rx, ry, n=32):
    return ("poly", [(cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)], True)


def arc(cx, cy, r, a0, a1, n=24):
    """Polyline along a circle from angle a0 to a1 in degrees, y pointing down."""
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def parabola(x0, y0, x1, y1, height, n=16):
    """A throw arc from (x0, y0) to (x1, y1) that rises by height above the straight line between them."""
    pts = []
    for i in range(n + 1):
        t = i / n
        pts.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t - 4 * height * t * (1 - t)))
    return pts


ICONS = {
    # a box that can be picked up, with a lift arrow
    "plXRGrabbableComponent": [
        poly((12, 9), (20, 13), (20, 18.5), (12, 22.5), (4, 18.5), (4, 13), closed=True), poly((4, 13), (12, 17), (20, 13)), line(12, 17, 12, 22.5),
        line(12, 2, 12, 6), poly((9.5, 4.5), (12, 2), (14.5, 4.5))],
    # an open hand
    "plXRHandComponent": [
        line(8, 12, 8, 6), line(11, 11, 11, 3.5), line(14, 11, 14, 4), line(17, 12, 17, 7),
        poly((17, 12), (17, 16), (15, 21), (10, 21), (7.5, 18), (4.5, 13), (6, 12), (8, 14), (8, 12))],
    # a beam ending in a hit dot
    "plXRPointerComponent": [line(3, 21, 12.5, 11.5), circle(16.5, 7.5, 3.5), circle(16.5, 7.5, 0.5)],
    # a throw arc landing in a ring
    "plXRTeleportComponent": [poly(*parabola(2.5, 17, 16, 19, 8)), ellipse(16, 19, 5, 2)],
    # a turn arrow around the player
    "plXRTurnComponent": [poly(*(arc(12, 12, 9, 0, 270) + [(14.8, 3.4), (18.4, 5.6), (21, 8)])), poly((21, 3), (21, 8), (16, 8))],
}

# placed in a level and not visible by themselves
SHAPE = list(ICONS.keys())


def fmt(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


def to_svg(strokes):
    parts = []
    for s in strokes:
        if s[0] == "poly":
            d = "M" + " L".join(f"{fmt(x)} {fmt(y)}" for x, y in s[1]) + (" Z" if s[2] else "")
            parts.append(f'<path d="{d}"/>')
        elif s[0] == "circle":
            parts.append(f'<circle cx="{fmt(s[1])}" cy="{fmt(s[2])}" r="{fmt(s[3])}"/>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" '
            'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + "".join(parts) + "</svg>\n")


def to_png(strokes, size=256):
    scale = size * 4 / 24.0  # drawn four times too large, then filtered down
    image = Image.new("L", (size * 4, size * 4), 0)
    draw = ImageDraw.Draw(image)
    half = scale  # stroke width 2 units

    def stroke(points, closed):
        pts = [(x * scale, y * scale) for x, y in points]
        if closed:
            pts.append(pts[0])
        draw.line(pts, fill=255, width=int(round(half * 2)))
        for px, py in pts:  # round caps and joins
            draw.ellipse((px - half, py - half, px + half, py + half), fill=255)

    for s in strokes:
        if s[0] == "poly":
            stroke(s[1], s[2])
        elif s[0] == "circle":
            n = 48
            stroke([(s[1] + s[3] * math.cos(2 * math.pi * i / n), s[2] + s[3] * math.sin(2 * math.pi * i / n)) for i in range(n)], True)

    alpha = image.resize((size, size), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    out.putalpha(alpha)
    return out


def main():
    texconv = sys.argv[1] if len(sys.argv) > 1 else None
    os.makedirs(TYPE_ICONS, exist_ok=True)
    os.makedirs(SHAPE_ICONS, exist_ok=True)

    for name, strokes in ICONS.items():
        svg = to_svg(strokes)
        with open(os.path.join(TYPE_ICONS, name + ".svg"), "w", newline="\n") as f:
            f.write(svg)

        if name in SHAPE:
            with open(os.path.join(SHAPE_ICONS, name + ".svg"), "w", newline="\n") as f:
                f.write(svg)
            png = os.path.join(SHAPE_ICONS, name + ".png")
            to_png(strokes).save(png)
            if texconv:
                subprocess.run([texconv, "-out", os.path.join(SHAPE_ICONS, name + ".dds"), "-in", png, "-rgba", "in0", "-dilate", "true",
                                "-compression", "Medium", "-usage", "Color", "-mipmaps", "Kaiser"], check=True, capture_output=True)

    print(f"{len(ICONS)} type icons, {len(SHAPE)} shape icons")


if __name__ == "__main__":
    main()
