#!/usr/bin/env python3
"""Render synthetic ChArUco board photos with known ground-truth poses.

Purpose: test the pipeline plumbing (detection, calibration, SfM, board
evaluation) without a phone. These images are NOT a measure of real-world
accuracy: there is no lens distortion, no focus blur, and no sensor noise
beyond a little blur and Gaussian noise. Synthetic error should be near zero;
if it isn't, the math is wrong.
"""
import argparse
import json
import os

import cv2
import numpy as np

from common import load_meta, make_board

IMG_W, IMG_H = 4032, 3024
FX = FY = 3000.0
CX, CY = IMG_W / 2, IMG_H / 2


def random_rotation(rng, max_deg):
    axis = rng.normal(size=3)
    axis /= np.linalg.norm(axis)
    angle = np.deg2rad(rng.uniform(-max_deg, max_deg))
    rvec = axis * angle
    R, _ = cv2.Rodrigues(rvec)
    return R


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--meta", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--views", type=int, default=30)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--px-per-mm", type=float, default=12.0)
    a = p.parse_args()

    meta = load_meta(a.meta)
    board = make_board(meta)
    os.makedirs(a.out_dir, exist_ok=True)
    rng = np.random.default_rng(a.seed)

    # Render the board texture at high resolution, board coordinates = mm * px_per_mm.
    s = a.px_per_mm
    tex_w = round(meta["board_width_mm"] * s)
    tex_h = round(meta["board_height_mm"] * s)
    tex = board.generateImage((tex_w, tex_h), marginSize=0, borderBits=1)

    K = np.array([[FX, 0, CX], [0, FY, CY], [0, 0, 1.0]])
    board_center = np.array([meta["board_width_mm"] / 2, meta["board_height_mm"] / 2, 0.0])
    # Nominal pose: camera on the -z side looking along +z at the board, so
    # image x = board +x and image y = board +y (texture rows run down the image).
    # Using a det=+1 rotation keeps the board unmirrored.
    R0 = np.eye(3)

    gt = {"K": K.tolist(), "image_size": [IMG_W, IMG_H], "views": []}
    for i in range(a.views):
        dist_mm = rng.uniform(250, 400)
        lateral = rng.uniform(-50, 50, size=2)
        C = board_center + np.array([lateral[0], lateral[1], -dist_mm])
        R = random_rotation(rng, 20) @ R0
        t = -R @ C

        # Planar homography for Z=0 board points given in mm.
        H_mm = K @ np.column_stack([R[:, 0], R[:, 1], t])
        H_px = H_mm @ np.diag([1 / s, 1 / s, 1.0])

        warped = cv2.warpPerspective(
            tex, H_px, (IMG_W, IMG_H), flags=cv2.INTER_LINEAR, borderValue=255
        )
        warped = cv2.GaussianBlur(warped, (0, 0), 0.7).astype(np.float32)
        warped += rng.normal(0, 3.0, size=warped.shape).astype(np.float32)
        img = np.clip(warped, 0, 255).astype(np.uint8)

        name = f"view_{i:03d}.jpg"
        cv2.imwrite(os.path.join(a.out_dir, name), img, [cv2.IMWRITE_JPEG_QUALITY, 95])
        gt["views"].append({"name": name, "R": R.tolist(), "t": t.tolist()})

    # Keep ground truth outside the image folder so COLMAP doesn't try to read it.
    with open(a.out_dir.rstrip("/") + "_ground_truth.json", "w") as f:
        json.dump(gt, f, indent=2)
    print(f"rendered {a.views} views to {a.out_dir}")


if __name__ == "__main__":
    main()
