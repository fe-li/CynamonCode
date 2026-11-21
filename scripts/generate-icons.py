#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski
"""Generuje ikony CynamonCode z logo źródłowego.

PNG 16–512 px (zasoby Qt, Linux), cynamoncode.ico (Windows) i cynamoncode.icns
(macOS). Narzędzie dla deweloperów — nie jest częścią budowania. Wymaga Pillow:
    sudo apt install python3-pil      (Linux Mint / Ubuntu)
    python -m pip install pillow      (Windows, macOS)

Użycie (z katalogu repozytorium):
    python3 scripts/generate-icons.py                 # wszystkie formaty
    python3 scripts/generate-icons.py --formats ico icns
"""
import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "resources" / "icons" / "cynamoncode-zrodlo.png"
TARGET = ROOT / "resources" / "icons"
PNG_SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def report(path: Path) -> None:
    print(f"{path.relative_to(ROOT)}: {path.stat().st_size} B")


def write_png(image: Image.Image) -> None:
    for size in PNG_SIZES:
        out = TARGET / f"cynamoncode-{size}.png"
        image.resize((size, size), Image.Resampling.LANCZOS).save(out, optimize=True)
        report(out)


def write_ico(image: Image.Image) -> None:
    out = TARGET / "cynamoncode.ico"
    image.resize((256, 256), Image.Resampling.LANCZOS).save(
        out, format="ICO", sizes=[(size, size) for size in ICO_SIZES])
    report(out)


def write_icns(image: Image.Image) -> None:
    # Pillow tworzy z obrazu 1024 px wszystkie rozmiary ikony macOS (16–1024, także @2x).
    out = TARGET / "cynamoncode.icns"
    image.resize((1024, 1024), Image.Resampling.LANCZOS).save(out, format="ICNS")
    report(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--formats", nargs="+", choices=("png", "ico", "icns"),
                        default=["png", "ico", "icns"])
    args = parser.parse_args()

    image = Image.open(SOURCE).convert("RGBA")
    if image.width != image.height:
        raise SystemExit(f"Logo musi być kwadratowe, a ma {image.width}×{image.height} px.")
    writers = {"png": write_png, "ico": write_ico, "icns": write_icns}
    for name in args.formats:
        writers[name](image)


if __name__ == "__main__":
    main()
