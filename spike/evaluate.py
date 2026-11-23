#!/usr/bin/env python3
"""Measure metric accuracy against the ChArUco board.

Two modes:
  pnp    Board-only. Each image's pose comes from solvePnP on the board corners.
         This is the floor: it measures calibration and corner-detection error
         with no SfM involved.
  colmap SfM. Poses come from a COLMAP sparse model. The model is similarity-
         aligned to the board frame (scale included) using camera centres
         from board PnP, then board corners are triangulated and their
         pairwise distances are compared with the true mm values.

Error metric: for every pair of board corners observed in at least two views,
|d_measured - d_true| in mm. Reported as RMS, mean, 95th percentile, max, and
the fraction under 1 mm.
"""
import argparse
import itertools
import json
import os

import cv2
import numpy as np

from common import detect, load_meta, make_board


def umeyama(src, dst):
    """Similarity transform (s, R, t) minimising ||dst - (s R src + t)||."""
    n = len(src)
    mu_s, mu_d = src.mean(0), dst.mean(0)
    xs, xd = src - mu_s, dst - mu_d
    cov = xd.T @ xs / n
    U, D, Vt = np.linalg.svd(cov)
    S = np.eye(3)
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        S[2, 2] = -1
    R = U @ S @ Vt
    var_s = (xs ** 2).sum() / n
    s = np.trace(np.diag(D) @ S) / var_s
    t = mu_d - s * R @ mu_s
    return s, R, t


def cam_from_world(image):
    pose = image.cam_from_world() if callable(image.cam_from_world) else image.cam_from_world
    return pose.rotation.matrix(), np.asarray(pose.translation).reshape(3)


