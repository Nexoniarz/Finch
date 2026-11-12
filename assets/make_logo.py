# Draws the Finch mark, the f of the wordmark with a dot in its top-right corner, and writes every logo file:
#   python3 assets/make_logo.py            the SVGs: icon.svg, logo-mark*.svg, and the icon inside logo*.svg and social-preview.svg
#   python3 assets/make_logo.py --png      and renders the PNGs too (rsvg-convert or Inkscape)
# Plain paths only (no clip paths, no filters), so every SVG viewer and file manager shows it the same.
import os, sys

ORANGE, SEED, INK, CLOUD, WHITE = "#FF7A1A", "#FFC145", "#1B1F2A", "#F5F3EF", "#FFFFFF"

# the app icon's hexagon: pointy top, rounded corners
HEX = ("M226.6 37.0 Q256.0 20.0 285.4 37.0 L430.9 121.0 Q460.4 138.0 460.4 172.0 L460.4 340.0 Q460.4 374.0 430.9 391.0 "
       "L285.4 475.0 Q256.0 492.0 226.6 475.0 L81.1 391.0 Q51.6 374.0 51.6 340.0 L51.6 172.0 Q51.6 138.0 81.1 121.0 Z")

# the f, exactly as the wordmark draws it (stroke 30, round ends), in the wordmark's own units
STROKE = 30
F_STEM = "M60 230 L60 112 C60 84 78 70 104 70"
F_BAR = "M32 142 L98 142"
DOT_R = 19                      # the same size as the dot over the i in the wordmark
# right after the end of the f's curl (x 104, plus its round end), half a stroke apart, with their tops lined up
DOT = (104 + STROKE / 2 + STROKE / 2 + DOT_R, 70 - STROKE / 2 + DOT_R)
# the box the f and the dot fill together, so they can be centred exactly
BOX = (32 - STROKE / 2, 70 - STROKE / 2, DOT[0] + DOT_R, 230 + STROKE / 2)


def glyph(f_color, dot_color, cx=256, cy=256, scale=1.45):
    """the f and its dot, scaled, with the middle of their box at (cx, cy)"""
    bx = (BOX[0] + BOX[2]) / 2
    by = (BOX[1] + BOX[3]) / 2
    return (f'  <g transform="translate({cx} {cy}) scale({scale}) translate({-bx:.2f} {-by:.2f})">\n'
            f'    <path d="{F_STEM} {F_BAR}" fill="none" stroke="{f_color}" stroke-width="{STROKE}" '
            f'stroke-linecap="round" stroke-linejoin="round"/>\n'
            f'    <circle cx="{DOT[0]:.1f}" cy="{DOT[1]:.1f}" r="{DOT_R}" fill="{dot_color}"/>\n'
            f'  </g>\n')


def icon_body():
    """the app icon: a white f and a Seed dot on the orange hexagon"""
    return f'  <path d="{HEX}" fill="{ORANGE}"/>\n' + glyph(WHITE, SEED)


def doc(body, title="Finch"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">\n'
            f'  <title>{title}</title>\n' + body + '</svg>\n')


# ---- the logo: the icon next to the wordmark ----
# the wordmark, drawn with strokes (stroke 30, round ends); in its own units it spans x 17..585, y 55..245
WORDMARK = ["M60 230 L60 112 C60 84 78 70 104 70", "M32 142 L98 142", "M150 146 L150 230",
            "M210 230 L210 146 M210 188 C210 162 230 142 258 142 C286 142 302 160 302 188 L302 230",
            "M428 162 C414 148 400 142 384 142 C356 142 336 162 336 187 C336 212 356 232 384 232 C400 232 414 226 428 212",
            "M478 230 L478 70 M478 188 C478 162 498 142 526 142 C554 142 570 160 570 188 L570 230"]
I_DOT = (150, 96, 19)
TEXT_BOX = (17, 55, 585, 245)
# the hexagon is as tall as the lettering and shares its middle line; the gap is about two letter spaces
HEX_BOX = (51.6, 20.0, 460.4, 492.0)
GAP = 48
SCALE = (TEXT_BOX[3] - TEXT_BOX[1]) / (HEX_BOX[3] - HEX_BOX[1])
HEX_W = (HEX_BOX[2] - HEX_BOX[0]) * SCALE
LEFT = TEXT_BOX[0] - GAP - HEX_W                         # where the logo starts, in the wordmark's units


