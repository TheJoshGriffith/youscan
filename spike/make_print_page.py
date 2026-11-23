#!/usr/bin/env python3
"""Lay the ChArUco board on an A4 landscape page at true size, with a 100 mm check bar.

Print with "Actual size" / 100% and NO "fit to page". Then measure the check bar:
it must be exactly 100.0 mm. If it isn't, the print is scaled and the board is wrong.
"""
import argparse
import json

from PIL import Image, ImageDraw, ImageFont

DPI = 600
MM_PER_IN = 25.4
A4_LANDSCAPE_MM = (297.0, 210.0)


def mm_px(mm):
    return round(mm * DPI / MM_PER_IN)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--board", required=True, help="charuco_board.png from make_charuco.py")
    p.add_argument("--meta", required=True, help="board_meta.json")
    p.add_argument("--out", required=True, help="output PDF")
    a = p.parse_args()

    meta = json.load(open(a.meta))
    board = Image.open(a.board).convert("L")
    bw_mm = meta["board_width_mm"]
    bh_mm = meta["board_height_mm"]

    page_w, page_h = mm_px(A4_LANDSCAPE_MM[0]), mm_px(A4_LANDSCAPE_MM[1])
    page = Image.new("L", (page_w, page_h), 255)

    # Centre the board. Its top edge sits 25 mm down to leave room for the header.
    x0 = (page_w - mm_px(bw_mm)) // 2
    y0 = mm_px(25)
    board_px = board.resize((mm_px(bw_mm), mm_px(bh_mm)), Image.Resampling.LANCZOS)
    page.paste(board_px, (x0, y0))

    draw = ImageDraw.Draw(page)
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", mm_px(4))
        small = ImageFont.truetype("DejaVuSans.ttf", mm_px(2.5))
    except OSError:
        font = small = ImageFont.load_default()

    draw.text((mm_px(10), mm_px(8)),
              f"YouScan ChArUco board  {meta['squares_x']}x{meta['squares_y']} squares, "
              f"{meta['square_mm']:g} mm nominal  -  PRINT AT ACTUAL SIZE (100%), NO FIT TO PAGE",
              fill=0, font=font)

    # 100 mm check bar, below the board, with end ticks.
    bar_y = y0 + mm_px(bh_mm) + mm_px(14)
    bar_x = mm_px(10)
    bar_len = mm_px(100)
    tick = mm_px(2)
    draw.line([(bar_x, bar_y), (bar_x + bar_len, bar_y)], fill=0, width=mm_px(0.3))
    draw.line([(bar_x, bar_y - tick), (bar_x, bar_y + tick)], fill=0, width=mm_px(0.3))
    draw.line([(bar_x + bar_len, bar_y - tick), (bar_x + bar_len, bar_y + tick)], fill=0, width=mm_px(0.3))
    draw.text((bar_x, bar_y + mm_px(3)),
              "Check: this bar must measure exactly 100.0 mm with calipers",
              fill=0, font=small)

    page.save(a.out, "PDF", resolution=DPI)
    print(f"wrote {a.out}: A4 landscape, board {bw_mm:g}x{bh_mm:g} mm, check bar 100 mm")


if __name__ == "__main__":
    main()
