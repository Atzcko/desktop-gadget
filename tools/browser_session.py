"""tools/browser_session.py — the remote-browser half of the companion (D057).

Headless Chrome renders the page; the clock is a thin client. Pixels go DOWN
as a stream of JPEGs — the exact wire format the YouTube player already
decodes (D053) — and taps / scrolls / keys come UP as POSTed input events
that Chrome replays. Chrome does all the work, so any site renders, with the
Mac's own logged-in sessions (same posture and privacy line as the YouTube
Home, D054): nothing leaves this machine but pixels.

Chrome 151 is already installed, so Playwright drives the SYSTEM Chrome
(channel="chrome") and no separate Chromium is ever downloaded.
"""
import threading, queue, time

try:
    from playwright.sync_api import sync_playwright
    PLAYWRIGHT_OK = True
except Exception:
    PLAYWRIGHT_OK = False

# A phone-shaped viewport wants a phone layout; desktop layouts overflow a
# 450 px width badly. This is the one honest lie the browser tells a site.
_UA = ("Mozilla/5.0 (Linux; Android 13; desktop-gadget) AppleWebKit/537.36 "
       "(KHTML, like Gecko) Chrome/151.0.0.0 Mobile Safari/537.36")


class BrowserSession:
    """One headless Chrome page, driven from its own thread.

    The Playwright sync API is single-threaded, so ALL page calls happen in
    _run(); HTTP handler threads only ever touch the lock-guarded frame and
    the input queue.
    """
    def __init__(self, w=450, h=540):
        self.lock = threading.Lock()
        self.frame = None                 # latest JPEG bytes
        self.rev = 0
        self.inq = queue.Queue()
        self.w, self.h = w, h
        self.want_w, self.want_h = w, h
        self.url = "about:blank"
        self.alive = False
        self.err = None
        self.last_read = time.time()
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    # ---- called from HTTP handler threads -------------------------------
    def push(self, ev):
        self.inq.put(ev)

    def set_size(self, w, h):
        if 120 <= w <= 1200 and 120 <= h <= 1600:
            self.want_w, self.want_h = int(w), int(h)

    def get_frame(self):
        with self.lock:
            return self.frame, self.rev

    def touch(self):
        self.last_read = time.time()

    # ---- the session thread --------------------------------------------
    def _run(self):
        if not PLAYWRIGHT_OK:
            self.err = "playwright not installed - pip install playwright"
            return
        try:
            with sync_playwright() as p:
                browser = p.chromium.launch(
                    channel="chrome", headless=True,
                    args=["--no-sandbox", "--disable-gpu", "--disable-dev-shm-usage"])
                ctx = browser.new_context(
                    viewport={"width": self.w, "height": self.h},
                    device_scale_factor=1, user_agent=_UA)
                page = ctx.new_page()
                page.goto("https://duckduckgo.com", wait_until="commit", timeout=20000)
                self.alive = True
                idle = 0
                while True:
                    if time.time() - self.last_read > 90:
                        break                       # nobody watching; release Chrome
                    if (self.want_w, self.want_h) != (self.w, self.h):
                        self.w, self.h = self.want_w, self.want_h
                        try:
                            page.set_viewport_size({"width": self.w, "height": self.h})
                        except Exception:
                            pass
                    did = False
                    try:
                        while True:
                            self._apply(page, self.inq.get_nowait())
                            did = True
                    except queue.Empty:
                        pass
                    try:
                        jpg = page.screenshot(type="jpeg", quality=50, timeout=6000)
                        with self.lock:
                            self.frame = jpg
                            self.rev += 1
                        self.url = page.url
                    except Exception:
                        pass
                    idle = 0 if did else idle + 1
                    time.sleep(0.05 if idle < 20 else 0.25)   # burst, then idle
                browser.close()
        except Exception as e:
            self.err = str(e)[:140]
        finally:
            self.alive = False

    def _apply(self, page, ev):
        t = ev.get("type")
        try:
            if t == "nav":
                url = ev["url"]
                if "://" not in url:
                    # a bare "example.com" is a URL; anything with a space is a search
                    url = ("https://duckduckgo.com/?q=" + url.replace(" ", "+")
                           if (" " in url or "." not in url) else "https://" + url)
                page.goto(url, wait_until="commit", timeout=25000)
            elif t == "tap":
                page.mouse.click(float(ev["x"]), float(ev["y"]))
            elif t == "scroll":
                page.mouse.wheel(float(ev.get("dx", 0)), float(ev.get("dy", 0)))
            elif t == "key":
                if ev.get("key"):
                    page.keyboard.press(ev["key"])       # Backspace, Enter, ...
                elif ev.get("text") is not None:
                    page.keyboard.type(ev["text"])
            elif t == "back":
                page.go_back(timeout=15000)
            elif t == "forward":
                page.go_forward(timeout=15000)
            elif t == "reload":
                page.reload(timeout=20000)
        except Exception:
            pass                                          # a failed nav must not kill the loop


def stream_to(handler, session):
    """Write concatenated JPEGs to an HTTP client until it hangs up. Same
    format the clock's MJPEG decoder consumes: frame on SOI..EOI."""
    session.touch()
    handler.send_response(200)
    handler.send_header("Content-Type", "video/x-motion-jpeg")
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    last = -1
    try:
        while True:
            session.touch()
            frame, rev = session.get_frame()
            if frame is not None and rev != last:
                handler.wfile.write(frame)
                last = rev
            else:
                time.sleep(0.03)
    except (BrokenPipeError, ConnectionResetError):
        pass
