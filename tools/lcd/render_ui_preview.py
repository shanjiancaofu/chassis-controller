#!/usr/bin/env python3
"""Render the STM32 LCD pages using the same 5x7 glyphs and layout rules.

The output is a review aid, not a firmware renderer. It intentionally uses the
same 320x240 canvas, RGB565 colors, logo bytes, glyph table, coordinates and
sample status values as bsp_lcd.c.
"""

from __future__ import annotations

import re
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
LCD_C = ROOT / "firmware/application/stm32g474/bsp/lcd/bsp_lcd.c"
LOGO_H = ROOT / "firmware/application/stm32g474/bsp/lcd/assets/tafei/picture_tafei_logo.h"
OUT = ROOT / "docs/lcd_previews"
W, H = 320, 240

COLORS = {
    "background": 0x0882,
    "panel": 0x10C4,
    "panel_alt": 0x1905,
    "header": 0x0882,
    "divider": 0x29A7,
    "accent": 0x1E5A,
    "highlight": 0xEF9E,
    "text": 0xEF9E,
    "muted": 0x8CB4,
    "weak": 0x530D,
    "pass": 0x468F,
    "fail": 0xF2CC,
    "ready": 0xF5C8,
    "disabled": 0x530D,
}


def rgb565(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 11) & 0x1F) * 255 // 31,
        ((value >> 5) & 0x3F) * 255 // 63,
        (value & 0x1F) * 255 // 31,
    )


def parse_numbers(text: str) -> list[int]:
    return [int(token, 0) for token in re.findall(r"0[xX][0-9a-fA-F]+|\d+", text)]


def parse_glyphs() -> dict[str, list[int]]:
    text = LCD_C.read_text()
    glyphs: dict[str, list[int]] = {}
    pattern = re.compile(r"\{'(.|\\.)',\s*\{([^}]*)\}\}")
    for match in pattern.finditer(text):
        character = match.group(1)
        if character == "\\'":
            character = "'"
        glyphs[character] = parse_numbers(match.group(2))
    glyphs[" "] = [0, 0, 0, 0, 0]
    return glyphs


def parse_logo() -> tuple[list[int], list[int]]:
    text = LOGO_H.read_text()
    data_match = re.search(
        r"gImage_tafei_logo\[[^]]*\]\s*=\s*\{(.*?)\};", text, re.S
    )
    mask_match = re.search(
        r"gImage_tafei_logo_mask\[[^]]*\]\s*=\s*\{(.*?)\};", text, re.S
    )
    if data_match is None or mask_match is None:
        raise RuntimeError("logo arrays not found")
    return parse_numbers(data_match.group(1)), parse_numbers(mask_match.group(1))


GLYPHS = parse_glyphs()
LOGO_DATA, LOGO_MASK = parse_logo()


def draw_text(image: Image.Image, text: str, x: int, y: int, scale: int, color: int) -> None:
    pixels = image.load()
    rgb = rgb565(color)
    for char in text:
        glyph = GLYPHS.get(char, [0, 0, 0, 0, 0])
        for column, bits in enumerate(glyph):
            for row in range(7):
                if bits & (1 << row):
                    for dx in range(scale):
                        for dy in range(scale):
                            px = x + column * scale + dx
                            py = y + row * scale + dy
                            if 0 <= px < W and 0 <= py < H:
                                pixels[px, py] = rgb
        x += 6 * scale


def draw_logo(image: Image.Image) -> None:
    pixels = image.load()
    for row in range(32):
        for column in range(32):
            source_row = row * 40 // 32
            source_column = column * 40 // 32
            index = source_row * 40 + source_column
            if LOGO_MASK[index] == 0:
                continue
            hi = LOGO_DATA[index * 2]
            lo = LOGO_DATA[index * 2 + 1]
            pixels[W - 40 + column, 2 + row] = rgb565((hi << 8) | lo)


