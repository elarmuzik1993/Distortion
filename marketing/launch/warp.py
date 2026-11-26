"""Warps the plugin screenshot onto the tilted screen's quad at full quality.

Chromium rasterizes 3D-transformed layers below the page's pixel density, so the
tilted screenshot came out soft. render.js reads where the screen's four corners
land on the page and calls this to do the perspective itself: supersampled 2x,
bicubic, then Lanczos down, so the 2880 px editor render keeps its detail.

  python3 warp.py SRC OUT CANVAS_W CANVAS_H SCALE x0 y0 x1 y1 x2 y2 x3 y3

Corners are in CSS px, clockwise from top-left; CANVAS_* in CSS px; SCALE is the
device pixel ratio of the output. OUT is a transparent PNG the size of the page.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw

SS = 2                      # supersampling factor
RADIUS_CSS = 8.5            # .screen border-radius minus its border, in CSS px
SHEEN = [(0.00, 0.16), (0.28, 0.04), (0.42, 0.0)]  # matches .screen::after


def perspective_coeffs(dst, src):
    """PIL PERSPECTIVE coefficients that map output points dst to input points src."""
    rows, rhs = [], []
    for (x, y), (u, v) in zip(dst, src):
        rows.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); rhs.append(u)
        rows.append([0, 0, 0, x, y, 1, -v * x, -v * y]); rhs.append(v)
    return np.linalg.solve(np.array(rows, float), np.array(rhs, float)).tolist()


def add_sheen(img):
    """The glass sheen: a white diagonal fade across the top-left of the face."""
    w, h = img.size
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    # Position along a 118deg CSS gradient line, 0 at its start, 1 at its end.
    a = np.deg2rad(118.0)
    dx, dy = np.sin(a), -np.cos(a)
    length = abs(w * dx) + abs(h * dy)
    t = ((xx - w / 2) * dx + (yy - h / 2) * dy) / length + 0.5
    alpha = np.interp(t, [p for p, _ in SHEEN], [v for _, v in SHEEN])
    rgba = np.asarray(img, np.float32)
    rgba[..., :3] = rgba[..., :3] * (1 - alpha[..., None]) + 255 * alpha[..., None]
    return Image.fromarray(rgba.clip(0, 255).astype(np.uint8), "RGBA")


def main():
    src_path, out_path = sys.argv[1], sys.argv[2]
    canvas_w, canvas_h, scale = float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
    quad = np.array(sys.argv[6:14], float).reshape(4, 2)

    src = Image.open(src_path).convert("RGBA")
    sw, sh = src.size
    # Round the corners the way the .screen box clips them.
    css_w = np.linalg.norm(quad[1] - quad[0])
    r = RADIUS_CSS * sw / css_w
    mask = Image.new("L", src.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, sw - 1, sh - 1], radius=r, fill=255)
    src = add_sheen(src)
    src.putalpha(Image.fromarray(np.minimum(np.asarray(src.getchannel("A")), np.asarray(mask))))

    k = scale * SS
    dst = quad * k
    x0, y0 = np.floor(dst.min(axis=0) / SS) * SS
    x1, y1 = np.ceil(dst.max(axis=0) / SS) * SS
    box_w, box_h = int(x1 - x0), int(y1 - y0)
    coeffs = perspective_coeffs(dst - [x0, y0], [(0, 0), (sw, 0), (sw, sh), (0, sh)])
    warped = src.transform((box_w, box_h), Image.PERSPECTIVE, coeffs,
                           Image.BICUBIC, fillcolor=(0, 0, 0, 0))
    warped = warped.resize((box_w // SS, box_h // SS), Image.LANCZOS)

    out = Image.new("RGBA", (round(canvas_w * scale), round(canvas_h * scale)), (0, 0, 0, 0))
    out.alpha_composite(warped, (int(x0 // SS), int(y0 // SS)))
    out.save(out_path)


if __name__ == "__main__":
    main()
