"""nib · snap — a snapshot of everything that is source, beside the repo and outside git's reach.

    python tools/snap.py <label>          copy src/ tools/ docs/ build.bat nib.theme *.md into
                                          versions/<YYYYMMDD-HHMMSS>-<label>/ with an MD5SUMS.txt
    python tools/snap.py --list           the snapshots on disk, newest last
    python tools/snap.py --diff <dir>     which files differ between a snapshot and the tree now

Git versions every green step; this is the belt to that brace. It exists because on 2026-09-05 a
whole-file write was cut mid-token and the session that wrote it did not know it had (CLAUDE.md,
"versioned edits"). A snapshot before a risky step and after every green commit means the most a
truncated write can cost is the step it was in. `versions/` is gitignored: a backup that git can
reset away is not a backup.
"""
import hashlib
import os
import shutil
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSIONS = os.path.join(ROOT, "versions")
PICK_DIRS = ("src", "tools", "docs", "tests")
PICK_FILES = ("build.bat", "nib.theme", "README.md", "HANDOFF.md", "CLAUDE.md", ".gitignore")


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sources():
    out = []
    for d in PICK_DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if x not in ("node_modules", "__pycache__", "runs")]
            for fn in filenames:
                if fn.endswith((".obj", ".exe", ".pdb", ".pyc")):
                    continue
                p = os.path.join(dirpath, fn)
                out.append(os.path.relpath(p, ROOT))
    for f in PICK_FILES:
        if os.path.exists(os.path.join(ROOT, f)):
            out.append(f)
    return sorted(out)


def snap(label):
    stamp = time.strftime("%Y%m%d-%H%M%S")
    dest = os.path.join(VERSIONS, "%s-%s" % (stamp, label))
    os.makedirs(dest, exist_ok=True)
    lines = []
    for rel in sources():
        src = os.path.join(ROOT, rel)
        dst = os.path.join(dest, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        lines.append("%s  %s" % (md5(src), rel.replace(os.sep, "/")))
    with open(os.path.join(dest, "MD5SUMS.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("%s: %d files" % (dest, len(lines)))
    return 0


def list_snaps():
    if not os.path.isdir(VERSIONS):
        print("no snapshots")
        return 0
    for d in sorted(os.listdir(VERSIONS)):
        m = os.path.join(VERSIONS, d, "MD5SUMS.txt")
        n = sum(1 for _ in open(m, encoding="utf-8")) if os.path.exists(m) else 0
        print("%s  %d files" % (d, n))
    return 0


def diff(snapdir):
    m = os.path.join(snapdir if os.path.isabs(snapdir) else os.path.join(VERSIONS, snapdir), "MD5SUMS.txt")
    if not os.path.exists(m):
        print("no MD5SUMS.txt in %s" % snapdir)
        return 2
    then = {}
    for line in open(m, encoding="utf-8"):
        line = line.rstrip("\n")
        if "  " in line:
            h, rel = line.split("  ", 1)
            then[rel] = h
    now = {rel.replace(os.sep, "/"): md5(os.path.join(ROOT, rel)) for rel in sources()}
    changed = sorted(r for r in now if r in then and then[r] != now[r])
    added = sorted(r for r in now if r not in then)
    gone = sorted(r for r in then if r not in now)
    for r in changed:
        print("changed  %s" % r)
    for r in added:
        print("added    %s" % r)
    for r in gone:
        print("gone     %s" % r)
    print("%d changed, %d added, %d gone" % (len(changed), len(added), len(gone)))
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "--list":
        return list_snaps()
    if sys.argv[1] == "--diff" and len(sys.argv) > 2:
        return diff(sys.argv[2])
    label = "".join(c if c.isalnum() or c in "-_." else "-" for c in sys.argv[1])
    return snap(label)


sys.exit(main())
