import SwiftUI

struct ContentView: View {
    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                Text("YouScan")
                    .font(.largeTitle.bold())
                Text("1× lens, locked focus and exposure, 48 MP HEIC")
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                NavigationLink("Start capture") {
                    CaptureView()
                }
                .buttonStyle(.borderedProminent)
            }
            .padding()
        }
    }
}