def lockup(text_color, dy=0):
    """the icon and the wordmark side by side, in the wordmark's units (moved down by dy)"""
    tx = LEFT - HEX_BOX[0] * SCALE
    ty = (TEXT_BOX[1] + TEXT_BOX[3]) / 2 - 256 * SCALE + dy
    paths = "\n".join(f'      <path d="{d}"/>' for d in WORDMARK)
    x, y, r = I_DOT
    return (f'  <g transform="translate({tx:.2f} {ty:.2f}) scale({SCALE:.5f})">\n' + icon_body() + '  </g>\n'
            f'  <g transform="translate(0 {dy})">\n'
            f'    <g fill="none" stroke="{text_color}" stroke-width="30" stroke-linecap="round" stroke-linejoin="round">\n'
            f'{paths}\n    </g>\n'
            f'    <circle cx="{x}" cy="{y}" r="{r}" fill="{ORANGE}"/>\n  </g>\n')


def logo(text_color, pad=24):
    """the logo file: the lockup with even space around it"""
    w = TEXT_BOX[2] - LEFT + 2 * pad
    h = TEXT_BOX[3] - TEXT_BOX[1] + 2 * pad
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" width="{w:.0f}" height="{h:.0f}">\n'
            f'  <title>Finch</title>\n'
            f'  <g transform="translate({pad - LEFT:.2f} {pad - TEXT_BOX[1]})">\n' + lockup(text_color) + '  </g>\n</svg>\n')


def social(path, text_color=CLOUD):
    """the logo row of the social preview: its left edge lined up with the text under it (x 70)"""
    text = open(path).read()
    start = text.index('  <g transform="translate(')            # the logo row is the first group after the background
    end = text.index('  <text ')
    row = (f'  <g transform="translate({70 - 0.6 * LEFT:.2f} 110) scale(0.6)">\n' + lockup(text_color, dy=20) + '  </g>\n')
    open(path, 'w').write(text[:start] + row + text[end:])


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write = lambda name, text: open(os.path.join(here, name), "w").write(text)
    write("icon.svg", doc(icon_body()))
    write("logo-mark.svg", doc(glyph(INK, ORANGE, scale=2.0)))           # for light backgrounds
    write("logo-mark-dark.svg", doc(glyph(CLOUD, ORANGE, scale=2.0)))    # for dark backgrounds
    vscode = os.path.join(here, "..", "editors", "vscode", "images")
    if os.path.isdir(vscode):
        for name in ["file-light.svg", "file-dark.svg"]:
            open(os.path.join(vscode, name), "w").write(doc(icon_body()))
    write("logo.svg", logo(INK))
    write("logo-dark.svg", logo(CLOUD))
    social(os.path.join(here, "social-preview.svg"))
    if "--png" not in sys.argv:
        print("the mark is in every SVG; render the PNGs with: python3 assets/make_logo.py --png")
        sys.exit(0)
    import shutil, subprocess
    sizes = [("icon.svg", "icon.png", 512, 512), ("icon.svg", "icon-256.png", 256, 256), ("icon.svg", "favicon-64.png", 64, 64),
             ("logo.svg", "logo.png", 1000, 0), ("logo-dark.svg", "logo-dark.png", 1000, 0),
             ("social-preview.svg", "social-preview.png", 1280, 640)]
    if os.path.isdir(vscode):
        sizes.append(("icon.svg", os.path.join(vscode, "icon.png"), 128, 128))
    for src, out, w, h in sizes:
        src = os.path.join(here, src)
        out = os.path.join(here, out)
        if shutil.which("rsvg-convert"):
            size = ["-w", str(w)] + (["-h", str(h)] if h else [])     # h 0: keep the drawing's proportions
            subprocess.run(["rsvg-convert"] + size + [src, "-o", out], check=True)
        elif shutil.which("inkscape"):
            size = ["-w", str(w)] + (["-h", str(h)] if h else [])
            subprocess.run(["inkscape", src] + size + ["-o", out], check=True)
        else:
            sys.exit("no rsvg-convert or inkscape to render the PNGs with (on Nix: nix-shell -p librsvg)")
    print("the mark is in every SVG and PNG")
