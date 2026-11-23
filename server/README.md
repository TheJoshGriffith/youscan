# ys-server

Self-hosted job server: accepts a capture (`.tar.gz`), runs the reconstruction
pipeline, and serves the resulting STL. C++20-free Qt 6.8 / C++17, SQLite, no
outbound network calls.

## Build

Debian trixie (or any distro with Qt ≥ 6.8):

```sh
apt-get install cmake ninja-build g++ qt6-base-dev qt6-httpserver-dev \
  qt6-websockets-dev libqt6sql6-sqlite
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```sh
export YOUSCAN_TOKEN="$(openssl rand -hex 32)"   # required, >= 16 chars
./build/ys-server --data-dir ./data --port 8080 --bind 127.0.0.1 \
  --job-command "bash pipeline/run_job.sh" --max-upload-mb 1024
```

The job command runs once per capture with two extra arguments: the capture
directory and the output directory. It needs the spike tools (COLMAP, OpenCV,
pycolmap, trimesh) on its `PATH`, or it can be a `docker run … youscan-spike`
wrapper.

## API

Every route except `/api/v1/health` needs `Authorization: Bearer $YOUSCAN_TOKEN`.

| Method | Path | Notes |
|---|---|---|
| GET | `/api/v1/health` | liveness |
| POST | `/api/v1/jobs` | body: capture as `.tar.gz`; returns 201 + job |
| GET | `/api/v1/jobs` | newest first, up to 100 |
| GET | `/api/v1/jobs/<id>` | state, message, output file names |
| GET | `/api/v1/jobs/<id>/log` | pipeline log |
| GET | `/api/v1/jobs/<id>/files/<name>` | e.g. `mesh.stl`, `mesh_report.json` |

Job states: `queued → running → done | failed`.

A capture archive must contain `images/` (JPEG or PNG), `intrinsics.json` and
`board_meta.json`. Archives with absolute paths, `..` components, symlinks or
device files are rejected before extraction.

## Known limits

- **Uploads are buffered in memory.** QHttpServer reads the whole body first,
  so `--max-upload-mb` also bounds RAM use. Streaming uploads are the next fix.
- **One job at a time.** Jobs share the GPU and the host's limited RAM.
- **Archive listing and extraction run on the request thread.** Fine for a
  single user; move to a worker thread before sharing the server.
- **HEIC is not converted yet.** `run_job.sh` rejects HEIC captures.
- Auth is a single shared bearer token. It is compared as a plain string.

## Privacy

The server stores captures, meshes and logs under `--data-dir` and makes no
outbound requests. Nothing is sent to a third party.
