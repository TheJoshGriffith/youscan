# YouScan

Self-hosted 3D scanning for electronics and small objects. Target: sub-millimetre
measurement from iPhone photogrammetry with a calibrated ChArUco reference board.

Nothing leaves the user's hardware. The phone captures, and a server the user runs
does all reconstruction.

## Layout

| Path | What it is |
|---|---|
| `ios/` | iOS app (SwiftUI + AVFoundation). 1× lens, 48 MP HEIC, focus and exposure locked. |
| `spike/` | Photogrammetry accuracy spike: board generation, calibration, SfM, dense stage, mesh export, accuracy evaluation. |
| `.github/workflows/ci.yml` | CI: builds the iOS app on a macOS runner (no signing) and syntax-checks the spike. |

## Building the iOS app

Requires macOS with Xcode and [XcodeGen](https://github.com/yonaskolb/XcodeGen):

```sh
brew install xcodegen
cd ios && xcodegen generate
open YouScan.xcodeproj
```

CI does the same on a macOS runner, so the build can be checked without a Mac.

## Spike

See [`spike/README.md`](spike/README.md).
