#!/usr/bin/env python3
"""Combine reference-rendered screen PPMs; requires Pillow."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1] / "preview"
for path in sorted(root.glob("coast_*_top.ppm")):
    i = int(path.stem.split("_")[1])
    top = Image.open(root / f"coast_{i}_top.ppm")
    bottom = Image.open(root / f"coast_{i}_bottom.ppm")
    canvas = Image.new("RGB", (400, 492), "#080d18")
    canvas.paste(top, (0, 0))
    canvas.paste(bottom, (40, 252))
    canvas.resize((800, 984), Image.Resampling.NEAREST).save(root / f"coast_{i}.png")
