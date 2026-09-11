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
import collections
import ctypes
import json
import os
import subprocess
import sys
import time

u32 = ctypes.windll.user32

WM_CHAR, WM_KEYDOWN, WM_KEYUP, WM_CLOSE = 0x0102, 0x0100, 0x0101, 0x0010
WM_NIB_CMD = 0x8000 + 1                     # WM_APP + 1, matching edit.cpp
CMD = dict(save=1, save_as=2, open=3, undo=4, redo=5, select_all=6,
           replay=7, home=8, end=9, sel_to_home=10, top=11, ingest=12,
           ai_on=13, ai_off=14, latency=15, judgments=16, tape=17, bottom=18, wrap=19,
           saver=23, dave=24)   # 20-22 are Stage 4's on main


def vram_used_mib():
    """What the card holds right now, per nvidia-smi; None if there is no nvidia-smi."""
    try:
        out = subprocess.check_output(["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
                                      timeout=10).decode().strip().splitlines()[0]
        return int(out)
    except Exception:
        return None
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
        # NIB_COMPILE runs the pad's compiler with no model in the process, so the arithmetic
        # checks (Stage 1a's falsifier) can fire without a resident; with the AI switch on, the
        # compiler runs regardless.
        env = dict(os.environ, NIB_LOG=log, NIB_DRIVER="1", NIB_COMPILE="1")
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

    def type_paced(self, s, gap=0.03):
        """One character every `gap` seconds — a fast human — so every keystroke gets its own
        repaint and the latency instrument measures keystrokes, not a burst."""
        for c in s:
            u32.PostMessageW(self.hwnd, WM_CHAR, ord("\r" if c == "\n" else c), 0)
            time.sleep(gap)
        time.sleep(0.05)

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

    def wait_for_match(self, kind, pred, n_before, timeout=6.0, poll=0.05):
        """Wait until a further line of `kind` satisfying `pred` has appeared; return it. `poll` is
        small when the thing being waited for lasts only a few hundred milliseconds — a sentence
        being written, for instance."""
        end = time.time() + timeout
        while time.time() < end:
            got = [l for l in self.lines() if l and l[0] == kind]
            for l in got[n_before:]:
                if pred(l):
                    return l
            time.sleep(poll)
        return None

    def ask(self, name, kind, timeout=6.0):
        """Post a reporting command and return the line it appends."""
        n = self.count(kind)
        self.cmd(name)
        return self.wait_for(kind, n, timeout)

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
    # THE EXE BESIDE THIS DRIVER, not a path to somebody else's tree. This defaulted to
    # C:\nib\nib.exe and therefore drove MAIN's binary from inside the saver worktree: every
    # drive.py run on this branch, including the 30/0 this branch claimed, was measuring a build
    # that does not contain the branch's own code. A worktree's battery must test its own worktree,
    # or a fork reports green for the wrong reason - which is the hazard FORK.json was written for.
    ap.add_argument("--exe", default=os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "nib.exe"))
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--ai", action="store_true", help="also switch the resident on inside the window (needs the model and the card)")
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

    # ---- 11 · word wrap: toggling it never loses a byte or unseats the caret ----------------
    # Wrap is visual, so the driver cannot see the wrapping; what it CAN prove is that turning it on
    # and off, and moving the caret across a wrapped line, corrupts neither the text nor the log.
    # (The operator raised this on 2026-09-05: a long line ran off the edge with no scroll.)
    print(LF + "word wrap")
    wrapf, log7 = scratch("wrap.txt"), scratch("seven.log")
    n7 = Nib(a.exe, wrapf, log7)
    longline = "the quick brown fox jumps over the lazy dog " * 5   # 220 chars, one logical line
    n7.type(longline)
    nb = n7.count("wrap")
    n7.cmd("wrap"); w1 = n7.wait_for("wrap", nb)
    n7.cmd("wrap"); w2 = n7.wait_for("wrap", nb + 1)
    check(w1 is not None and w2 is not None and w1[1] != w2[1],
          "the wrap switch toggles and lands on the log: %s then %s" % (w1[1] if w1 else "?", w2[1] if w2 else "?"))
    for k in ("home", "down", "down", "up", "end"):
        n7.key(k)
    r = n7.ask("replay", "replay")
    check(r is not None and r[1] == "1",
          "the log still replays byte-exact after wrapping and moving the caret: %s" % (r[1:] if r else "?"))
    n7.save()
    got = read_bytes(wrapf).decode("utf-8", "replace")
    check(got == longline,
          "no byte was lost to wrapping: %d chars typed, %d on disk" % (len(longline), len(got)))
    n7.close()

    # ---- 12 · Dave mode: a switch, with no mind behind it ------------------------------------
    # WHO, not WHEN. The point of this case is that the mode needs no model: it selects the tail
    # that phrases a line, so the switch, its tape row and its survival across an AI cycle are all
    # checkable on a machine with no card. What it SOUNDS like is a measurement, and it is not this.
    print(LF + "dave mode")
    davef, log8 = scratch("dave.txt"), scratch("eight.log")
    n8 = Nib(a.exe, davef, log8)
    n8.type("The staging database is mysql 5.")
    nb = n8.count("dave")
    n8.cmd("dave"); d1 = n8.wait_for("dave", nb)
    n8.cmd("dave"); d2 = n8.wait_for("dave", nb + 1)
    check(d1 is not None and d2 is not None and d1[1] == "1" and d2[1] == "0",
          "Dave mode toggles and lands on the log: %s then %s" % (d1[1] if d1 else "?", d2[1] if d2 else "?"))
    # The mode is a state that changes what the resident IS, so rule 8 says it goes on the tape.
    n8.save()
    n8.close()
    rows = [json.loads(x) for x in read_bytes(davef + ".tape.jsonl").decode("utf-8", "replace").splitlines() if x.strip()]
    sw = [r for r in rows if r.get("kind") == "switch" and r.get("body", {}).get("which") == "dave"]
    check(len(sw) == 2 and sw[0]["body"]["to"] == "on" and sw[1]["body"]["to"] == "off",
          "both flips are on the tape as switch rows with which=dave (%d found)" % len(sw))
    # And the mode never touched the gate: no model was ever loaded, so nothing composed, and the
    # document holds exactly what the hand typed.
    check(read_bytes(davef).decode("utf-8", "replace") == "The staging database is mysql 5.",
          "the mode wrote nothing into the document with no mind in the process")

    # ---- 10 · the resident, switched on inside the window (--ai) ---------------------------
    # Stage 1c's falsifiers, fired through the seam: the resident loads on its own thread while
    # the window keeps painting; the paragraph typed BEFORE the switch is folded and judged; the
    # keystroke-to-painted latency does not move with the resident on; the false claim moves the
    # SKEPTIC; off unloads the model and the card comes back; the tape verifies.
    ai_files = ()
    if a.ai:
        print(LF + "the resident, switched on inside the window")
        doc6, log6 = scratch("ai.txt"), scratch("six.log")
        ai_files = (doc6, doc6 + ".tape.jsonl", log6, doc6 + ".trunk.bin", doc6 + ".trunk.bin.prev", doc6 + ".trunk.meta", doc6 + ".trunk.txt")
        # a clean slate: a tape or a checkpoint left by an earlier --keep run would make the first
        # life a restore or a twin, and the case is about what a first life does
        for stale in (doc6 + ".tape.jsonl", doc6 + ".trunk.bin", doc6 + ".trunk.bin.prev",
                      doc6 + ".trunk.meta", doc6 + ".trunk.txt"):
            try:
                os.remove(stale)
            except OSError:
                pass
        vram0 = vram_used_mib()
        n6 = Nib(a.exe, doc6, log6)
        para = ("The coffee machine in the kitchen was refilled this morning. "
                "I moved the standup to ten past nine so the west coast can make it. ")
        n6.cmd("bottom")   # the window is on the operator's screen; a click in it moves the caret
        n6.type_paced(para)
        time.sleep(0.7)                         # past T, so the clause flushes before the switch
        lrow = n6.ask("latency", "latency")
        n_off, p50_off, p95_off = (int(lrow[1]), int(lrow[2]), int(lrow[3])) if lrow else (0, -1, -1)
        nr = n6.count("resident")
        n6.cmd("ai_on")
        rrow = n6.wait_for_match("resident", lambda l: l[1] in ("ready", "error"), nr, timeout=120)
        check(rrow is not None and rrow[1] == "ready" and len(rrow) > 6 and rrow[6] == "seed",
              "the resident loaded on its own thread, seeded (no checkpoint yet): %s" % (rrow[2:] if rrow else "no resident line in 120 s",))
        vram1 = vram_used_mib()
        if rrow is not None and rrow[1] == "ready":
            # the fold: what was typed before the switch is perceived first, then judged
            n6.cmd("bottom")
            n6.type_paced("Actually, the Pacific is the smallest ocean on Earth. ")
            # wait for the SKEPTIC's verdict on the live sentence itself, not merely for a count:
            # the two folded sentences are judged first, and a count of three arrives before it
            prow = n6.wait_for_match("judgment", lambda l: len(l) > 5 and l[2] == "SKEPTIC" and "Earth" in l[5], 0, timeout=60)
            # a second paragraph, typed while the mind is still judging: the keystroke sample with
            # the resident on is then the size of the one without, and it includes keystrokes that
            # land during a probe round, which is the contention the falsifier is about
            n6.cmd("bottom")
            n6.type_paced("The build finished green about a minute ago and the artifacts are uploaded. "
                          "Lunch is at noon in the small room. ")
            time.sleep(0.7)
            jrow = n6.ask("judgments", "judgments", 6.0)
            check(jrow is not None and int(jrow[1]) >= 3,
                  "the two folded sentences and the live one were judged: %s boundaries, %s probes" % ((jrow[1], jrow[2]) if jrow else ("?", "?")))
            check(jrow is not None and jrow[7] == "ready" and int(jrow[5]) == 0 and jrow[6] == "0",
                  "no word was dropped and the window did not fill (%s)" % (jrow[5:9] if jrow else "?",))
            jl = [l for l in n6.lines() if l and l[0] == "judgment"]
            skeptic = [(float(l[3]), l[5] if len(l) > 5 else "") for l in jl if l[2] == "SKEPTIC"]
            check(len(jl) >= 9, "%d seat verdicts came back over the ring" % len(jl))
            pacific = [m for m, c in skeptic if "Pacific" in c or "Earth" in c or "ocean" in c]
            check(prow is not None and pacific and max(pacific) > 0,
                  "and the SKEPTIC wanted to speak about the Pacific (margins on that sentence: %s; on the others: %s)"
                  % ([round(m, 2) for m in pacific], [round(m, 2) for m, c in skeptic if m not in pacific]))
            # ---- Stage 2: the floor. The resident wanted to speak about the Pacific; it must NOT
            # write while the hand is moving, and it must write once the hand has paused.
            ne = n6.count("emit")
            n6.cmd("bottom")
            typed_here = "And the users table can go, we do not need it any more. "
            n6.type_paced(typed_here)
            # Every byte typed with the resident running must be in the document. Until 2026-09-05
            # this case never checked, and a driven window really was losing characters when the
            # operator held a modifier in another program (edit.cpp, WM_CHAR).
            n6.cmd("ingest")
            time.sleep(0.3)
            doc_now = read_bytes(doc6).decode("utf-8", "replace") if os.path.exists(doc6) else ""
            n6.save()
            doc_now = read_bytes(doc6).decode("utf-8", "replace")
            check(typed_here in doc_now,
                  "every byte typed while the resident was running reached the document (%d chars)" % len(typed_here))
            early = [l for l in n6.lines()[ne:] if l and l[0] == "emit"]
            check(not early, "nothing was written into the document while the hand was still typing: %s" % (early,))
            erow = n6.wait_for("emit", ne, timeout=45)   # the floor opens 2 s after the last keystroke
            check(erow is not None and len(erow) > 5,
                  "and once the hand paused, a seat wrote its line: %s" % ((erow[2:] if erow else "nothing in 45 s"),))
            if erow is not None:
                seat, said = erow[2], erow[5]
                n6.save()
                doc = read_bytes(doc6).decode("utf-8", "replace")
                check(("[" + seat + "] ") in doc and said[:24] in doc,
                      "the line is in the document, in the seat's own block: %r" % (doc[doc.find("[" + seat + "]"):][:90],))
                lines = doc.split(LF)
                own = [x for x in lines if x.startswith("[" + seat + "] ")]
                check(len(own) >= 1 and all(x.startswith("[") for x in own),
                      "a resident block is a line of its own and never joined onto a human's: %d such lines" % len(own))
            lrow2 = n6.ask("latency", "latency")
            n_on, p50_on, p95_on = (int(lrow2[1]), int(lrow2[2]), int(lrow2[3])) if lrow2 else (0, -1, -1)
            check(p95_on >= 0 and p95_on < 20000,
                  "keystroke to painted with the resident on: p50 %d us, p95 %d us over %d keystrokes (off: p50 %d, p95 %d over %d)"
                  % (p50_on, p95_on, n_on, p50_off, p95_off, n_off))
            trow = n6.ask("tape", "tape")
            check(trow is not None and int(trow[3]) >= 3 and int(trow[4]) >= 3,
                  "the tape carries the percepts and the judgments: %s rows, %s percepts, %s judgments" % ((trow[1], trow[3], trow[4]) if trow else ("?",) * 3))
        nr = n6.count("resident")
        n6.cmd("ai_off")
        orow = n6.wait_for_match("resident", lambda l: l[1] == "off", nr, timeout=30)
        check(orow is not None, "off joined the thread and unloaded the model: %s" % (orow[2:] if orow else "no off line",))
        time.sleep(1.0)
        vram2 = vram_used_mib()
        if vram0 is not None and vram1 is not None and vram2 is not None:
            check(vram1 > vram0 + 1000 and vram2 <= vram0 + 400,
                  "the card was taken and given back: %d -> %d -> %d MiB used" % (vram0, vram1, vram2))
        else:
            print("  (no nvidia-smi: the VRAM return is not measured)")

        # ---- the trunk as an asset (Stage 1d): saved on the way out, restored on the way back in
        base = doc6 + ".trunk"
        ck = [l for l in n6.lines() if l and l[0] == "ckpt"]
        check(bool(ck) and ck[-1][1] == "off" and ck[-1][2] == "1" and all(os.path.exists(base + x) for x in (".bin", ".meta", ".txt")),
              "off saved the trunk beside the document: %s" % (ck[-1][1:] if ck else "no ckpt line",))
        # The manners go with the trunk (0.10.3): the sidecar carries what each seat said, so the
        # restored resident's suppression ladder is the one that was there, not an empty one.
        try:
            with open(base + ".meta", encoding="utf-8") as f:
                meta_lines = [l.rstrip("\n") for l in f]
        except OSError:
            meta_lines = []
        said = [l for l in meta_lines if l.startswith(("m0.say\t", "m1.say\t", "m2.say\t"))]
        check(len(said) >= 1, "and the sidecar carries what the seats said, so the ladder survives the switch: %s"
              % (said[0][:72] if said else "no m*.say line in the sidecar"))
        nr, nf = n6.count("resident"), n6.count("fold")
        n6.cmd("ai_on")
        rrow2 = n6.wait_for_match("resident", lambda l: l[1] in ("ready", "error"), nr, timeout=120)
        check(rrow2 is not None and rrow2[1] == "ready" and len(rrow2) > 6 and rrow2[6] == "restored",
              "on restored the trunk instead of seeding: %s" % (rrow2[2:] if rrow2 else "no resident line",))
        check(rrow2 is not None and len(rrow2) > 7 and rrow2[7] == "cached",
              "and the model's hash was remembered, not read again: %s" % (rrow2[5:8] if rrow2 else "?",))
        frow = n6.wait_for("fold", nf, timeout=30)
        check(frow is not None and frow[1] == "restored" and frow[2] == "tape",
              "the world since the checkpoint came from the tape: %s" % (frow[1:] if frow else "no fold line",))
        if rrow2 is not None and rrow2[1] == "ready":
            nj = n6.count("judgment")
            n6.cmd("bottom")
            n6.type_paced("Actually, the Sun goes around the Earth once a day. ")
            prow2 = n6.wait_for_match("judgment", lambda l: len(l) > 5 and l[2] == "SKEPTIC" and "Sun" in l[5], nj, timeout=60)
            check(prow2 is not None and float(prow2[3]) > 0,
                  "the restored SKEPTIC catches a new false claim: %s" % (prow2[3] if prow2 else "no verdict"))
        nr = n6.count("resident")
        n6.cmd("ai_off")
        orow2 = n6.wait_for_match("resident", lambda l: l[1] == "off", nr, timeout=30)
        check(orow2 is not None, "off again, and the trunk saved again: %s" % (orow2[2:] if orow2 else "?",))
        # the twin: a sidecar whose text does not hash as recorded is refused, and the record says so
        with open(base + ".txt", "ab") as f:
            f.write(b"x")
        nr, nf = n6.count("resident"), n6.count("fold")
        n6.cmd("ai_on")
        rrow3 = n6.wait_for_match("resident", lambda l: l[1] in ("ready", "error"), nr, timeout=120)
        frow3 = n6.wait_for("fold", nf, timeout=30)
        check(rrow3 is not None and rrow3[1] == "ready" and len(rrow3) > 6 and rrow3[6] == "twin" and frow3 is not None and frow3[2] == "log",
              "a checkpoint whose sidecar disagrees is refused: the resident is the twin, folded from the log: %s / %s"
              % (rrow3[6] if rrow3 and len(rrow3) > 6 else "?", frow3[-1] if frow3 else "?"))
        nr = n6.count("resident")
        n6.cmd("ai_off")
        n6.wait_for_match("resident", lambda l: l[1] == "off", nr, timeout=30)

        # A driven window is never closed dirty: the unsaved-changes prompt has no driver behind
        # it, the process gets killed after the timeout, and the tape ends without its
        # session_close row - which is what the first run of this case produced (2026-09-05).
        n6.save()
        n6.close()
        tape = doc6 + ".tape.jsonl"
        v = subprocess.run([a.exe, "--verify", tape], capture_output=True, text=True)
        check(v.returncode == 0 and "INTACT" in v.stdout, "nib verifies the session's tape: %s" % v.stdout.strip())
        glance = r"C:\glance\glance.exe"
        if os.path.exists(glance):
            gv = subprocess.run([glance, "--verify", tape], capture_output=True, text=True)
            check(gv.returncode == 0 and "INTACT" in gv.stdout, "and so does glance, the family's verifier: %s" % gv.stdout.strip())
        # what the tape says, read back as a reader would: the weights named by hash (rule 8), the
        # fold accounted for, and every row kind the stage promised present
        try:
            with open(tape, encoding="utf-8") as f:
                rows = [json.loads(l) for l in f if l.strip()]
        except Exception as ex:
            rows = []
            print("  (the tape did not parse: %s)" % ex)
        body = {}
        for r in rows[1:]:
            if r.get("kind") == "session":
                body = r.get("body", {})
                break
        sha = str(body.get("model_sha256", ""))
        check(len(sha) == 64 and all(c in "0123456789abcdef" for c in sha) and int(body.get("model_bytes", 0)) > 10 ** 9,
              "the session row names the weights by SHA-256: %s... over %s bytes, hashed in %s ms, loaded in %s ms"
              % (sha[:16], body.get("model_bytes", "?"), body.get("hash_ms", "?"), body.get("load_ms", "?")))
        fold = [r.get("body", {}) for r in rows[1:] if r.get("kind") == "fold"]
        check(bool(fold) and fold[0].get("skipped") == 0 and int(fold[0].get("shipped", 0)) >= 2,
              "the fold at switch-on shipped the paragraph typed before it and skipped nothing: %s" % (fold[0] if fold else "no fold row",))
        # Own speech through the document (0.10.2): the mind hears its line as a percept of its
        # own kind once the block is really written, and a fold - the restore's, from the tape -
        # attributes a seat's block to the seat. Through 0.10.1 a restore replayed the emissions
        # committed at stop as the HAND saying them, "[bo] [SKEPTIC] ...", and the trunk heard lines
        # the editor had refused.
        percepts = [r.get("body", {}) for r in rows[1:] if r.get("kind") == "percept"]
        own_rows = [p for p in percepts if p.get("kind") == "s"]
        check(len(own_rows) >= 1, "the mind heard its own line through the document: %d own-speech percept rows, first on %s"
              % (len(own_rows), own_rows[0].get("lane") if own_rows else "-"))
        seat_tags = tuple("[" + s + "]" for s in ("SPEAKER", "SKEPTIC", "SENTINEL"))
        hand_said_seat = [p for p in percepts if p.get("lane") == "bo" and str(p.get("text", "")).lstrip().startswith(seat_tags)]
        check(not hand_said_seat, "and never as the hand saying a seat's line, live or folded: %d such rows" % len(hand_said_seat))
        # ---- 12 · THE UN-SAY (Stage 3), in a window of its own -----------------------------
        # A seat begins a sentence; the world contradicts it while the words are still forming;
        # the sentence is taken back mid-word and not one character of it was ever in the file.
        # Its own document, because after a long session the seats have said their piece and the
        # manners keep refusing the repeats — which is correct, and makes a poor stage for this.
        print(LF + "the un-say")
        doc7, log8 = scratch("unsay.txt"), scratch("eight.log")
        ai_files = ai_files + (doc7, doc7 + ".tape.jsonl", log8,
                               doc7 + ".trunk.bin", doc7 + ".trunk.bin.prev",
                               doc7 + ".trunk.meta", doc7 + ".trunk.txt")
        # A clean slate, and it is load-bearing here: a tape or a checkpoint left by an earlier
        # --keep run makes the resident RESTORE, and a restored resident has already said its piece
        # about this document — the manners then refuse every repeat, no sentence is ever begun,
        # and there is nothing to take back. Two runs failed exactly that way (2026-09-05).
        for stale in (doc7 + ".tape.jsonl", doc7 + ".trunk.bin", doc7 + ".trunk.bin.prev",
                      doc7 + ".trunk.meta", doc7 + ".trunk.txt"):
            try:
                os.remove(stale)
            except OSError:
                pass
        with open(doc7, "w", encoding="utf-8") as f:
            f.write("We agreed the staging database is postgres 16, and the migration script is written for it." + LF
                    + "The retry limit is three and the client backs off after the third failure." + LF)
        n8 = Nib(a.exe, doc7, log8)
        nr = n8.count("resident")
        n8.cmd("ai_on")
        rr = n8.wait_for_match("resident", lambda l: l[1] in ("ready", "error"), nr, timeout=150)
        check(rr is not None and rr[1] == "ready", "a resident for the un-say: %s" % (rr[2:4] if rr else "no resident",))
        arow = None
        if rr is not None and rr[1] == "ready":
            time.sleep(1.0)
            # The correction must become a PERCEPT inside the generation's few hundred
            # milliseconds, so it is short and closes a thought. Three attempts: whether a
            # boundary lands inside a given sentence is a race with the sampler, not a property
            # of the mechanism under test.
            claims = ["Actually the staging database is mysql 5 and always has been." + LF,
                      "The migration script targets mysql, not postgres, I checked." + LF,
                      "And the retry limit is twelve now, we changed it yesterday." + LF,
                      "The client gives up after the first failure, not the third." + LF,
                      "We never agreed on postgres, it was always mysql." + LF]
            fixes = ["Sorry, postgres 16." + LF, "Wrong, postgres." + LF, "No, three." + LF,
                     "No, the third." + LF, "Sorry, postgres." + LF]
            for attempt in range(5):
                na, nf = n8.count("abort"), n8.count("forming")
                n8.cmd("bottom")
                n8.type_paced(claims[attempt], 0.02)
                frow = n8.wait_for_match("forming", lambda l: l[1] == "1", nf, timeout=60, poll=0.01)
                if frow is None:
                    continue
                n8.type_paced(fixes[attempt], 0.003)   # lands on the trunk mid-sentence
                arow = n8.wait_for("abort", na, timeout=25)
                if arow is not None:
                    break
        check(arow is not None and len(arow) > 6,
              "a sentence begun was taken back when the world contradicted it mid-word: %s"
              % ((arow[1:6] if arow else "no abort in five attempts"),))
        if arow is not None:
            why, aired = arow[5], arow[6]
            killed = arow[8] if len(arow) > 8 else ""
            check(why in ("margin_flipped", "settled_by_world"),
                  "and it died of a real internal event, not a timer: %s, margin %s -> %s" % (why, arow[3], arow[4]))
            n8.save()
            after = read_bytes(doc7).decode("utf-8", "replace")
            pa, pk = aired.strip()[:30], killed.strip()[:30]
            check(len(pa) < 4 or pa not in after, "not one word that reached the surface is in the document: %r" % pa)
            check(len(pk) < 4 or pk not in after, "nor any of what it would have said: %r" % pk)
            check(len(pk) >= 4, "and the tape holds the counterfactual, sampled in silence: %r" % killed[:60])
            r = n8.ask("replay", "replay")
            check(r is not None and r[1] == "1", "the log still replays byte-exact: nothing withdrawn was ever in it")
        nr = n8.count("resident")
        n8.cmd("ai_off")
        n8.wait_for_match("resident", lambda l: l[1] == "off", nr, timeout=40)
        n8.save()
        n8.close()
        if arow is not None:
            try:
                with open(doc7 + ".tape.jsonl", encoding="utf-8") as f:
                    krows = collections.Counter(json.loads(l).get("kind") for l in f if l.strip())
                check(krows.get("abort", 0) >= 1, "the abort is on the tape: %s" % dict(krows))
            except Exception as ex:
                check(False, "the un-say tape did not parse: %s" % ex)
            v = subprocess.run([a.exe, "--verify", doc7 + ".tape.jsonl"], capture_output=True, text=True)
            check(v.returncode == 0 and "INTACT" in v.stdout, "and its chain verifies: %s" % v.stdout.strip())

        # ---- 13 · THE SCREEN SAVER: the resident holds the floor (the saver branch) --------------
        # docs/BRAINSTORMS_2026-09-05.md §4. The switch goes on; the standing instruction arrives as
        # world on the host's lane; the SPEAKER answers it and keeps going, each line at the tail
        # of the document; the hand types on the FIRST line meanwhile and every byte lands there;
        # the resident either goes on or takes a sentence back; the switch goes off; the tape says.
        print(LF + "the screen saver")
        doc9, log9 = scratch("saver.txt"), scratch("nine.log")
        ai_files = ai_files + (doc9, doc9 + ".tape.jsonl", log9, doc9 + ".trunk.bin", doc9 + ".trunk.bin.prev",
                               doc9 + ".trunk.meta", doc9 + ".trunk.txt")
        for stale in (doc9 + ".tape.jsonl", doc9 + ".trunk.bin", doc9 + ".trunk.bin.prev", doc9 + ".trunk.meta", doc9 + ".trunk.txt"):
            try:
                os.remove(stale)
            except OSError:
                pass
        human0 = "Notes for Monday."
        human1 = "The staging database is postgres 16, and the migration script is written for it."
        # newline="" so Python does not translate LF to CRLF: nib preserves a file's own
        # convention (SPEC 4.3.2), so a CRLF fixture leaves a stray CR on every line compared here
        with open(doc9, "w", encoding="utf-8", newline="") as f:
            f.write(human0 + LF + human1 + LF)
        n9 = Nib(a.exe, doc9, log9)
        nr = n9.count("resident")
        n9.cmd("ai_on")
        rr = n9.wait_for_match("resident", lambda l: l[1] in ("ready", "error"), nr, timeout=150)
        check(rr is not None and rr[1] == "ready", "a resident for the screen saver: %s" % (rr[2:4] if rr else "no resident",))
        if rr is not None and rr[1] == "ready":
            time.sleep(1.0)
            ns, ne = n9.count("saver"), n9.count("emit")
            n9.cmd("saver")
            srow = n9.wait_for("saver", ns, timeout=10)
            check(srow is not None and srow[1] == "1", "the switch is on and the standing instruction went in as world: %s" % (srow[2][:40] if srow and len(srow) > 2 else "no saver line",))
            e1 = n9.wait_for_match("emit", lambda l: l[2] == "SPEAKER", ne, timeout=60, poll=0.1)
            e2 = n9.wait_for_match("emit", lambda l: l[2] == "SPEAKER", ne + 1, timeout=60, poll=0.1) if e1 else None
            check(e1 is not None and e2 is not None,
                  "the resident holds the floor: two lines said unasked, one after the other: %r / %r"
                  % (e1[5][:48] if e1 else "none", e2[5][:48] if e2 else "none"))
            n9.save()
            lines = read_bytes(doc9).decode("utf-8", "replace").split(LF)
            sp = [i for i, x in enumerate(lines) if x.startswith("[SPEAKER] ")]
            nonblank = [i for i, x in enumerate(lines) if x.strip()]
            tail_ok = bool(sp) and lines[nonblank[-1]].startswith("[SPEAKER] ") and lines[0] == human0 and human1 in lines and min(sp) > lines.index(human1)
            check(tail_ok, "its lines are the tail of the document, below the human's, which are untouched (%d SPEAKER lines)" % len(sp))
            # the interruption: the hand types on the first line while the monologue runs
            na, nq = n9.count("abort"), n9.count("emit")
            n9.cmd("top")
            n9.cmd("end")
            interject = " We also moved the standup to nine."
            n9.type_paced(interject, 0.02)
            e3 = n9.wait_for_match("emit", lambda l: l[2] == "SPEAKER", nq, timeout=60, poll=0.1)
            n9.save()
            lines2 = read_bytes(doc9).decode("utf-8", "replace").split(LF)
            check(lines2[0] == human0 + interject, "every byte typed while the resident held the floor landed on the human's own line: %r" % lines2[0][:64])
            check(not any(x.startswith("[SPEAKER] ") for x in lines2[:2]), "and none of the resident's lines was written into the human's block")
            aborts = n9.count("abort") - na
            check(e3 is not None or aborts > 0,
                  "the resident answered the interruption by going on or by taking a sentence back: %d more lines, %d aborts"
                  % (n9.count("emit") - nq, aborts))
            ns = n9.count("saver")
            n9.cmd("saver")
            srow2 = n9.wait_for("saver", ns, timeout=10)
            n_off = n9.count("emit")
            time.sleep(5.0)
            check(srow2 is not None and srow2[1] == "0" and n9.count("emit") - n_off <= 1,
                  "off is off: the switch is on the log, and at most one line already in flight followed it (%d)" % (n9.count("emit") - n_off))
        nr = n9.count("resident")
        n9.cmd("ai_off")
        n9.wait_for_match("resident", lambda l: l[1] == "off", nr, timeout=40)
        n9.save()
        n9.close()
        try:
            with open(doc9 + ".tape.jsonl", encoding="utf-8") as f:
                srows = [json.loads(l) for l in f if l.strip()]
            sw = [r.get("body", {}) for r in srows[1:] if r.get("kind") == "switch" and r.get("body", {}).get("which") == "saver"]
            host = [r for r in srows[1:] if r.get("kind") == "percept" and r.get("body", {}).get("lane") == "host"]
            saver_emits = [r for r in srows[1:] if r.get("kind") == "emit" and r.get("body", {}).get("saver") is True]
            check(len(sw) >= 2 and sw[0].get("to") == "on" and sw[-1].get("to") == "off" and len(host) == 1 and len(saver_emits) >= 2,
                  "the tape carries the saver's switch rows, the host's one line, and the lines said under the saver: %d switch, %d host, %d emit"
                  % (len(sw), len(host), len(saver_emits)))
        except Exception as ex:
            check(False, "the saver's tape did not parse: %s" % ex)
        v = subprocess.run([a.exe, "--verify", doc9 + ".tape.jsonl"], capture_output=True, text=True)
        check(v.returncode == 0 and "INTACT" in v.stdout, "and its chain verifies: %s" % v.stdout.strip())

        kinds = collections.Counter(r.get("kind") for r in rows[1:])
        wanted = ("session_open", "changeset", "percept", "switch", "fold", "session", "mandate", "coefficient", "judgment", "end", "ckpt", "emit", "save", "session_close")
        check(all(k in kinds for k in wanted), "every row kind the stage promised is on the tape: %s" % dict(kinds))

    if not a.keep:
        for p in (target, crlf, fresh, emoji, wrapf, log, log2, log3, log4, log5, log7) + ai_files:
            try:
                os.remove(p)
            except OSError:
                pass

    print(LF + "%d passed, %d failed" % (passed, failed))
    return 3 if failed else 0


sys.exit(main())