def background(image: Image.Image, page: int) -> None:
    pixels = image.load()
    for row in range(H):
        for column in range(W):
            if row < 36:
                color = COLORS["accent"] if 34 <= row < 36 and 12 <= column < 92 else COLORS["header"]
            elif row in (36, 211):
                color = COLORS["divider"]
            elif 40 <= row < 110:
                color = COLORS["panel"]
            elif 116 <= row < 195:
                color = COLORS["panel_alt"]
            elif row >= 216:
                color = COLORS["header"]
            else:
                color = COLORS["background"]
            pixels[column, row] = rgb565(color)


def page_lines(page: int) -> list[tuple[str, int, int, int, int]]:
    # Text, x, y, scale, RGB565 color. Values mirror a live stopped board.
    if page == 0:
        return [
            ("OVERVIEW", 12, 8, 2, COLORS["accent"]),
            ("01/04", 190, 9, 1, COLORS["muted"]),
            ("POWER", 12, 45, 1, COLORS["muted"]),
            ("12.52V", 12, 57, 3, COLORS["text"]),
            ("93%", 236, 61, 2, COLORS["accent"]),
            ("CONTROL", 12, 122, 1, COLORS["muted"]),
            ("STOPPED", 12, 143, 1, COLORS["ready"]),
            ("CAN", 170, 124, 1, COLORS["muted"]),
            ("PASS", 232, 124, 1, COLORS["pass"]),
            ("QSPI", 170, 143, 1, COLORS["muted"]),
            ("PASS", 232, 143, 1, COLORS["pass"]),
            ("IMU", 170, 162, 1, COLORS["muted"]),
            ("READY", 232, 162, 1, COLORS["pass"]),
            ("FAULTS", 12, 178, 1, COLORS["weak"]),
            ("0", 96, 178, 1, COLORS["weak"]),
            ("FW V0.14.0 B1", 12, 220, 1, COLORS["muted"]),
        ]
    if page == 1:
        return [
            ("MOTOR", 12, 8, 2, COLORS["accent"]),
            ("02/04", 190, 9, 1, COLORS["muted"]),
            ("LEFT", 12, 45, 1, COLORS["muted"]),
            ("0", 72, 57, 3, COLORS["text"]),
            ("RPM", 108, 79, 1, COLORS["muted"]),
            ("RIGHT", 170, 45, 1, COLORS["muted"]),
            ("0", 230, 57, 3, COLORS["text"]),
            ("RPM", 266, 79, 1, COLORS["muted"]),
            ("TARGET 0", 12, 120, 1, COLORS["text"]),
            ("PWM 0", 12, 136, 1, COLORS["text"]),
            ("ENC 0", 12, 152, 1, COLORS["text"]),
            ("TARGET 0", 170, 120, 1, COLORS["text"]),
            ("PWM 0", 170, 136, 1, COLORS["text"]),
            ("ENC 0", 170, 152, 1, COLORS["text"]),
            ("POSE", 12, 174, 1, COLORS["muted"]),
            ("X0 Y0 H0", 12, 188, 1, COLORS["weak"]),
            ("CONTROL", 190, 174, 1, COLORS["muted"]),
            ("STOPPED", 190, 188, 1, COLORS["ready"]),
            ("FW V0.14.0 B1", 12, 220, 1, COLORS["muted"]),
        ]
    if page == 2:
        return [
            ("SENSORS", 12, 8, 2, COLORS["accent"]),
            ("03/04", 190, 9, 1, COLORS["muted"]),
            ("IMU", 12, 45, 1, COLORS["muted"]),
            ("READY", 12, 60, 1, COLORS["pass"]),
            ("RPY N/A", 12, 80, 1, COLORS["ready"]),
            ("SR501", 170, 45, 1, COLORS["muted"]),
            ("READY", 170, 60, 1, COLORS["pass"]),
            ("MOTION NO", 170, 80, 1, COLORS["text"]),
            ("EVENTS 0", 170, 96, 1, COLORS["text"]),
            ("POWER", 12, 124, 1, COLORS["muted"]),
            ("12.52V", 12, 142, 2, COLORS["text"]),
            ("RTC", 170, 124, 1, COLORS["muted"]),
            ("03:35:20", 170, 142, 2, COLORS["text"]),
            ("FW V0.14.0 B1", 12, 220, 1, COLORS["muted"]),
        ]
    return [
        ("SYSTEM", 12, 8, 2, COLORS["accent"]),
        ("04/04", 190, 9, 1, COLORS["muted"]),
            ("HEALTH", 12, 44, 1, COLORS["muted"]), ("OK", 90, 44, 1, COLORS["pass"]),
            ("STORAGE", 170, 44, 1, COLORS["muted"]), ("QSPI 8M", 170, 61, 1, COLORS["text"]),
            ("PASS", 260, 61, 1, COLORS["pass"]),
            ("TASKS", 12, 82, 1, COLORS["muted"]),
            ("SERVICE", 12, 98, 1, COLORS["muted"]), ("RUN", 104, 98, 1, COLORS["pass"]),
            ("CONTROL", 12, 114, 1, COLORS["muted"]), ("RUN", 104, 114, 1, COLORS["pass"]),
            ("DIAG", 170, 98, 1, COLORS["muted"]), ("RUN", 250, 98, 1, COLORS["pass"]),
            ("DISPLAY", 170, 114, 1, COLORS["muted"]), ("RUN", 250, 114, 1, COLORS["pass"]),
            ("STACK FREE", 12, 140, 1, COLORS["muted"]),
            ("SVC 388 CTRL 440", 12, 155, 1, COLORS["text"]),
            ("DIAG 490 LCD 320", 170, 155, 1, COLORS["text"]),
            ("UPTIME", 12, 174, 1, COLORS["muted"]),
            ("00:02:28", 12, 190, 1, COLORS["text"]),
            ("RESET", 190, 174, 1, COLORS["muted"]),
            ("SW", 190, 190, 1, COLORS["text"]),
        ("FW V0.14.0 B1", 12, 220, 1, COLORS["muted"]),
    ]


