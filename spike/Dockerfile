# Accuracy spike toolchain: COLMAP (CUDA) + OpenCV (aruco) + pycolmap.
# Built on the official image so we don't compile COLMAP ourselves.
FROM colmap/colmap:latest

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3-venv python3-pip \
 && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --no-cache-dir \
      "numpy<2.3" opencv-contrib-python-headless pycolmap pillow trimesh

ENV PATH=/opt/venv/bin:$PATH
ENTRYPOINT []
WORKDIR /spike
