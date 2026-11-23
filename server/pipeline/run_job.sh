#!/usr/bin/env bash
# Pipeline for one capture job. Called by ys-server as:
#   run_job.sh <capture_dir> <out_dir>
#
# capture_dir must contain:
#   images/        JPEG or PNG photos (HEIC must be converted first)
#   intrinsics.json   from calibrate.py
#   board_meta.json   ChArUco board geometry
#
# Writes to out_dir: mesh.stl (millimetres, board frame), mesh_report.json,
# alignment.json, evaluate.json, and work/ (intermediate COLMAP output).
set -euo pipefail

CAP=${1:?capture dir}
OUT=${2:?output dir}
SPIKE=${SPIKE_DIR:-$(cd "$(dirname "$0")/../../spike" && pwd)}

for f in images intrinsics.json board_meta.json; do
  if [ ! -e "$CAP/$f" ]; then
    echo "missing $f in capture" >&2
    exit 2
  fi
done

if find "$CAP/images" -iname '*.heic' -print -quit | grep -q .; then
  echo "HEIC images are not supported yet; convert to JPEG first" >&2
  exit 2
fi

mkdir -p "$OUT"
echo "[1/4] SfM with fixed intrinsics"
PARAMS=$(python3 -c '
import json, sys
c = json.load(open(sys.argv[1]))
print(",".join(f"{c[k]:.10g}" for k in ["fx","fy","cx","cy","k1","k2","p1","p2"]))
' "$CAP/intrinsics.json")
bash "$SPIKE/run_sfm.sh" "$CAP/images" "$OUT/work" "$PARAMS"

echo "[2/4] Board evaluation and alignment to millimetres"
python3 "$SPIKE/evaluate.py" --mode colmap \
  --images "$CAP/images" \
  --meta "$CAP/board_meta.json" \
  --intrinsics "$CAP/intrinsics.json" \
  --sparse "$OUT/work/sparse/0" \
  --save-alignment "$OUT/alignment.json" > "$OUT/evaluate.json"

echo "[3/4] Dense reconstruction and meshing"
bash "$SPIKE/dense.sh" "$CAP/images" "$OUT/work/sparse/0" "$OUT/work"

echo "[4/4] Export STL in millimetres"
python3 "$SPIKE/export_mesh.py" \
  --mesh "$OUT/work/dense/mesh.ply" \
  --alignment "$OUT/alignment.json" \
  --out-stl "$OUT/mesh.stl" \
  --report "$OUT/mesh_report.json"

echo "done"
