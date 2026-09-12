#!/usr/bin/env python3
"""Assemble a clickable, relocatable DAVE MONOLOGUE bundle from this tree's nib.exe.

The bundle is a folder. Double-click `Dave monologue.cmd` (or the .lnk beside it) and nib opens
full screen with the mind loading, Dave on and the screen saver armed: the resident holds the
floor and talks, as Dave, until you type. Nothing is installed and nothing is registered.

What goes in, and why:
  nib.exe              this tree's binary, its serve pin checked before it is copied
  the runtime DLLs     llama.cpp's llama/ggml, every ggml-cpu variant (the loader picks by score),
                       ggml-cuda and its three CUDA libraries, libomp - and NEVER ggml-rpc.dll,
                       which imports ws2_32 and is the DLL nib's runtime gate exists to keep out
  nib.theme            the operator's palette, a screensaver-sized font, `llama_dir .` (the bundle
                       itself: a relative theme path is anchored to the exe, not the working
                       directory), the model by absolute path unless --with-model copied it in
  runs/                where an untitled monologue's tape lands, and the model-hash cache
  Dave monologue.cmd   `start "" "%~dp0nib.exe" --monologue` - portable, no path baked in
  Dave monologue.lnk   the same, with a working directory, for a taskbar or the desktop
  About.cmd            `nib.exe --about` and a pause: the pin, the backends, the module gate
  README.txt

Usage:
  python tools/make_monologue_bundle.py                       -> dist/dave-monologue/
  python tools/make_monologue_bundle.py --out D:/Dave --force
  python tools/make_monologue_bundle.py --with-model          (copies the 6 GB model in; the bundle
                                                               then needs nothing outside itself
                                                               but a card)
  python tools/make_monologue_bundle.py --no-dlls             (reference C:/llama.cpp instead of
                                                               copying 1.1 GB; not relocatable)
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import os
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "nib.exe")

# The runtime, by allowlist. Anything not named here stays out, and ggml-rpc.dll is refused by name
# even if a future pattern would admit it.
DLL_PATTERNS = ["llama.dll", "ggml.dll", "ggml-base.dll", "ggml-cpu-*.dll", "ggml-cuda.dll",
                "cudart64_*.dll", "cublas64_*.dll", "cublasLt64_*.dll", "libomp140.x86_64.dll"]
DLL_REFUSED = ["ggml-rpc.dll"]

CMD_LAUNCH = ("@echo off\r\n"
              "rem Dave holds the floor: nib opens full screen, the mind loads, and he talks until you type.\r\n"
              "rem Ctrl+Shift+M ends the monologue, Ctrl+Shift+A switches the mind off, close the window to quit.\r\n"
              "start \"\" \"%~dp0nib.exe\" --monologue\r\n")

CMD_ABOUT = ("@echo off\r\n"
             "rem What this build is: the version, the serve pin, which DLLs came up, and the module gate.\r\n"
             "\"%~dp0nib.exe\" --about --llama-dir \"%~dp0.\"\r\n"
             "echo.\r\n"
             "pause\r\n")

README = """DAVE MONOLOGUE - the screen saver with a person in it
=====================================================

Double-click "Dave monologue.cmd" (or the .lnk). nib opens full screen. The mind loads on its own
thread (a few seconds; the status line says so), then a single line arrives on the pad as world -
"[host] Watcher, talk to me about anything until I interrupt." - and the resident answers it and
keeps going, one sentence at a time, in Dave's voice, each line appended as its own block at the
tail of the page. His margins (how much HE wanted to say each line) are on the record beside each.

  Typing anywhere            is the interruption. He sees it between two tokens and either goes
                             on, takes his sentence back mid-word, or answers you.
  Ctrl+Shift+M               ends the monologue (the switch is on the tape); again restarts it.
  Ctrl+Shift+D               Dave off / on (the seats speak as themselves when he is off).
  Ctrl+Shift+A               switches the mind off; the card comes back.
  Close the window           quits. Nothing is saved unless you asked for a file.

What it needs
  An NVIDIA card with about 7 GB free (the model is a 9B at Q5, 16k context). On the CPU it runs
  47x slower and is not worth watching. The model file is named in nib.theme; if it is not there,
  the status line says so and nothing else happens.

