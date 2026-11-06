#!/usr/bin/env python3
"""Generuje ikony CynamonCode (16–512 px) z logo źródłowego (D033).

Narzędzie dla deweloperów — nie jest częścią budowania. Wymaga Pillow:
    sudo apt install python3-pil      (Linux Mint / Ubuntu)

Użycie (z katalogu repozytorium):
    python3 scripts/generate-icons.py
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "resources" / "icons" / "cynamoncode-zrodlo.png"
TARGET = ROOT / "resources" / "icons"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def main() -> None:
    image = Image.open(SOURCE).convert("RGBA")
    if image.width != image.height:
        raise SystemExit(f"Logo musi być kwadratowe, a ma {image.width}×{image.height} px.")
    for size in SIZES:
        scaled = image.resize((size, size), Image.Resampling.LANCZOS)
        out = TARGET / f"cynamoncode-{size}.png"
        scaled.save(out, optimize=True)
        print(f"{out.relative_to(ROOT)}: {out.stat().st_size} B")


if __name__ == "__main__":
    main()
