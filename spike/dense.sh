#!/usr/bin/env bash
# Dense reconstruction from a COLMAP sparse model: undistort, GPU patch-match
# stereo, fusion to a point cloud, and Poisson meshing.
#
# usage: dense.sh <images_dir> <sparse_model_dir> <work_dir>
#
# Outputs: <work_dir>/dense/fused.ply and <work_dir>/dense/mesh.ply
set -euo pipefail

IMAGES=$1
SPARSE=$2
WORK=$3
DENSE="$WORK/dense"

mkdir -p "$DENSE"

colmap image_undistorter \
  --image_path "$IMAGES" \
  --input_path "$SPARSE" \
  --output_path "$DENSE" \
  --output_type COLMAP

colmap patch_match_stereo \
  --workspace_path "$DENSE" \
  --workspace_format COLMAP \
  --PatchMatchStereo.max_image_size 3200 \
  --PatchMatchStereo.gpu_index 0

colmap stereo_fusion \
  --workspace_path "$DENSE" \
  --workspace_format COLMAP \
  --input_type geometric \
  --output_path "$DENSE/fused.ply"

colmap poisson_mesher \
  --input_path "$DENSE/fused.ply" \
  --output_path "$DENSE/mesh.ply" \
  --PoissonMeshing.depth 10

echo "dense outputs:"
ls -la "$DENSE"/fused.ply "$DENSE"/mesh.ply