What it never does
  Nothing leaves this machine. The binary links no network library, the runtime is loaded by name
  so the one llama.cpp DLL that carries a socket (ggml-rpc.dll) is not in this folder, and the
  resident refuses to start if a network module is ever in the process. About.cmd prints that
  receipt. The tape of every session is in runs/ (or beside the file you named), hash-chained;
  `nib.exe --verify <tape>` walks it.

The record
  Every line he says, every line the manners refused him, every switch, the model's SHA-256 and
  the hash of the persona text are on the tape. The persona is Tenancy's shipping prompt, verbatim,
  as a third composing cue; the gate that decides WHEN a seat speaks is the pinned probe, untouched.
  That is why the status line can say "the gate is unchanged, every line says cue: dave".

Other doors in the same exe
  nib.exe --edit [FILE]      the plain editor with the resident (Ctrl+Shift+A on / off)
  nib.exe --monologue FILE   the monologue appended to a file of your choosing, tape beside it
  nib.exe --help             everything else

Made by tools/make_monologue_bundle.py from the `dave` branch of github.com/bochen2029-pixel/nib.
"""


def sha256(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def human(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return "%.1f %s" % (n, unit) if unit != "B" else "%d B" % n
        n /= 1024.0
    return "%d" % n


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", **kw)


def theme_lines(src_theme: str, model: str, llama_dir: str, font_pt: int) -> str:
    """The operator's palette carried over; the mind's keys rewritten for the bundle; ai/dave left
    off because --monologue throws the switches itself and `--edit` from this folder should still be
    a plain editor."""
    out = ["# nib.theme - written by tools/make_monologue_bundle.py for the Dave monologue bundle.",
           "# `Dave monologue.cmd` runs `nib.exe --monologue`, which forces emit on and throws the AI, Dave",
           "# and saver switches itself; the keys below are the editor's positions for `--edit`.",
           "# A model or llama_dir with no drive letter is taken relative to this folder."]
    seen = set()
    override = {"model": model, "llama_dir": llama_dir, "font_pt": str(font_pt), "ai": "off", "dave": "off"}
    if os.path.exists(src_theme):
        with open(src_theme, encoding="utf-8", errors="replace") as f:
            for raw in f:
                line = raw.rstrip("\r\n")
                s = line.strip()
                if not s or s.startswith("#"):
                    out.append(line)
                    continue
                key = s.split(None, 1)[0]
                if key in override:
                    out.append("%-10s %s" % (key, override[key]))
                    seen.add(key)
                else:
                    out.append(line)
    for key in ("model", "llama_dir", "font_pt", "ai", "dave"):
        if key not in seen:
            out.append("%-10s %s" % (key, override[key]))
    return "\n".join(out) + "\n"


def make_lnk(lnk: str, target: str, args: str, workdir: str, desc: str) -> str:
    cmd = ("$s=(New-Object -ComObject WScript.Shell).CreateShortcut('%s'); $s.TargetPath='%s'; "
           "$s.Arguments='%s'; $s.WorkingDirectory='%s'; $s.Description='%s'; $s.Save()"
           % (lnk, target, args, workdir, desc))
    for shell in ("pwsh", "powershell"):
        r = run([shell, "-NoProfile", "-NonInteractive", "-Command", cmd])
        if r.returncode == 0 and os.path.exists(lnk):
            return shell
    return ""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(ROOT, "dist", "dave-monologue"))
    ap.add_argument("--llama-dir", default="C:/llama.cpp")
    ap.add_argument("--model", default="C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf")
    ap.add_argument("--font-pt", type=int, default=16)
    ap.add_argument("--with-model", action="store_true", help="copy the model into the bundle (about 6 GB)")
    ap.add_argument("--no-dlls", action="store_true", help="reference --llama-dir instead of copying the runtime")
    ap.add_argument("--force", action="store_true", help="replace an existing bundle directory")
    a = ap.parse_args()

    if not os.path.exists(EXE):
        print("no nib.exe beside this tools/ directory - build first"); return 2
    about = run([EXE, "--about", "--no-load"])
    if about.returncode != 0 or "verbatim" not in about.stdout:
        print("REFUSED: nib.exe --about did not print the serve pin verbatim:\n" + about.stdout + about.stderr); return 2
    print("pin      " + [l for l in about.stdout.splitlines() if l.startswith("serve hash")][0])

    out = os.path.abspath(a.out)
    if os.path.exists(out):
        if not a.force:
            print("REFUSED: %s exists; pass --force to replace it" % out); return 2
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, "runs"))

    shutil.copy2(EXE, os.path.join(out, "nib.exe"))
    cache = os.path.join(ROOT, "runs", "model-hashes.txt")
    if os.path.exists(cache):
        shutil.copy2(cache, os.path.join(out, "runs", "model-hashes.txt"))

    copied = []
    if a.no_dlls:
        llama_dir = a.llama_dir.replace("\\", "/")
    else:
        for name in sorted(os.listdir(a.llama_dir)):
            if name.lower() in [r.lower() for r in DLL_REFUSED]:
                continue
            if any(fnmatch.fnmatch(name.lower(), p.lower()) for p in DLL_PATTERNS):
                shutil.copy2(os.path.join(a.llama_dir, name), os.path.join(out, name))
                copied.append(name)
        llama_dir = "."
    for bad in DLL_REFUSED:
        if os.path.exists(os.path.join(out, bad)):
            print("REFUSED: %s reached the bundle" % bad); return 2

    if a.with_model:
        base = os.path.basename(a.model)
        print("copying the model (%s) ..." % human(os.path.getsize(a.model)))
        shutil.copy2(a.model, os.path.join(out, base))
        model = base
    else:
        model = a.model.replace("\\", "/")

    with open(os.path.join(out, "nib.theme"), "w", encoding="utf-8", newline="\n") as f:
        f.write(theme_lines(os.path.join(ROOT, "nib.theme"), model, llama_dir, a.font_pt))
    with open(os.path.join(out, "Dave monologue.cmd"), "w", encoding="ascii", newline="") as f:
        f.write(CMD_LAUNCH)
    with open(os.path.join(out, "About.cmd"), "w", encoding="ascii", newline="") as f:
        f.write(CMD_ABOUT)
    with open(os.path.join(out, "README.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write(README)
    shell = make_lnk(os.path.join(out, "Dave monologue.lnk"), os.path.join(out, "nib.exe"), "--monologue", out,
                     "Dave holds the floor - the screen saver with a person in it")

    # The receipt: the bundle's own exe, with the bundle's own DLLs, brings the backends up and the
    # module gate holds. --about reads --llama-dir, not the theme, so the bundle dir is passed.
    gate = run([os.path.join(out, "nib.exe"), "--about", "--llama-dir", out])
    gate_ok = gate.returncode == 0 and "no network module" in gate.stdout
    total = sum(os.path.getsize(os.path.join(out, n)) for n in os.listdir(out) if os.path.isfile(os.path.join(out, n)))

    print("bundle   " + out)
    print("nib.exe  sha256 %s  %s" % (sha256(os.path.join(out, "nib.exe")), human(os.path.getsize(os.path.join(out, "nib.exe")))))
    print("runtime  %s (%d DLLs)%s" % ("copied" if copied else "referenced at " + llama_dir, len(copied),
                                       "" if copied else ""))
    for n in copied:
        print("         %-28s %s" % (n, human(os.path.getsize(os.path.join(out, n)))))
    print("refused  " + ", ".join(DLL_REFUSED) + " (absent: %s)" % all(not os.path.exists(os.path.join(out, b)) for b in DLL_REFUSED))
    print("model    " + model + ("" if a.with_model else "  (by reference)"))
    print("theme    llama_dir %s · font_pt %d · ai off · dave off (the launch throws the switches)" % (llama_dir, a.font_pt))
    print("launch   Dave monologue.cmd · Dave monologue.lnk (%s) · About.cmd" % (shell or "NOT made - no PowerShell"))
    print("total    " + human(total))
    print("gate     " + ("OK: " + [l for l in gate.stdout.splitlines() if l.startswith("gate")][0]
                         if gate_ok else "FAILED:\n" + gate.stdout + gate.stderr))
    return 0 if gate_ok else 3


if __name__ == "__main__":
    sys.exit(main())
