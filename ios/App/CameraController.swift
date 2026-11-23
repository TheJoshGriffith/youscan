import AVFoundation
import Foundation

enum CameraError: Error {
    case noCamera
    case configurationFailed
    case noImageData
}

/// Still-photo capture on the 1x main camera at the largest supported resolution.
/// Focus and exposure can be locked so every shot in a set shares the same intrinsics.
final class CameraController: NSObject, AVCapturePhotoCaptureDelegate {
    let session = AVCaptureSession()

    private let photoOutput = AVCapturePhotoOutput()
    private let sessionQueue = DispatchQueue(label: "dev.youscan.camera.session")
    private var device: AVCaptureDevice?
    private var pendingURL: URL?
    private var completion: ((Result<URL, Error>) -> Void)?

    /// Current lens position in 0...1 (0 = near, 1 = far). Read it after autofocus settles.
    var currentLensPosition: Float {
        device?.lensPosition ?? 0.5
    }

    func configure() throws {
        guard let camera = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back) else {
            throw CameraError.noCamera
        }
        device = camera

        let input = try AVCaptureDeviceInput(device: camera)
        session.beginConfiguration()
        defer { session.commitConfiguration() }

        session.sessionPreset = .photo
        guard session.canAddInput(input), session.canAddOutput(photoOutput) else {
            throw CameraError.configurationFailed
        }
        session.addInput(input)
        session.addOutput(photoOutput)

        if let largest = camera.activeFormat.supportedMaxPhotoDimensions.last {
            photoOutput.maxPhotoDimensions = largest
        }
        photoOutput.maxPhotoQualityPrioritization = .quality
    }

    func start() {
        sessionQueue.async { self.session.startRunning() }
    }

    func stop() {
        sessionQueue.async { self.session.stopRunning() }
    }

    /// Lock focus at the given lens position and lock exposure.
    func lock(lensPosition: Float) throws {
        guard let device else { throw CameraError.noCamera }
        try device.lockForConfiguration()
        defer { device.unlockForConfiguration() }

        if device.isFocusModeSupported(.locked) {
            let clamped = min(max(lensPosition, 0), 1)
            device.setFocusModeLocked(lensPosition: clamped, completionHandler: nil)
        }
        if device.isExposureModeSupported(.locked) {
            device.exposureMode = .locked
        }
    }

    /// Capture one HEIC still and write it to `directory`.
    func capture(to directory: URL, index: Int, completion: @escaping (Result<URL, Error>) -> Void) {
        let settings = AVCapturePhotoSettings(format: [AVVideoCodecKey: AVVideoCodecType.hevc])
        settings.maxPhotoDimensions = photoOutput.maxPhotoDimensions
        settings.photoQualityPrioritization = .quality

        pendingURL = directory.appendingPathComponent(String(format: "shot_%03d.heic", index))
        self.completion = completion
        photoOutput.capturePhoto(with: settings, delegate: self)
    }

    // MARK: - AVCapturePhotoCaptureDelegate

    func photoOutput(_ output: AVCapturePhotoOutput,
                     didFinishProcessingPhoto photo: AVCapturePhoto,
                     error: Error?) {
        let finish = completion
        let url = pendingURL
        completion = nil
        pendingURL = nil

        if let error {
            finish?(.failure(error))
            return
        }
        guard let url, let data = photo.fileDataRepresentation() else {
            finish?(.failure(CameraError.noImageData))
            return
        }
        do {
            try data.write(to: url, options: .atomic)
            finish?(.success(url))
        } catch {
            finish?(.failure(error))
        }
    }
}
