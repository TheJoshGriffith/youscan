#!/usr/bin/env python3
"""Estimate camera intrinsics from ChArUco board photos.

Shoot 20-30 views of the board at the working distance (focus locked there),
varying angle and position. The output JSON feeds COLMAP as fixed intrinsics.
"""
import argparse
import glob
import json
import os

import cv2

from common import detect, load_meta, make_board


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--images", required=True, help="glob, e.g. '/data/calib/*.jpg'")
    p.add_argument("--meta", required=True, help="board_meta.json")
    p.add_argument("--out", required=True)
    a = p.parse_args()

    meta = load_meta(a.meta)
    board = make_board(meta)

    obj_pts, img_pts = [], []
    image_size = None
    used, skipped = [], []
    for path in sorted(glob.glob(a.images)):
        gray = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
        if gray is None:
            skipped.append(path)
            continue
        corners, ids = detect(board, gray)
        if corners is None:
            skipped.append(path)
            continue
        image_size = (gray.shape[1], gray.shape[0])
        obj, img = board.matchImagePoints(corners, ids.reshape(-1, 1))
        obj_pts.append(obj)
        img_pts.append(img)
        used.append(os.path.basename(path))

    if len(used) < 10:
        raise SystemExit(f"only {len(used)} usable views; need at least 10")

    # Fix k3 to keep the model identical to COLMAP's OPENCV model (k1,k2,p1,p2).
    flags = cv2.CALIB_FIX_K3
    rms, K, dist, _, _ = cv2.calibrateCamera(
        obj_pts, img_pts, image_size, None, None, flags=flags
    )
    dist = dist.flatten()
    result = {
        "image_size": list(image_size),
        "rms_reprojection_px": float(rms),
        "fx": float(K[0, 0]),
        "fy": float(K[1, 1]),
        "cx": float(K[0, 2]),
        "cy": float(K[1, 2]),
        "k1": float(dist[0]),
        "k2": float(dist[1]),
        "p1": float(dist[2]),
        "p2": float(dist[3]),
        "views_used": len(used),
        "views_skipped": len(skipped),
    }
    with open(a.out, "w") as f:
        json.dump(result, f, indent=2)

    print(json.dumps(result, indent=2))
    # COLMAP OPENCV camera_params order: fx, fy, cx, cy, k1, k2, p1, p2
    print("COLMAP params:", ",".join(
        f"{result[k]:.10g}" for k in ["fx", "fy", "cx", "cy", "k1", "k2", "p1", "p2"]))


if __name__ == "__main__":
    main()
