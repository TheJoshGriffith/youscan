"""Shared helpers for the photogrammetry accuracy spike.

The ChArUco board is the metric reference: its geometry is defined in
millimetres by board_meta.json, so every pose derived from it is in mm.
"""
import json

import cv2
import numpy as np

ARUCO_DICT = cv2.aruco.DICT_5X5_1000


def load_meta(path):
    with open(path) as f:
        return json.load(f)


def make_board(meta):
    dictionary = cv2.aruco.getPredefinedDictionary(ARUCO_DICT)
    return cv2.aruco.CharucoBoard(
        (meta["squares_x"], meta["squares_y"]),
        meta["square_mm"],
        meta["marker_mm"],
        dictionary,
    )


def detect(board, gray, min_corners=8):
    """Return (corners Nx1x2 float32, ids N) or (None, None) if too few corners."""
    detector = cv2.aruco.CharucoDetector(board)
    corners, ids, _, _ = detector.detectBoard(gray)
    if corners is None or ids is None or len(ids) < min_corners:
        return None, None
    return corners, ids.flatten()
