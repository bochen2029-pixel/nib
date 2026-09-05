"""nib · drive — the window's test battery.

A window is verified by posting the messages a keyboard would cause and then reading an artefact:
the bytes on disk and the lines nib appends to NIB_LOG. It is never verified by looking at it, and
it is never driven with `keybd_event` or `SendInput` — synthesised global input lands wherever the
focus happens to be, which can type into somebody else's window. That is why edit.cpp carries the
WM_APP+1 command channel: a chord posted with PostMessage does NOT update the thread's key state,
so `GetKeyState(VK_CONTROL)` reads false and Ctrl+S would silently do nothing.

    python tools/drive.py                 # the battery, against C:/nib/nib.exe
    python tools/drive.py --exe X --keep  # a different build; leave the scratch files behind

Exit code 0 when every check passes, 3 when one does not, so this can join --selftest's discipline
even though it lives outside the exe.
"""
import argparse
import ctypes
import os
import subprocess
import sys
import time

u32 = ctypes.windll.user32

WM_CHAR, WM_KEYDOWN, WM_KEYUP, WM_CLOSE = 0x0102, 0x0100, 0x0101, 0x0010
WM_NIB_CMD = 0x8000 + 1                     # WM_APP + 1, matching edit.cpp
CMD = dict(save=1, save_as=2, open=3, undo=4, redo=5, select_all=6,
           replay=7, home=8, end=9, sel_to_home=10, top=11, ingest=12)
VK = dict(back=0x08, delete=0x2E, left=0x25, right=0x27, up=0x26, down=0x28,
          home=0x24, end=0x23)

passed, failed = 0, 0


def check(ok, what):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
    print(("  ok    " if ok else "  FAIL  ") + what)
    sys.stdout.flush()


class Nib:
    """One running window, driven by messages and read through its log."""

    def __init__(self, exe, path, log):
        self.log = log
        # NIB_DRIVER makes the window no-activate: posted messages still arrive, the keyboard never
        # does. Without it a test window takes the foreground and eats whatever the operator is
        # typing elsewhere — which is how fragments of an unrelated sentence reached the scratch
        # files on 2026-09-04.
        env = dict(os.environ, NIB_LOG=log, NIB_DRIVER="1")
        self.proc = subprocess.Popen([exe, "--edit", path], env=env)
        # Bind to the window belonging to THIS process. FindWindow by class alone will happily
        # return a leftover from a previous case, or the operator's own editor — which made two
        # runs in three fail in ways that had nothing to do with the code under test.
        self.hwnd = 0
        for _ in range(120):
            self.hwnd = self._find_own()
            if self.hwnd:
                break
            time.sleep(0.05)
        if not self.hwnd:
            raise SystemExit("the window never appeared for pid %d" % self.proc.pid)
        time.sleep(0.25)   # let WM_CREATE finish before anything is posted

    def _find_own(self):
        want = self.proc.pid
        found = []

        @ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
        def cb(hwnd, _):
            pid = ctypes.c_ulong(0)
            u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == want:
                buf = ctypes.create_unicode_buffer(64)
                u32.GetClassNameW(hwnd, buf, 64)
                if buf.value == "nibWindow":
                    found.append(hwnd)
            return 1

        u32.EnumWindows(cb, 0)
        return found[0] if found else 0

    # --- driving ---------------------------------------------------------------------------
    def type(self, s):
        for c in s:
            u32.PostMessageW(self.hwnd, WM_CHAR, ord("\r" if c == "\n" else c), 0)
        time.sleep(0.03 + 0.005 * len(s))

    def key(self, name, times=1):
        # a bare virtual key needs no modifier state, so posting it is faithful
        for _ in range(times):
            u32.PostMessageW(self.hwnd, WM_KEYDOWN, VK[name], 0)
            u32.PostMessageW(self.hwnd, WM_KEYUP, VK[name], 0)
        time.sleep(0.04)

    def cmd(self, name):
        u32.PostMessageW(self.hwnd, WM_NIB_CMD, CMD[name], 0)

    # --- reading ---------------------------------------------------------------------------
    def lines(self):
        if not os.path.exists(self.log):
            return []
        with open(self.log, encoding="utf-8", errors="replace") as f:
            return [l.rstrip("\n").split("\t") for l in f if l.strip()]

    def wait_for(self, kind, n_before, timeout=6.0):
        """Wait until a further line of `kind` has been appended. Polling the artefact rather than
        sleeping a guessed interval is what keeps this battery from being flaky."""
        end = time.time() + timeout
        while time.time() < end:
            got = [l for l in self.lines() if l and l[0] == kind]
            if len(got) > n_before:
                return got[-1]
            time.sleep(0.03)
        return None

    def count(self, kind):
        return len([l for l in self.lines() if l and l[0] == kind])

    def save(self, timeout=6.0):
        n = self.count("saved")
        self.cmd("save")
        return self.wait_for("saved", n, timeout)

    def close(self):
        u32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)
        try:
            self.proc.wait(timeout=8)
        except Exception:
            self.proc.kill()
            self.proc.wait(timeout=5)
        time.sleep(0.15)   # the window is gone before the next case looks for one


