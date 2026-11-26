"""Packs the 3x launch images into one PDF: python3 marketing/launch/make-pdf.py

Page 1 is the feed post (4:5), page 2 the Story (9:16), each at its full 3240 px
width. img2pdf embeds the PNG data as is, so nothing is recompressed (Pillow's
own PDF writer would turn the pages into JPEGs). Needs: pip install img2pdf
"""
from pathlib import Path

import img2pdf

here = Path(__file__).parent / "hires"
images = [str(here / n) for n in ("sledge-launch-feed@3x.png", "sledge-launch-story@3x.png")]
out = here / "sledge-launch-post.pdf"
# 324 dpi puts the 3240 px width on a 10 inch page.
layout = img2pdf.get_fixed_dpi_layout_fun((324, 324))
out.write_bytes(img2pdf.convert(images, layout_fun=layout))
print(out)
