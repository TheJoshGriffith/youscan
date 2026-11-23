#!/usr/bin/env python3
"""Generate a ChArUco board at an exact physical size.

Print the PNG at 100% / "Actual size" with no page scaling, then measure
several squares with calipers and record the measured value in board_meta.json
(field "measured_square_mm") if it differs from the nominal value. The
evaluation uses square_mm from the meta file, so keep the two in sync.
"""
import argparse
import json

import numpy as np
from PIL import Image

from common import ARUCO_DICT, make_board
import cv2


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out-dir", required=True)
    p.add_argument("--squares-x", type=int, default=7)
    p.add_argument("--squares-y", type=int, default=5)
    p.add_argument("--square-mm", type=float, default=30.0)
    p.add_argument("--marker-mm", type=float, default=22.0)
    p.add_argument("--dpi", type=int, default=600)
    a = p.parse_args()

    import os
    os.makedirs(a.out_dir, exist_ok=True)

    meta = {
        "squares_x": a.squares_x,
        "squares_y": a.squares_y,
        "square_mm": a.square_mm,
        "marker_mm": a.marker_mm,
        "dictionary": "DICT_5X5_1000",
        "dpi": a.dpi,
    }
    board = make_board(meta)

    px_per_mm = a.dpi / 25.4
    w = round(a.squares_x * a.square_mm * px_per_mm)
    h = round(a.squares_y * a.square_mm * px_per_mm)
    img = board.generateImage((w, h), marginSize=0, borderBits=1)

    png = os.path.join(a.out_dir, "charuco_board.png")
    Image.fromarray(img).save(png, dpi=(a.dpi, a.dpi))

    meta["image_px"] = [w, h]
    meta["board_width_mm"] = a.squares_x * a.square_mm
    meta["board_height_mm"] = a.squares_y * a.square_mm
    with open(os.path.join(a.out_dir, "board_meta.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"wrote {png} ({w}x{h}px) board {meta['board_width_mm']}x{meta['board_height_mm']} mm")
    print("Print at 100% / actual size, then measure squares with calipers.")


if __name__ == "__main__":
    main()