def render(page: int) -> Image.Image:
    image = Image.new("RGB", (W, H))
    background(image, page)
    draw_logo(image)
    # Page indicator is deliberately drawn after the logo, matching the BSP.
    for selected in range(4):
        color = COLORS["accent"] if selected == page else COLORS["divider"]
        ImageDraw.Draw(image).rectangle(
            (222 + selected * 12, 26, 229 + selected * 12, 28), fill=rgb565(color)
        )
    draw = ImageDraw.Draw(image)
    if page == 0:
        draw.line((159, 116, 159, 194), fill=rgb565(COLORS["divider"]))
    elif page in (1, 2):
        draw.line((159, 40, 159, 194), fill=rgb565(COLORS["divider"]))
    else:
        draw.line((159, 40, 159, 137), fill=rgb565(COLORS["divider"]))
    if page == 0:
        x, y, width, height, percent = 12, 88, 288, 16, 93
        draw.rectangle((x, y, x + width, y + height), outline=rgb565(COLORS["muted"]))
        fill = (width - 4) * percent // 100
        draw.rectangle((x + 2, y + 2, x + 2 + fill, y + height - 2), fill=rgb565(COLORS["accent"]))
        draw.rectangle((x + width + 1, y + 4, x + width + 4, y + height - 4), fill=rgb565(COLORS["muted"]))
    for text, x, y, scale, color in page_lines(page):
        draw_text(image, text, x, y, scale, color)
    return image


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    names = ("overview", "motor", "sensors", "system")
    pages = [render(page) for page in range(4)]
    for name, image in zip(names, pages):
        image.resize((640, 480), Image.NEAREST).save(OUT / f"{name}.png")
    contact = Image.new("RGB", (640, 1920), "#101010")
    for index, image in enumerate(pages):
        contact.paste(image.resize((640, 480), Image.NEAREST), (0, index * 480))
    contact.save(OUT / "all_pages.png")
    print(f"wrote {len(pages) + 1} images to {OUT}")


if __name__ == "__main__":
    main()
