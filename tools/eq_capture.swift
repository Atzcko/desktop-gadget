// tools/eq_capture.swift — system-audio spectrum helper for the Equalizer app (D058).
//
// ScreenCaptureKit captures the Mac's OWN audio output (no virtual-audio
// driver, no rerouting; the same path OBS uses on modern macOS), Accelerate
// runs a 2048-point FFT, and 32 log-spaced bands come out on stdout as one
// hex line per frame at ~30 fps: 64 hex chars (32 bands × 0–255) + "\n".
// A line starting with "!" is a status for the clock to display.
//
// Needs the "Screen & System Audio Recording" permission once, granted to
// whichever app launched it (Terminal, or Claude when driven from there).
//
// Build:  swiftc -swift-version 5 -O tools/eq_capture.swift -o tools/eq_capture
// (tools/ytserve does this itself when the binary is missing or stale.)

import Foundation
import ScreenCaptureKit
import Accelerate
import CoreMedia

let BANDS   = 32
let FFT_N   = 2048
let HOP     = 1600                 // 48000 / 1600 = 30 frames/s
let SR      = 48000.0
let FMIN    = 40.0
let FMAX    = 16000.0
let ATTACK: Float = 0.60           // rise fraction per frame
let DECAY:  Float = 0.14           // fall fraction per frame
let DB_SPAN: Float = 54            // dB below the running peak that maps to 0

func say(_ s: String) {
    FileHandle.standardError.write((s + "\n").data(using: .utf8)!)
}

final class Cap: NSObject, SCStreamOutput, SCStreamDelegate {
    var ring    = [Float](repeating: 0, count: FFT_N)
    var pending = [Float]()
    var window  = [Float](repeating: 0, count: FFT_N)
    var winBuf  = [Float](repeating: 0, count: FFT_N)
    var re      = [Float](repeating: 0, count: FFT_N / 2)
    var im      = [Float](repeating: 0, count: FFT_N / 2)
    var ore     = [Float](repeating: 0, count: FFT_N / 2)
    var oim     = [Float](repeating: 0, count: FFT_N / 2)
    var mags    = [Float](repeating: 0, count: FFT_N / 2)
    var edges   = [Int]()
    var levels  = [Float](repeating: 0, count: BANDS)
    var peakDb: Float = -30
    let dft: OpaquePointer?
    let lock = NSLock()

    override init() {
        dft = vDSP_DFT_zrop_CreateSetup(nil, vDSP_Length(FFT_N), .FORWARD)
        super.init()
        vDSP_hann_window(&window, vDSP_Length(FFT_N), Int32(vDSP_HANN_NORM))
        let binHz = SR / Double(FFT_N)
        for i in 0...BANDS {
            let f = FMIN * pow(FMAX / FMIN, Double(i) / Double(BANDS))
            edges.append(max(1, min(FFT_N / 2 - 1, Int(f / binHz))))
        }
    }

