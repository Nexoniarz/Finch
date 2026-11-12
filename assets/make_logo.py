# Draws the Finch mark (a faceted hexagon with a finch in flight) and puts it into the logos:
#   python3 assets/make_logo.py assets/icon.svg      the mark alone; see the bottom of this file for the rest
# Then render the PNGs, e.g. with rsvg-convert (BRAND.md has the list).
import math, sys

C = 256.0          # centre
R = 236.0          # hexagon radius (to the corners)
ROUND = 34.0       # corner rounding


def hex_points(r=R):
    # pointy top, clockwise from the top
    return [(C + r * math.sin(math.radians(a)), C - r * math.cos(math.radians(a))) for a in range(0, 360, 60)]


def rounded(points, rad):
    n = len(points)
    d = []
    for i in range(n):
        p0, p1, p2 = points[i - 1], points[i], points[(i + 1) % n]
        v1 = (p0[0] - p1[0], p0[1] - p1[1])
        v2 = (p2[0] - p1[0], p2[1] - p1[1])
        l1, l2 = math.hypot(*v1), math.hypot(*v2)
        a = (p1[0] + v1[0] / l1 * rad, p1[1] + v1[1] / l1 * rad)
        b = (p1[0] + v2[0] / l2 * rad, p1[1] + v2[1] / l2 * rad)
        d.append(("M" if i == 0 else "L") + f"{a[0]:.1f} {a[1]:.1f} Q{p1[0]:.1f} {p1[1]:.1f} {b[0]:.1f} {b[1]:.1f}")
    return " ".join(d) + " Z"


def poly(points):
    return "M" + " L".join(f"{x:.1f} {y:.1f}" for x, y in points) + " Z"


P = hex_points()
top, ur, lr, bot, ll, ul = P
centre = (C, C)
# three facets meeting a little right of the centre, like light from the upper left
k = (C + 26, C + 18)
LIGHT = poly([top, k, ll, ul])                 # the upper-left half
MID = poly([top, ur, lr, k])                    # the right
DARK = poly([ll, k, lr, bot])                   # the bottom

# the finch: in flight towards the upper right, wings swept back, a short seed-cracker beak, a forked tail
BIRD = """
M 396 214
C 396 176 368 150 334 150
C 306 150 284 168 276 194
C 246 160 196 126 120 116
C 132 170 180 210 236 226
C 206 226 172 236 142 254
C 178 264 212 268 246 266
C 226 294 194 318 158 336
L 120 368
L 172 356
L 152 396
L 206 362
C 268 350 322 322 360 290
C 384 268 396 242 396 214
Z
"""
BEAK = "M 386 178 L 426 194 L 392 214 Z"
EYE = (352, 186, 9)


def svg(size=512, bg=None, colors=("#FFA24D", "#FF7A1A", "#D9570B"), bird="#FFFFFF", beak="#FFC145", title="Finch"):
    light, mid, dark = colors
    hexpath = rounded(P, ROUND)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="{size}" height="{size}">',
           f'  <title>{title}</title>',
           '  <defs>',
           f'    <clipPath id="hex"><path d="{hexpath}"/></clipPath>',
           '  </defs>']
    if bg:
        out.append(f'  <rect width="512" height="512" fill="{bg}"/>')
    out += ['  <g clip-path="url(#hex)">',
            f'    <rect width="512" height="512" fill="{mid}"/>',
            f'    <path d="{LIGHT}" fill="{light}"/>',
            f'    <path d="{DARK}" fill="{dark}"/>',
            '  </g>',
            '  <!-- the finch, in flight -->',
            '  <g transform="translate(14 22)">',
            f'  <path d="{BEAK}" fill="{beak}"/>',
            f'  <path d="{" ".join(BIRD.split())}" fill="{bird}"/>',
            f'  <circle cx="{EYE[0]}" cy="{EYE[1]}" r="{EYE[2]}" fill="{dark}"/>',
            '  </g>',
            '</svg>']
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    open(sys.argv[1], "w").write(svg())


def mark(clip_id):
    """the hexagon and the bird, as SVG elements for any document (the clip path gets its own id)"""
    s = svg()
    body = s.split('\n', 2)[2].rsplit('</svg>', 1)[0]     # without <svg ...> and <title>
    return body.replace('id="hex"', f'id="{clip_id}"').replace('url(#hex)', f'url(#{clip_id})')


def swap_bird(path, clip_id):
    text = open(path).read()
    start = text.index('<g transform="translate(0 6) scale(0.56)">')
    end = text.index('<g transform="translate(330 20)">')
    new = '<g transform="translate(0 6) scale(0.56)">\n' + mark(clip_id) + '  </g>\n  '
    open(path, 'w').write(text[:start] + new + text[end:])
