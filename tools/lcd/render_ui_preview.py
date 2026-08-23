#!/usr/bin/env python3
"""Render previews with the firmware's C LCD UI renderer."""

from __future__ import annotations

import pathlib
import re
import subprocess
import tempfile

from PIL import Image


ROOT = pathlib.Path(__file__).resolve().parents[2]
APP_ROOT = ROOT / "firmware/application/stm32g474"
UI_C = APP_ROOT / "ui/lcd/lcd_ui.c"
LAYOUT_H = APP_ROOT / "ui/lcd/lcd_ui_layout.h"
HOST_C = ROOT / "tools/lcd/lcd_ui_preview_host.c"
OUT = ROOT / "docs/lcd_previews"


def numeric_macro(name: str) -> int:
    source = LAYOUT_H.read_text(encoding="ascii")
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|\d+)[UuLl]*\s*$",
        source,
        re.MULTILINE,
    )
    if match is None:
        raise RuntimeError(f"missing numeric layout macro: {name}")
    return int(match.group(1), 0)


WIDTH = numeric_macro("LCD_UI_WIDTH")
HEIGHT = numeric_macro("LCD_UI_HEIGHT")
NEAREST = getattr(Image, "Resampling", Image).NEAREST


def build_renderer(output: pathlib.Path) -> None:
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DLCD_UI_HOST_PREVIEW",
            f"-I{APP_ROOT}",
            f"-I{APP_ROOT / 'include'}",
            str(UI_C),
            str(HOST_C),
            "-o",
            str(output),
        ],
        check=True,
    )


def load_rgb565(path: pathlib.Path) -> Image.Image:
    raw = path.read_bytes()
    expected_size = WIDTH * HEIGHT * 2
    if len(raw) != expected_size:
        raise RuntimeError(
            f"{path.name}: expected {expected_size} bytes, got {len(raw)}"
        )

    rgb = bytearray(WIDTH * HEIGHT * 3)
    for source, target in zip(range(0, len(raw), 2), range(0, len(rgb), 3)):
        value = (raw[source] << 8) | raw[source + 1]
        rgb[target] = ((value >> 11) & 0x1F) * 255 // 31
        rgb[target + 1] = ((value >> 5) & 0x3F) * 255 // 63
        rgb[target + 2] = (value & 0x1F) * 255 // 31
    return Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(rgb))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    names = ("overview", "motor", "sensors", "system")
    with tempfile.TemporaryDirectory(prefix="lcd-ui-preview-") as directory:
        temporary = pathlib.Path(directory)
        renderer = temporary / "lcd_ui_preview"
        build_renderer(renderer)
        subprocess.run([str(renderer), str(temporary)], check=True)
        pages = [load_rgb565(temporary / f"{name}.rgb565") for name in names]

    for name, page in zip(names, pages):
        page.resize((WIDTH * 2, HEIGHT * 2), NEAREST).save(
            OUT / f"{name}.png"
        )
    all_pages = Image.new("RGB", (WIDTH * 2, HEIGHT * 2 * len(pages)))
    for index, page in enumerate(pages):
        all_pages.paste(
            page.resize((WIDTH * 2, HEIGHT * 2), NEAREST),
            (0, index * HEIGHT * 2),
        )
    all_pages.save(OUT / "all_pages.png")
    print(f"wrote {len(pages) + 1} images to {OUT} using {UI_C.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
