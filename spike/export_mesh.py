#!/usr/bin/env python3
"""Bring a COLMAP-frame mesh into the metric board frame and export it for printing.

Input:  mesh PLY from dense.sh, alignment JSON from evaluate.py --save-alignment.
Output: STL (millimetres, board frame), plus a JSON report with print-readiness checks:
        watertight, winding consistent, volume, and bounding-box extents in mm.
"""
import argparse
import json

import numpy as np
import trimesh


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mesh", required=True, help="mesh PLY from COLMAP poisson_mesher")
    p.add_argument("--alignment", required=True, help="alignment JSON (scale, R, t)")
    p.add_argument("--out-stl", required=True)
    p.add_argument("--report", required=True)
    a = p.parse_args()

    al = json.load(open(a.alignment))
    s = al["scale"]
    R = np.array(al["R"])
    t = np.array(al["t"])

    mesh = trimesh.load(a.mesh, process=False)
    # X_board = s * R @ X_colmap + t
    mesh.vertices = s * (mesh.vertices @ R.T) + t

    report = {
        "vertices": int(len(mesh.vertices)),
        "faces": int(len(mesh.faces)),
        "watertight": bool(mesh.is_watertight),
        "winding_consistent": bool(mesh.is_winding_consistent),
        "volume_mm3": float(mesh.volume) if mesh.is_watertight else None,
        "extents_mm": [float(x) for x in mesh.extents],
        "bounds_mm": [[float(x) for x in row] for row in mesh.bounds],
        "units": "mm, board frame",
    }
    mesh.export(a.out_stl)
    with open(a.report, "w") as f:
        json.dump(report, f, indent=2)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
