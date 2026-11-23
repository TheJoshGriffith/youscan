#!/usr/bin/env bash
# Run COLMAP SfM with FIXED calibrated intrinsics.
#
# usage: run_sfm.sh <images_dir> <work_dir> "<fx,fy,cx,cy,k1,k2,p1,p2>"
#
# Intrinsics are held fixed (no refinement) so the reconstruction can only
# move in pose/structure, never in lens model. That keeps the accuracy test honest.
set -euo pipefail

IMAGES=$1
WORK=$2
PARAMS=$3

mkdir -p "$WORK"
DB="$WORK/database.db"
rm -f "$DB"

colmap feature_extractor \
  --database_path "$DB" \
  --image_path "$IMAGES" \
  --ImageReader.single_camera 1 \
  --ImageReader.camera_model OPENCV \
  --ImageReader.camera_params "$PARAMS" \
  --FeatureExtraction.use_gpu 1 \
  --FeatureExtraction.max_image_size 3200 \
  --SiftExtraction.max_num_features 8192

colmap exhaustive_matcher \
  --database_path "$DB" \
  --FeatureMatching.use_gpu 1

mkdir -p "$WORK/sparse"
colmap mapper \
  --database_path "$DB" \
  --image_path "$IMAGES" \
  --output_path "$WORK/sparse" \
  --Mapper.ba_refine_focal_length 0 \
  --Mapper.ba_refine_principal_point 0 \
  --Mapper.ba_refine_extra_params 0

echo "sparse models:"
ls "$WORK/sparse"