def read_bytes(p):
    with open(p, "rb") as f:
        return f.read()


def scratch(name):
    d = os.path.join(os.environ["TEMP"], "nib-drive")
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, name)
    if os.path.exists(p):
        os.remove(p)
    return p


LF = chr(10)
TWO_LINES = ("hello world" + LF + "second line" + LF).encode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=r"C:\nib\nib.exe")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    if not os.path.exists(a.exe):
        raise SystemExit("no such exe: " + a.exe)
    print("nib driver · %s" % a.exe)

    # ---- 1 · type, save, and the bytes on disk ------------------------------------------
    print(LF + "typing and saving")
    target, log = scratch("one.txt"), scratch("one.log")
    n = Nib(a.exe, target, log)
    # The process must be per-monitor DPI aware (SPEC 4.1.2). The QC of 2026-09-04 measured it
    # PROCESS_DPI_UNAWARE: GetDpiForWindow answered 96 and WM_DPICHANGED was dead code.
    aw = ctypes.c_int(-1)
    try:
        ctypes.windll.shcore.GetProcessDpiAwareness(ctypes.c_void_p(int(n.proc._handle)), ctypes.byref(aw))
    except Exception:
        pass
    check(aw.value == 2, "the process is per-monitor DPI aware (awareness %d, want 2)" % aw.value)
    n.type("hello world" + LF + "second line" + LF)
    row = n.save()
    check(row is not None, "the save command reached the window and the log says so: %s" % (row,))
    body = read_bytes(target) if os.path.exists(target) else b""
    check(body == TWO_LINES, "the bytes are exactly what was typed: %r" % body)
    check(row is not None and int(row[1]) == len(body), "the log's byte count matches the file")

    # ---- 2 · a selection typed over -----------------------------------------------------
    # The property is that the FIRST character both removes the selection and inserts itself:
    # one splice, one revision. Typing five characters is five revisions whether or not there was
    # a selection — the first version of this check asserted that wrongly, and the driver caught
    # it, which is the point of having one.
    print(LF + "selection")
    before_revs = int(row[2])
    n.cmd("top")
    n.cmd("end")
    n.cmd("sel_to_home")     # line one selected
    time.sleep(0.2)
    n.type("H")
    row2 = n.save()
    body = read_bytes(target)
    check(body == ("H" + LF + "second line" + LF).encode(),
          "one character typed over a selection replaced it: %r" % body)
    check(row2 is not None and int(row2[2]) == before_revs + 1,
          "and it cost ONE revision (%s -> %s)" % (before_revs, row2[2] if row2 else "?"))

    # ---- 3 · undo takes back a BURST, not a character ------------------------------------
    # The driver found this: undo used to unpick typing one character at a time, which is
    # technically correct and unusable. A burst typed without a pause is one thing a person did.
    print(LF + "undo and redo, by group")
    n.type("ELLO")           # continues the burst: same hand, contiguous, no pause
    n.save()
    check(read_bytes(target) == ("HELLO" + LF + "second line" + LF).encode(),
          "a burst of typing: %r" % read_bytes(target))
    # what the compiler has seen so far: the undo below must move the removed-bytes counters
    ni0 = n.count("ingest")
    n.cmd("ingest")
    irow0 = n.wait_for("ingest", ni0)
    rin0 = int(irow0[6]) if irow0 else -1
    nu = n.count("undo")
    n.cmd("undo")
    urow = n.wait_for("undo", nu)
    n.save()
    body = read_bytes(target)
    check(body == TWO_LINES,
          "ONE undo took back the whole burst, and the replaced selection with it: %r" % body)
    check(urow is not None and int(urow[1]) > before_revs + 5,
          "and the log GREW rather than shrank — undo is appended (%s)" % (urow[1] if urow else "?"))
    nr = n.count("redo")
    n.cmd("redo")
    n.wait_for("redo", nr)
    n.save()
    check(read_bytes(target) == ("HELLO" + LF + "second line" + LF).encode(),
          "and one redo put the whole burst back: %r" % read_bytes(target))
    # The undo removed HELLO (5 bytes) and the redo removed "hello world" (11): both are percepts.
    # The QC of 2026-09-04 watched an undo empty the document while these counters stood still.
    ni1 = n.count("ingest")
    n.cmd("ingest")
    irow1 = n.wait_for("ingest", ni1)
    rin1, rout1 = (int(irow1[6]), int(irow1[7])) if irow1 else (-1, -2)
    check(rin1 == rin0 + 16, "the undo and the redo were perceived as removals: %d -> %d removed bytes (want +16)" % (rin0, rin1))
    check(rin1 == rout1, "and every removed byte left the compiler: in %d == out %d" % (rin1, rout1))

    # ---- 4 · backspace and delete, posted as bare virtual keys --------------------------
    print(LF + "backspace and delete")
    n.cmd("top")
    n.cmd("end")
    n.key("back", 5)          # remove HELLO
    n.type("bye")
    n.save()
    check(read_bytes(target) == ("bye" + LF + "second line" + LF).encode(),
          "backspace then typing: %r" % read_bytes(target))
    n.cmd("top")
    n.key("delete", 3)
    n.save()
    check(read_bytes(target) == (LF + "second line" + LF).encode(),
          "delete forward: %r" % read_bytes(target))

    # ---- 5 · the Stage 0 falsifier, from inside the running window ----------------------
    print(LF + "the log replays")
    nrep = n.count("replay")
    n.cmd("replay")
    rrow = n.wait_for("replay", nrep)
    check(rrow is not None and rrow[1] == "1",
          "the window folded %s revisions and the text matched byte-exact" % (rrow[2] if rrow else "?"))

    # ---- 5b · Stage 1: the pad compiled a world while all of that was happening ----------
    # The resident does not exist yet. What must be true already is that everything typed since
    # the window opened was compiled, in order, with nothing lost -- because a percept dropped
    # between a keystroke and the mind is a turn reborn inside the loop.
    print(LF + "ingest")
    ni = n.count("ingest")
    n.cmd("ingest")
    irow = n.wait_for("ingest", ni)
    check(irow is not None, "the window reports its ingest arithmetic: %s" % (irow,))
    if irow is not None:
        percepts, dropped, tin, tout, pushed, rin, rout = (int(x) for x in irow[1:8])
        check(percepts > 0, "typing produced percepts (%d)" % percepts)
        check(tin == tout,
              "every byte that entered the compiler left it: in %d == out %d" % (tin, tout))
        check(rin == rout, "and every removed byte too: in %d == out %d" % (rin, rout))
        check(dropped == 0, "and none were dropped on the way to the ring (%d)" % dropped)
        check(pushed == percepts,
              "every percept reached the ring: %d pushed of %d" % (pushed, percepts))

    # ---- 6 · close and reopen ------------------------------------------------------------
    print(LF + "close and reopen")
    n.close()
    check(read_bytes(target) == (LF + "second line" + LF).encode(),
          "the file survived the close unchanged")
    log2 = scratch("two.log")
    n2 = Nib(a.exe, target, log2)
    n2.type("X")
    n2.save()
    body = read_bytes(target)
    check(body == ("X" + LF + "second line" + LF).encode(),
          "reopening loaded the saved file, and typing lands in it: %r" % body)
    n2.close()

    # ---- 7 · a file's own conventions are restored, not converted -----------------------
    print(LF + "CRLF and BOM are preserved")
    crlf = scratch("crlf.txt")
    with open(crlf, "wb") as f:
        f.write(b"\xef\xbb\xbfalpha\r\nbeta\r\n")
    log3 = scratch("three.log")
    n3 = Nib(a.exe, crlf, log3)
    n3.type("Z")
    n3.save()
    body = read_bytes(crlf)
    check(body.startswith(b"\xef\xbb\xbf"), "the UTF-8 BOM came back: %r" % body[:6])
    check(body == b"\xef\xbb\xbfZalpha\r\nbeta\r\n",
          "CRLF stayed CRLF and the edit landed where it was typed: %r" % body)
    n3.close()

    # ---- 8 · a path that does not exist yet is a new file --------------------------------
    print(LF + "a new file")
    fresh = scratch("fresh.txt")
    log4 = scratch("four.log")
    n4 = Nib(a.exe, fresh, log4)
    n4.type("made from nothing" + LF)
    n4.save()
    made = read_bytes(fresh) if os.path.exists(fresh) else None
    check(made == ("made from nothing" + LF).encode(),
          "a path that did not exist was created on save: %r" % (made,))
    n4.close()

    # ---- 9 · an astral character arrives as two WM_CHARs, and must land as one -----------
    # Windows delivers an emoji as a high surrogate then a low one. Converting either alone
    # produces U+FFFD, which is how the QC of 2026-09-04 found a smile on disk as two question
    # marks. An unpaired half is dropped, never substituted.
    print(LF + "an emoji, as Windows delivers it")
    emoji, log5 = scratch("emoji.txt"), scratch("five.log")
    n5 = Nib(a.exe, emoji, log5)
    u32.PostMessageW(n5.hwnd, WM_CHAR, 0xD83D, 0)
    u32.PostMessageW(n5.hwnd, WM_CHAR, 0xDE00, 0)
    u32.PostMessageW(n5.hwnd, WM_CHAR, 0xDE00, 0)    # a stray low half: nothing may come of it
    time.sleep(0.1)
    n5.save()
    got = read_bytes(emoji)
    check(got == b"\xf0\x9f\x98\x80",
          "two surrogate WM_CHARs became one four-byte character, and the stray half was dropped: %r" % got)
    n5.close()

    if not a.keep:
        for p in (target, crlf, fresh, emoji, log, log2, log3, log4, log5):
            try:
                os.remove(p)
            except OSError:
                pass

    print(LF + "%d passed, %d failed" % (passed, failed))
    return 3 if failed else 0


sys.exit(main())
