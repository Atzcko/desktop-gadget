"""tools/eq_session.py — system-audio spectrum for the Equalizer app (D058).

The clock has no microphone; the Mac hears its own output. tools/eq_capture
(Swift, ScreenCaptureKit + Accelerate) emits 32 band levels per frame at
~30 fps as one hex line; this module runs ONE helper while any client is
attached and relays its lines over GET /eq/stream. A line starting with "!"
is a status the clock displays (most often: the permission that must be
granted once).

`?demo=1` streams a synthetic spectrum with no helper at all — the seam
that lets the device's rendering be verified independently of the Mac's
audio permission (the same each-half-on-its-own discipline as D039).
"""
import math, os, random, subprocess, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "eq_capture.swift")
BIN = os.path.join(HERE, "eq_capture")


def ensure_built():
    """Compile the helper when it is missing or older than its source."""
    if os.path.exists(BIN) and os.path.getmtime(BIN) >= os.path.getmtime(SRC):
        return True, ""
    try:
        print("[eq] building eq_capture ...")
        r = subprocess.run(["xcrun", "swiftc", "-swift-version", "5", "-O", SRC, "-o", BIN],
                           capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            tail = (r.stderr.strip().splitlines() or ["?"])[-1][:90]
            return False, "helper build failed: " + tail
        return True, ""
    except FileNotFoundError:
        return False, "swiftc not found - install the Xcode Command Line Tools"
    except Exception as e:
        return False, f"helper build error: {e}"


class Capture:
    def __init__(self):
        self.lock = threading.Lock()         # guards line/rev/status/clients
        self.start_lock = threading.Lock()   # one builder/starter at a time
        self.line = None
        self.rev = 0
        self.status = None                   # "!..." when the helper cannot capture
        self.proc = None
        self.clients = 0
        self.last_client = 0.0

    def attach(self):
        with self.lock:
            self.clients += 1
            self.last_client = time.time()
        self.ensure_running()

    def detach(self):
        with self.lock:
            self.clients = max(0, self.clients - 1)
            self.last_client = time.time()

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def ensure_running(self):
        with self.start_lock:
            if self.alive():
                return
            ok, msg = ensure_built()
            if not ok:
                with self.lock:
                    self.status = "!" + msg
                return
            with self.lock:
                self.status = None
            self.proc = subprocess.Popen([BIN], stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE, text=True, bufsize=1)
            threading.Thread(target=self._read_out, args=(self.proc,), daemon=True).start()
            threading.Thread(target=self._read_err, args=(self.proc,), daemon=True).start()
            threading.Thread(target=self._reaper, args=(self.proc,), daemon=True).start()

    def _read_out(self, p):
        for ln in p.stdout:
            ln = ln.strip()
            if ln:
                with self.lock:
                    self.line = ln
                    self.rev += 1

    def _read_err(self, p):
        for ln in p.stderr:
            ln = ln.strip()
            if ln.startswith("!"):
                with self.lock:
                    self.status = ln
            print("[eq]", ln)

    def _reaper(self, p):
        """Release the helper (and the mic-style capture) a few seconds
        after the last client leaves; nothing captures while nobody looks."""
        while p.poll() is None:
            time.sleep(1)
            with self.lock:
                idle = self.clients == 0 and time.time() - self.last_client > 4
            if idle:
                p.terminate()
                print("[eq] capture released")
                break

    def get(self):
        with self.lock:
            return self.line, self.rev, self.status


_cap = Capture()


def _demo_line(t):
    """Two wandering resonances, a 120 bpm kick in the lows, a little noise."""
    vals = []
    beat = max(0.0, math.sin(t * 2 * math.pi * 2.0)) ** 8
    for k in range(32):
        x = k / 31.0
        v = 0.55 * math.exp(-((x - (0.5 + 0.35 * math.sin(t * 0.9))) ** 2) / 0.02)
        v += 0.45 * math.exp(-((x - (0.5 + 0.40 * math.cos(t * 1.7))) ** 2) / 0.01)
        v += 0.6 * beat * math.exp(-(x ** 2) / 0.05)
        v += random.random() * 0.06
        vals.append(min(255, int(v * 255)))
    return "".join("%02x" % v for v in vals) + "\n"


def stream_to(handler, demo=False):
    if os.environ.get("EQ_FORCE_DEMO") not in (None, "", "0"):
        demo = True            # see the clock's bars without the audio permission
    handler.send_response(200)
    handler.send_header("Content-Type", "text/plain")
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    if demo:
        t0 = time.time()
        try:
            while True:
                handler.wfile.write(_demo_line(time.time() - t0).encode())
                handler.wfile.flush()
                time.sleep(1 / 30)
        except (BrokenPipeError, ConnectionResetError):
            pass
        return

    _cap.attach()
    last, last_status = -1, None
    try:
        while True:
            line, rev, status = _cap.get()
            if status and status != last_status:
                handler.wfile.write((status + "\n").encode())
                handler.wfile.flush()
                last_status = status
            if line is not None and rev != last:
                handler.wfile.write((line + "\n").encode())
                handler.wfile.flush()
                last = rev
            else:
                time.sleep(0.008)
            if not _cap.alive() and status:
                # the helper is gone (permission, build): end the response so
                # the clock reconnects in a moment and we try again — which is
                # what makes granting the permission self-healing.
                time.sleep(0.5)
                break
    except (BrokenPipeError, ConnectionResetError):
        pass
    finally:
        _cap.detach()
