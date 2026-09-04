"""nib · theme_detect — read a theme out of an image.

Point it at a screenshot and it writes `nib.theme`, which nib reads at startup. Colours are data,
not an opinion baked into the binary.

    python tools/theme_detect.py shot.png
    python tools/theme_detect.py shot.png --out C:/nib/nib.theme

What it finds:
  background  the modal colour — by far the most common pixel
  foreground  the most common colour far from the background: body text
  dim         the most common colour between the two: secondary text, captions
  accent      the most saturated colour present in quantity
  selection   the background lifted toward the accent
"""
import argparse
from collections import Counter

from PIL import Image


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def sat(c):
    return max(c) - min(c)


def dist(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])


def hexs(c):
    return "#%02x%02x%02x" % tuple(c)


ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--out", default=r"C:/nib/nib.theme")
ap.add_argument("--font", default="Consolas")
ap.add_argument("--pt", type=int, default=11)
a = ap.parse_args()

img = Image.open(a.image).convert("RGB")
counts = Counter(img.getdata())
total = sum(counts.values())
common = counts.most_common(600)

bg = common[0][0]
fg = max((c for c, n in common if dist(c, bg) > 120),
         key=lambda c: counts[c] * (1 + abs(lum(c) - lum(bg)) / 255.0), default=(200, 200, 200))
mid = [c for c, n in common if 40 < dist(c, bg) <= 120 and dist(c, fg) > 40]
dim = max(mid, key=lambda c: counts[c], default=None)
if dim is None:   # nothing between them: sit the dim tone halfway
    dim = tuple(int(bg[i] + (fg[i] - bg[i]) * 0.45) for i in range(3))
accent = max((c for c, n in common if sat(c) > 60 and n > total * 0.0002),
             key=lambda c: sat(c) * counts[c] ** 0.25, default=fg)
selection = tuple(min(255, int(bg[i] + accent[i] * 0.22)) for i in range(3))

lines = [
    "# nib.theme - written by tools/theme_detect.py from %s" % a.image,
    "# Colours are data. Edit freely; nib reads this file at startup and falls back to its own",
    "# defaults for anything missing or unparsable.",
    "background %s" % hexs(bg),
    "foreground %s" % hexs(fg),
    "dim        %s" % hexs(dim),
    "accent     %s" % hexs(accent),
    "selection  %s" % hexs(selection),
    "font       %s" % a.font,
    "font_pt    %d" % a.pt,
]
open(a.out, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
print("\n".join(lines))
print("\n%d pixels, %d distinct; the background is %.0f%% of them"
      % (total, len(counts), counts[bg] * 100.0 / total))
print("written to %s" % a.out)