    // ---- SCStreamOutput -------------------------------------------------
    func stream(_ stream: SCStream, didOutputSampleBuffer sb: CMSampleBuffer,
                of type: SCStreamOutputType) {
        guard type == .audio else { return }
        var needed = 0
        CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sb, bufferListSizeNeededOut: &needed, bufferListOut: nil, bufferListSize: 0,
            blockBufferAllocator: nil, blockBufferMemoryAllocator: nil, flags: 0,
            blockBufferOut: nil)
        guard needed > 0 else { return }
        let raw = UnsafeMutableRawPointer.allocate(byteCount: needed,
                                                   alignment: MemoryLayout<AudioBufferList>.alignment)
        defer { raw.deallocate() }
        let abl = raw.bindMemory(to: AudioBufferList.self, capacity: 1)
        var bb: CMBlockBuffer?
        let st = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sb, bufferListSizeNeededOut: nil, bufferListOut: abl, bufferListSize: needed,
            blockBufferAllocator: kCFAllocatorDefault, blockBufferMemoryAllocator: kCFAllocatorDefault,
            flags: 0, blockBufferOut: &bb)
        guard st == noErr else { return }
        let bufs = UnsafeMutableAudioBufferListPointer(abl)

        // downmix whatever layout arrives (non-interleaved per-channel buffers,
        // or one interleaved buffer) to mono
        var mono = [Float]()
        if bufs.count >= 2 {
            let n = Int(bufs[0].mDataByteSize) / 4
            guard let a = bufs[0].mData?.assumingMemoryBound(to: Float.self),
                  let b = bufs[1].mData?.assumingMemoryBound(to: Float.self) else { return }
            mono.reserveCapacity(n)
            for i in 0..<n { mono.append((a[i] + b[i]) * 0.5) }
        } else if bufs.count == 1 {
            let ch = max(1, Int(bufs[0].mNumberChannels))
            let n = Int(bufs[0].mDataByteSize) / 4 / ch
            guard let a = bufs[0].mData?.assumingMemoryBound(to: Float.self) else { return }
            mono.reserveCapacity(n)
            for i in 0..<n {
                var s: Float = 0
                for c in 0..<ch { s += a[i * ch + c] }
                mono.append(s / Float(ch))
            }
        } else { return }

        lock.lock(); defer { lock.unlock() }
        pending.append(contentsOf: mono)
        while pending.count >= HOP {
            ring.removeFirst(HOP)
            ring.append(contentsOf: pending[0..<HOP])
            pending.removeFirst(HOP)
            analyze()
        }
    }

    func stream(_ stream: SCStream, didStopWithError error: Error) {
        say("!capture stopped: \(error.localizedDescription)")
        exit(3)
    }

    // ---- DSP -------------------------------------------------------------
    func analyze() {
        vDSP_vmul(ring, 1, window, 1, &winBuf, 1, vDSP_Length(FFT_N))
        winBuf.withUnsafeBufferPointer { wp in
            wp.baseAddress!.withMemoryRebound(to: DSPComplex.self, capacity: FFT_N / 2) { cp in
                var split = DSPSplitComplex(realp: &re, imagp: &im)
                vDSP_ctoz(cp, 2, &split, 1, vDSP_Length(FFT_N / 2))
            }
        }
        vDSP_DFT_Execute(dft!, re, im, &ore, &oim)
        var split = DSPSplitComplex(realp: &ore, imagp: &oim)
        vDSP_zvmags(&split, 1, &mags, 1, vDSP_Length(FFT_N / 2))
        mags[0] = 0                                       // DC is not music

        var frameMax: Float = -120
        var target = [Float](repeating: 0, count: BANDS)
        for k in 0..<BANDS {
            let lo = edges[k], hi = max(edges[k] + 1, edges[k + 1])
            var acc: Float = 0
            for b in lo..<hi { acc = max(acc, mags[b]) }        // peak within the band
            let db = 10 * log10f(acc + 1e-12)
            target[k] = db
            frameMax = max(frameMax, db)
        }
        // slow AGC: the running peak defines "full scale", so quiet sources fill too
        if frameMax > peakDb { peakDb += (frameMax - peakDb) * 0.30 }
        else                 { peakDb += (frameMax - peakDb) * 0.005 }
        peakDb = max(peakDb, -60)

        var line = ""
        line.reserveCapacity(BANDS * 2 + 1)
        for k in 0..<BANDS {
            var v = (target[k] - (peakDb - DB_SPAN)) / DB_SPAN   // 0..1
            v = min(1, max(0, v))
            let cur = levels[k]
            levels[k] = cur + (v - cur) * (v > cur ? ATTACK : DECAY)
            let byte = Int(levels[k] * 255 + 0.5)
            line += String(format: "%02x", min(255, max(0, byte)))
        }
        line += "\n"
        fputs(line, stdout)
        fflush(stdout)                                    // a broken pipe (ytserve gone) ends us
    }
}

let cap = Cap()
Task {
    do {
        let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: false)
        guard let display = content.displays.first else { say("!no display to capture from"); exit(2) }
        let filter = SCContentFilter(display: display, excludingWindows: [])
        let cfg = SCStreamConfiguration()
        cfg.capturesAudio = true
        cfg.excludesCurrentProcessAudio = true
        cfg.sampleRate = Int(SR)
        cfg.channelCount = 2
        cfg.width = 2
        cfg.height = 2
        cfg.minimumFrameInterval = CMTime(value: 1, timescale: 1)   // we add no video output anyway
        let stream = SCStream(filter: filter, configuration: cfg, delegate: cap)
        try stream.addStreamOutput(cap, type: .audio, sampleHandlerQueue: DispatchQueue(label: "eq.audio"))
        try await stream.startCapture()
        say("eq_capture: capturing system audio")
    } catch {
        let msg = "\(error)"
        if msg.lowercased().contains("declined") || msg.contains("-3801") || msg.lowercased().contains("tcc") {
            say("!grant Screen & System Audio Recording to the app that launched this (System Settings > Privacy & Security)")
        } else {
            say("!capture failed: \(error.localizedDescription)")
        }
        exit(2)
    }
}
dispatchMain()
