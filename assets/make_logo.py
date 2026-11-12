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


def swap_mark(path):
    """put the app icon into a logo, left of the wordmark, where the old mark was"""
    text = open(path).read()
    start = text.index('<g transform="translate(0 6) scale(0.56)">')
    end = text.index('<g transform="translate(330 20)">')
    new = '<g transform="translate(0 6) scale(0.56)">\n' + icon_body() + '  </g>\n  '
    open(path, 'w').write(text[:start] + new + text[end:])


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
    for name in ["logo.svg", "logo-dark.svg", "social-preview.svg"]:
        swap_mark(os.path.join(here, name))
    if "--png" not in sys.argv:
        print("the mark is in every SVG; render the PNGs with: python3 assets/make_logo.py --png")
        sys.exit(0)
    import shutil, subprocess
    sizes = [("icon.svg", "icon.png", 512, 512), ("icon.svg", "icon-256.png", 256, 256), ("icon.svg", "favicon-64.png", 64, 64),
             ("logo.svg", "logo.png", 1000, 300), ("logo-dark.svg", "logo-dark.png", 1000, 300),
             ("social-preview.svg", "social-preview.png", 1280, 640)]
    if os.path.isdir(vscode):
        sizes.append(("icon.svg", os.path.join(vscode, "icon.png"), 128, 128))
    for src, out, w, h in sizes:
        src = os.path.join(here, src)
        out = os.path.join(here, out)
        if shutil.which("rsvg-convert"):
            subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), src, "-o", out], check=True)
        elif shutil.which("inkscape"):
            subprocess.run(["inkscape", src, "-w", str(w), "-h", str(h), "-o", out], check=True)
        else:
            sys.exit("no rsvg-convert or inkscape to render the PNGs with (on Nix: nix-shell -p librsvg)")
    print("the mark is in every SVG and PNG")
