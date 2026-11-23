import Foundation
import SwiftUI

/// Owns one capture set: a timestamped folder of HEIC shots.
final class CaptureModel: ObservableObject {
    let camera = CameraController()
    let folder: URL

    @Published var shotCount = 0
    @Published var ready = false
    @Published var message: String?

    init() {
        let stamp = ISO8601DateFormatter().string(from: Date())
            .replacingOccurrences(of: ":", with: "-")
        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        folder = documents.appendingPathComponent("captures/\(stamp)", isDirectory: true)
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)

        do {
            try camera.configure()
            ready = true
        } catch {
            message = "Camera unavailable: \(error)"
        }
    }

    func start() { camera.start() }
    func stop() { camera.stop() }

    func lockFocus() {
        do {
            try camera.lock(lensPosition: camera.currentLensPosition)
            message = "Focus and exposure locked"
        } catch {
            message = "Lock failed: \(error)"
        }
    }

    func shoot() {
        let index = shotCount + 1
        camera.capture(to: folder, index: index) { result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self.shotCount = index
                case .failure(let error):
                    self.message = "Shot failed: \(error.localizedDescription)"
                }
            }
        }
    }
}

struct CaptureView: View {
    @StateObject private var model = CaptureModel()

    var body: some View {
        ZStack(alignment: .bottom) {
            CameraPreview(session: model.camera.session)
                .ignoresSafeArea()

            VStack(spacing: 12) {
                if let message = model.message {
                    Text(message)
                        .font(.footnote)
                        .padding(8)
                        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
                }
                Text("Shots: \(model.shotCount)")
                    .font(.headline)
                HStack(spacing: 16) {
                    Button("Lock focus") { model.lockFocus() }
                        .buttonStyle(.bordered)
                    Button("Shoot") { model.shoot() }
                        .buttonStyle(.borderedProminent)
                        .disabled(!model.ready)
                }
            }
            .padding()
            .frame(maxWidth: .infinity)
            .background(.ultraThinMaterial)
        }
        .onAppear { model.start() }
        .onDisappear { model.stop() }
        .navigationTitle("Capture")
        .navigationBarTitleDisplayMode(.inline)
    }
}
