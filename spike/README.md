# Photogrammetry accuracy spike

Goal: decide whether iPhone photogrammetry, with a calibrated ChArUco board as the
metric reference, can reach **<1 mm** on electronics-scale objects.

## Why not LiDAR for measurement

iPhone LiDAR depth is centimetre-class. At 25–40 cm it can't deliver <1 mm, so it
stays out of the measurement path. Camera-only photogrammetry at close range
has the resolution for it: at ~30 cm with the main lens, a pixel covers roughly
0.1 mm on the object (estimate; the spike measures it). Depth precision improves
with a wider orbit, so capture shots spread around the object, not in a narrow arc.

## Pipeline

| Step | Script | Output |
|---|---|---|
| 1. Make the board | `make_charuco.py` | `charuco_board.png`, `board_meta.json` |
| 2. Calibrate intrinsics | `calibrate.py` | `intrinsics.json` (+ COLMAP param string) |
| 3. SfM, fixed intrinsics | `run_sfm.sh` | `work/sparse/0/` |
| 4. Accuracy vs board | `evaluate.py --mode pnp` (floor), `--mode colmap` (SfM) | JSON summary |
| Plumbing test | `synth.py` → steps 2–4 | expects near-zero error |

## Capture protocol (iPhone 16 Pro Max)

Keep the setup identical across every shot. Changing any of these moves the
intrinsics and ruins the test.

1. **Lens:** the 1× main lens only. Don't use the 0.5×, 2×, 5×, or auto-macro
   switch. Turn off Macro Control in Camera settings.
2. **Format:** HEIF/HEIC at 48 MP (Settings → Camera → Formats → High Efficiency).
3. **Focus and exposure:** tap and hold on the board to lock AF/AE. Keep the
   lock for the whole session. Focus breathing changes intrinsics, so calibrate
   at the same working distance you shoot the object at.
4. **Light:** diffuse and even. Avoid glare on the board and solder; a
   cross-polarised setup helps with shiny parts later.
5. **Board:** print at 100% with no page scaling. Measure several squares with
   calipers, record the actual value, and keep the board flat (glass or
   aluminium plate beats card for accuracy).
6. **Calibration set:** 20–30 photos of the board alone, varied angle and
   position, at the working distance. Board filling 30–70% of the frame.
7. **Object set:** the object sits **on the board**, so the board defines the
   metric frame. About 60–80 photos in three rings (high, mid, low), ~70%
   overlap, distance ~25–40 cm, same lock state.
8. Transfer as originals (HEIC is fine). Keep EXIF.

Convert HEIC to JPEG for the pipeline (the spike scripts read JPEG/PNG) with:
`magick mogrify -format jpg *.HEIC` or `heif-convert`.

## Running it on a GPU host

Any Linux host with an NVIDIA GPU and Docker with the NVIDIA runtime works
(tested: RTX 2080 Super, 8 GB). Copy `spike/` to a working directory, build the image there, then
run the tools with `--gpus all` and a memory cap so the host stays responsive:

```sh
docker build -t youscan-spike .
docker run --rm --gpus all --memory 20g -v $PWD:/spike youscan-spike \
  python3 synth.py --meta board/board_meta.json --out-dir data/synth
```

## Interpreting the numbers

- `pnp` error is the **floor**: calibration plus corner detection. If the floor
  is above ~0.5 mm, fix the board, calibration, or focus lock before blaming SfM.
- `colmap` error should sit close to the floor. A large gap means SfM pose or
  alignment is the bottleneck.
- Synthetic data has no lens distortion and no focus blur, so its error is an
  upper bound on what the plumbing can introduce, not a measure of the phone.

## Status (synthetic plumbing test)

Run on a GPU host (`youscan-spike` image, RTX 2080 Super). Synthetic views, 30 frames,
no lens distortion, no focus blur, fixed intrinsics in COLMAP.

| Check | Result |
|---|---|
| Calibration recovers fx (true 3000 px) | 2998.2 px (−0.07%) |
| Board-only PnP floor, pairwise distance error | RMS 0.003 mm, max 0.008 mm |
| COLMAP SfM, aligned to board, pairwise distance error | RMS 0.033 mm, max 0.071 mm |
| COLMAP registered views | 30 / 30 |

These numbers only show the math and plumbing are right. They are not evidence about
the phone. The real test needs photos of the printed board from the 1× lens.

GPU-host notes: COLMAP 4 renamed the GPU/size options to `FeatureExtraction.*`
and `FeatureMatching.*`. SIFT-GPU fails on 4032 px images, so extraction is capped at
3200 px. Keep that cap unless the spike shows it costs accuracy.