def triangulate(P_list, pts_norm):
    """DLT. P_list: 3x4 extrinsics [R|t] in metric board frame. pts_norm: normalised (x, y)."""
    A = []
    for P, (x, y) in zip(P_list, pts_norm):
        A.append(x * P[2] - P[0])
        A.append(y * P[2] - P[1])
    _, _, Vt = np.linalg.svd(np.array(A))
    X = Vt[-1]
    return X[:3] / X[3]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mode", choices=["pnp", "colmap"], required=True)
    p.add_argument("--images", required=True, help="directory with the same images used in SfM")
    p.add_argument("--meta", required=True)
    p.add_argument("--intrinsics", required=True, help="calibration JSON from calibrate.py")
    p.add_argument("--sparse", help="COLMAP sparse model dir (colmap mode)")
    p.add_argument("--min-observed-views", type=int, default=2)
    p.add_argument("--save-alignment", help="write SfM->board similarity (s,R,t) JSON here")
    a = p.parse_args()

    meta = load_meta(a.meta)
    board = make_board(meta)
    board_pts = board.getChessboardCorners()  # (N,3) mm, board frame

    calib = json.load(open(a.intrinsics))
    K = np.array([[calib["fx"], 0, calib["cx"]], [0, calib["fy"], calib["cy"]], [0, 0, 1.0]])
    dist = np.array([calib["k1"], calib["k2"], calib["p1"], calib["p2"], 0.0])

    # Per-image board observations and board-frame PnP poses.
    obs = {}  # name -> {"ids": ..., "norm": Nx2 normalised undistorted points}
    pnp = {}  # name -> (R, t) board-from-camera
    for name in sorted(os.listdir(a.images)):
        if not name.lower().endswith((".jpg", ".jpeg", ".png")):
            continue
        gray = cv2.imread(os.path.join(a.images, name), cv2.IMREAD_GRAYSCALE)
        if gray is None:
            continue
        corners, ids = detect(board, gray)
        if corners is None:
            continue
        pix = corners.reshape(-1, 2).astype(np.float64)
        norm = cv2.undistortPoints(pix.reshape(-1, 1, 2), K, dist).reshape(-1, 2)
        obs[name] = {"ids": ids, "norm": norm}
        ok, rvec, tvec = cv2.solvePnP(
            board_pts[ids].astype(np.float64), pix.reshape(-1, 1, 2), K, dist,
            flags=cv2.SOLVEPNP_ITERATIVE,
        )
        if ok:
            R, _ = cv2.Rodrigues(rvec)
            pnp[name] = (R, tvec.reshape(3))

    if len(obs) < 3:
        raise SystemExit(f"only {len(obs)} images with a detectable board")

    # Build metric board-frame extrinsics for every camera we can use.
    if a.mode == "pnp":
        extr = {n: pnp[n] for n in obs if n in pnp}
        scale = 1.0
    else:
        import pycolmap
        rec = pycolmap.Reconstruction(a.sparse)
        col = {}
        for image in rec.images.values():
            name = os.path.basename(image.name)
            if name in obs:
                R, t = cam_from_world(image)
                col[name] = (R, t)
        common = [n for n in col if n in pnp]
        if len(common) < 3:
            raise SystemExit(f"only {len(common)} images in both SfM and board PnP")
        # Camera centres: C = -R^T t.
        src = np.array([-col[n][0].T @ col[n][1] for n in common])
        dst = np.array([-pnp[n][0].T @ pnp[n][1] for n in common])
        scale, Ra, ta = umeyama(src, dst)
        if a.save_alignment:
            # Maps COLMAP-frame points to metric board-frame points: X_board = s * R @ X + t.
            with open(a.save_alignment, "w") as f:
                json.dump({"scale": float(scale), "R": Ra.tolist(), "t": ta.tolist()}, f, indent=2)
        extr = {}
        for n, (R_c, t_c) in col.items():
            Rp = R_c @ Ra.T
            tp = scale * t_c - Rp @ ta
            extr[n] = (Rp, tp)
        print(f"SfM registered {len(col)} board-visible views; alignment scale={scale:.6f}, "
              f"aligned on {len(common)} views")

    # Triangulate each board corner from every usable view.
    point_obs = {}  # corner id -> list of (name, norm pt)
    for n in extr:
        for cid, pt in zip(obs[n]["ids"], obs[n]["norm"]):
            point_obs.setdefault(int(cid), []).append((n, pt))

    est = {}
    reproj = []
    for cid, lst in point_obs.items():
        if len(lst) < a.min_observed_views:
            continue
        Ps, pts = [], []
        for n, pt in lst:
            R, t = extr[n]
            Ps.append(np.hstack([R, t.reshape(3, 1)]))
            pts.append(pt)
        X = triangulate(Ps, pts)
        est[cid] = X
        # Reprojection residual in normalised units, converted to px with fx.
        for P, pt in zip(Ps, pts):
            x = P @ np.append(X, 1.0)
            if x[2] > 0:
                reproj.append(np.linalg.norm(x[:2] / x[2] - pt) * calib["fx"])

    # Pairwise distance errors.
    errs = []
    for a_id, b_id in itertools.combinations(sorted(est), 2):
        d_meas = np.linalg.norm(est[a_id] - est[b_id])
        d_true = np.linalg.norm(board_pts[a_id] - board_pts[b_id])
        errs.append(d_meas - d_true)
    errs = np.abs(np.array(errs)) if errs else np.array([np.nan])

    summary = {
        "mode": a.mode,
        "views_with_board": len(obs),
        "views_used_for_pose": len(extr),
        "corners_triangulated": len(est),
        "pairs": int(np.sum(~np.isnan(errs))),
        "scale_colmap_to_mm": float(scale),
        "triangulation_reproj_rms_px": float(np.sqrt(np.mean(np.square(reproj)))) if reproj else None,
        "abs_error_mm": {
            "rms": float(np.sqrt(np.mean(errs ** 2))),
            "mean": float(np.mean(errs)),
            "p95": float(np.percentile(errs, 95)),
            "max": float(np.max(errs)),
            "fraction_under_1mm": float(np.mean(errs < 1.0)),
        },
    }
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
