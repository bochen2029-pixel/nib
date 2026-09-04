# HANDOFF — read this first

*Written 2026-09-03 by the session that built nib Stages 0a–0d, immediately before its context was
compacted. It is three things at once: a **continuation prompt** for the same work, a **handoff**
to a different session, and a **cold-start initialisation** for a session that has never seen this
estate. Everything load-bearing is either in this file or named by absolute path.*

**If you are starting from this file: read §1 and §2 before you touch anything, then §5 for where
to resume.**

---

## 1 · The machine — rules that bit this session, repeatedly

These are not style preferences. Each one cost real time today.

**1.1 File content NEVER travels through a shell literal.** Write files with the **Write** tool;
change them with **Edit**. A Bash heredoc, or a C++ string embedded in a `python - <<'PYEOF'`
block, will turn `"\n"` inside a C++ literal into a **real newline**, and MSVC says
`error C2001: newline in constant`. This happened **five separate times today** in `changeset.cpp`,
`nib.cpp`, and `edit.cpp` — twice after I had already written the rule down. It also once produced
a literal NUL byte inside `L'\0'`. If you must patch programmatically, write the replacement text
to a `.txt` with the Write tool and have Python *read that file*, never embed the text.

**1.2 Forward slashes in every shell command.** Git Bash eats backslashes in unquoted arguments:
`python C:\x\y.py` arrives as `C:xy.py`.

**1.3 `cmd` here does not search the current directory for an exe.** Use `./nib.exe` or an
absolute path.

**1.4 `system()` and `start` strip the outer quotes** of a command that begins with one. Wrap the
whole line again: `cmd /s /c "" "C:/a b/x.exe" ... ""`. A bare `start "" "C:\p\f.bat"` from Bash
produced `Access is denied` — write a small `.bat` in the scratchpad and run that instead.

**1.5 Do not load this machine.** The operator asked, in these words, that synthetic CPU
saturation stop. It is their working box with llama-server, a speech stack and other agent
sessions live on it. **Do not run load generators.** The Stage 3 loaded-box numbers in
`C:\glance\docs\devlog.md` stand as measured; they are not to be re-taken without being asked.

**1.6 Never synthesise global input.** `keybd_event` / `SendInput` land wherever the focus happens
to be, which can type into the operator's other windows. This session started such a driver and
killed it within a minute. nib has a proper seam instead: `WM_APP+1` commands and the `NIB_LOG`
file (see §4.6).

**1.7 Do what the operator says, not what you infer they meant.** Asked to match colours *from a
screenshot*, this session went and sampled a live application window instead, which was both
wrong and slower. The correction was blunt and deserved.

**1.8 The website has a deploy law.** `C:\Websites\_DEPLOY_LANE.md`, §1, in order. The step whose
absence caused real harm: **hash-manifest `_upload/` BEFORE touching anything**, because
`wrangler` ships the whole assets directory and will publish other people's unfinished files. This
session published another session's draft backups that way and had to un-publish them.

---

## 2 · How these repositories are written

Every tool in this family follows the same discipline. Match it or the work will not fit.

- **`CLAUDE.md`** at the repo root: hard rules and refusals, written as laws with reasons.
- **`docs/BLUEPRINT.md`**: the design and the stages, each stage with a **falsifier** — the
  observation that would say it failed.
- **`docs/devlog.md`**: a lab notebook. Dates, what was tried, what was measured, what was
  decided, and **traps the day they bite**. Failures stay printed beside the fixes.
- **`--selftest` before every commit.** Numbers get measured on this box and carry their date.
  No performance figure appears anywhere it was not measured.
- **Prose voice:** plain, specific, no marketing. Say what was measured and what was not. When a
  claim is unproven, the document says so in the same sentence.
- **`build.bat`, `/W4 /WX`, zero warnings, static CRT, one exe, C++20.** No frameworks.
- **A build gate proves the refusals mechanically** — `dumpbin /dependents` fails the build if a
  network DLL appears. Hazards unreachable by construction, never forbidden by policy.
- **Commit messages are prose**, several paragraphs, explaining the decision and the traps. Always
  end with the attribution trailer the harness specifies.
- **Selftest trap:** C++ leaves argument evaluation order unspecified, so
  `check(f(&err), "..." + err)` may read `err` before `f` runs. **Compute first, format after.**
  This session wrote that bug and caught it by reading the output rather than the pass count.

---

## 3 · The estate — what exists

| tool | path | state |
|---|---|---|
| **facet** | `C:\facet` | shipped · TOOL 01 |
| **vramtop** | `C:\GPUz` | shipped · TOOL 02 |
| **caseclock** | `C:\caseclock` | shipped · TOOL 03 · `--facts -` added by this session (`360cd2f`) |
| **glance** | `C:\glance` | 0.4.0 · TOOL 04 · public repo · 142 checks |
| **everywho** | `C:\Intellect_AI_tools\everywho` | TOOL 05 · Stage 0 only (counters tier) |
| **fray** | `C:\fray` | 0.4.0 · Stages 0–3 · 64 checks · not published |
| **nib** | `C:\nib` | **0.5.0 · the active work · Stages 0a–1a · 116 + 22 checks** |

The site is `C:\Websites\aorta-site`, deployed with `npx wrangler deploy`; five tools are live at
`https://opnaorta.ai/tools`. The deploy ledger is `aorta-site/DEPLOY_LOG_<date>.md`.

**The substrate nib is built on:** FUSOR-1, at `C:\auricle`. Read
`C:\NEW\FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md` for what it is. The pieces nib needs already
exist and are built: `C:\auricle\src\fusor\fusord.cpp` (the resident loop),
`C:\auricle\src\fusor\source.h` (**the intake seam — read this before writing `PadSource`**),
`C:\auricle\src\fabric\fabric.h` (the hash-chained tape), `C:\auricle\src\overlay\hud.cpp` (words
projected on screen). `C:\auricle\build\Release\` holds working binaries.

**Etherpad's source is on disk** at `C:\etherpad-develop` (Node/TypeScript). nib ports its
changeset library rather than bundling its server; the reasoning is in `C:\nib\docs\ASSEMBLY.md`.

---

## 4 · nib — the active work

### 4.1 What it is

**A writing surface with no send key on either side.** You type; a locally-resident model perceives
as you type. It writes; you watch the words form. Neither takes a turn. Act II puts it on a LAN.

The thesis, in one paragraph: Etherpad did not fail as technology, it failed as a **matching
problem** — its value needed two people free in the same instant, which is the scarcest thing in
collaboration. A resident mind is the first thing that is always free. So Act I is not a
collaboration tool with an assistant bolted on; it is *the second party*, and the network waits.
The fit is exact because both directions already stream at the same grain: a pad's input is the
ordered edit stream a decode-on-delta loop consumes, and a pad's output already renders another
author's characters as they arrive. **And there is no system prompt** — that is an artefact of
amnesia, and a resident does not have amnesia; the interaction is the prompt.

### 4.2 Read these, in this order

1. `C:\nib\CLAUDE.md` — the rules, including where nib deliberately breaks the family's no-model
   rule and what it pays for the exception.
2. `C:\nib\docs\SPEC.md` — **normative**. Every line marked BUILT / SPECIFIED / OPEN.
3. `C:\nib\docs\ROADMAP.md` — ten stages, three acts, each with its falsifier and whether it fired.
4. `C:\nib\docs\ASSEMBLY.md` — why the changeset library is ported and why OT rather than CRDT.
   **Supersedes BLUEPRINT §2 and §7 where they disagree.**
5. `C:\nib\docs\BLUEPRINT.md` — the design and the intent.
6. `C:\nib\docs\devlog.md` — what happened, in order, with the traps.

### 4.3 Built and green — 116 checks in the exe, 22 in the driver

```
C:\nib\build.bat               /W4 /WX, gate: no network DLL among the dependents
C:\nib\nib.exe --selftest      116 passed, 0 failed
python C:/nib/tools/drive.py   22 passed, 0 failed   (the window, driven by messages)
```

- **`src/changeset.h/.cpp`** — Etherpad's Easysync format in C++: base36, `Op`,
  `deserialize_ops` (a scanner, not `std::regex`), `unpack`/`pack`, `apply_to_text`, the three
  assemblers, `check_rep`, `ops_from_text`, `make_splice`, `Builder`.
  **Canonical form is the real test:** `check_rep` re-serialises what it parsed and demands
  byte-identity, so the port must agree with Etherpad about which ops fuse, that deletes precede
  inserts, and that a trailing bare keep is implicit.
- **`src/doc.h/.cpp`** — the document is an **op log**, not a string. Every edit is a changeset.
  Undo is an inverse changeset **appended**; the log is never truncated. The inverse is computed
  against the text the edit *produces*. Undo works in **groups**, not single edits — see §5.
- **`src/edit.cpp`** — Win32/GDI window. Typing, navigation, selection, clipboard, atomic save,
  open, CRLF/BOM preservation, undo/redo, the status line, per-monitor DPI, the theme loader, the
  driver seam.
- **`src/selftest.cpp`** — the oracle. **`src/nib.cpp`** — the CLI.
- **`src/ingest.h/.cpp`** — the compiler and `PadSource`. Edits become percepts; percepts become
  `Delta`s on auricle's ring. No model, no GPU, no threads of its own.
- **`tools/drive.py`** — the window battery. Posts messages, reads artefacts, never synthesises
  input and never looks at the screen.
- **`tools/theme_detect.py`** — derives `nib.theme` from an image. **`nib.theme`** — the palette
  (colours are data; the operator's is dark navy `#0d1520` / azure `#2196f3` / dim `#3f5f7a`).

### 4.4 The falsifiers that did not fire

| stage | property | evidence |
|---|---|---|
| 0a | the port agrees with Etherpad | 41 checks on Etherpad's own vectors + 4 negative canonical-form cases |
| 0b | a splice is canonical and applies correctly | **10,000 random splices**, deterministic generator |
| 0c | the log replays byte-exact | **1,000 random edits interleaved with undos** — 767 edits, 214 undos, 965 revisions, checked after *every* one |
| 0d | no save loses a file or silently changes its conventions | the Stage 0e driver: bytes compared on disk, CRLF stayed CRLF, a BOM came back, the file survived close-and-reopen |
| 0e | every save path is reachable without global input | 17 checks over eight cases, three consecutive identical runs |
| 1a | no percept is dropped without a loud count | byte conservation over **4,000 random typings, removals and idles**, and fired again from inside the running window |

`Ctrl+R` in the editor runs the Stage 0 falsifier live and prints the answer on the status line.

### 4.5 Commands

```
nib --selftest                       the oracle
nib --edit [FILE]                    the window
nib --splice "text" START NDEL "ins" the changeset for one edit, and the result
nib --unpack CS | --ops CS | --check CS | --apply CS TEXT
python tools/theme_detect.py shot.png
python tools/drive.py [--exe X] [--keep]   the window battery; exit 3 on any mismatch
nib --ingest FILE [--chars N --quiet-ms T --tick-s S --counts]   the percept stream, no GPU
```

Editor keys: `Ctrl+S` save · `Ctrl+Shift+S` save as · `Ctrl+O` open · `Ctrl+A/C/X/V` ·
`Ctrl+Z` undo · `Ctrl+Shift+Z` redo · `Ctrl+R` fold the log and check it.

### 4.6 The driver seam

The window is tested by **posting messages and reading an artefact**, never by synthesising input.
`WM_APP+1` (`WM_NIB_CMD`) with `wParam` one of `CmdSave=1, CmdSaveAs, CmdOpen, CmdUndo, CmdRedo,
CmdSelectAll, CmdReplay, CmdHome, CmdEnd, CmdSelToHome, CmdTop`. Setting the environment variable
`NIB_LOG` to a path makes the window append a tab-separated line per command result. Plain
`WM_CHAR` posts work for typing; **Ctrl chords posted with `PostMessage` do not**, because a posted
message does not update the thread's key state and `GetKeyState(VK_CONTROL)` reads false. That is
why the command channel exists.

---

## 5 · Resume here

**Stage 0e is done (2026-09-04, 0.4.1).** `C:\nib\tools\drive.py` — 17 checks over eight cases,
posting `WM_CHAR` and `WM_NIB_CMD` and asserting on the bytes on disk and the lines in `NIB_LOG`.
Its falsifier did not fire: every save path was reachable without global input, and no byte was
lost across close-and-reopen. That also closed the one honest gap in the ROADMAP — Stage 0d is no
longer marked done ahead of its evidence.

Run both oracles before any commit:

```
C:\nib\nib.exe --selftest        77 passed, 0 failed
python C:/nib/tools/drive.py     17 passed, 0 failed
```

Three things the driver found, kept here because they are the kind of thing that comes back:

- **Undo groups.** A burst by the same hand, contiguous, within `Doc::kGroupMs` (700 ms) and of the
  same kind is one undo. A pause, a newline, a jump elsewhere, a switch between typing and
  deleting, or a **different author** closes it. That last clause is load-bearing for Stage 1: the
  resident writes into the same buffer and must never fuse into a person's undo.
- **The two undo stacks carry opposite conventions.** An undo group is stored in edit order and
  applied newest-first; a redo group is stored in apply order and applied forwards. Reversing
  either one half-restores a burst and looks like corruption.
- **When a test is intermittent, suspect the test.** Two runs in three failed because
  `FindWindow` by class name returns *any* nib window — a leftover, or the operator's own. The
  driver now binds to the pid it launched. This looked exactly like an editor bug and was not.

**Stage 1a is done (2026-09-04, 0.5.0)** — `src/ingest.h/.cpp`. `PadSource` implements
`auricle::fusor::StreamingTextSource`; the compiler turns edits into percepts; the editor feeds it
from `edit_splice`; `nib --ingest FILE` shows the percept stream with no GPU. 116 selftest checks
and 22 driver checks, three identical runs.

**Four things it found that the header and the spec had wrong** — do not re-derive these:

- `sizeof(Delta)` is **528**, not the 512 `source.h`'s own comment claims.
- `fill_delta` **silently truncates at 495**: it reserves the last byte for a NUL. SPEC 5.1.7 used
  to say chunk at 496, which loses a byte per full chunk with no signal. Use `nib::kChunkMax`.
- a `PadSource` is **~528 KB and must never be a stack local** — it crashes at construction with
  no output at all (`0xC00000FD`), which looks like a broken build rather than a stack overflow.
- a percept is a **clause, not a word**. fusord prepends `\n[lane] ` per Delta and judges when the
  line ends, so one percept per word would probe three seats per word. A word boundary is where a
  percept may *end*. `.`/`!`/`?` close a thought; `;`/`:` do not (fusord measured that).

**Now: Stage 1b — a resident that only holds.** The trunk, the segmenter, hold/emit computed and
recorded with **emission disabled**; the margins move against your own typing before the thing ever
writes a word. Read `C:\auricle\src\fusor\fusord.cpp` and **lift the loop rather than re-deriving
it** (SPEC 6.2.1), with the seed hashed and the build refusing to run if a character drifted
(6.2.2), and its own `--idle-tick-s` set to 0 because the pad already supplies ticks (SPEC 5.1.9.1).

**This is the first stage that needs the GPU** — `C:/models/Qwen3.5-9B-emit-v11-Q5_K_M.gguf`,
6.6 GB, on a card the operator shares with llama-server and a speech stack. Per §1.5 it does not
get loaded without being asked. Everything through 1a runs with the GPU untouched, and per
`CLAUDE.md` every stage below Stage 2 must stay runnable that way.

Two constraints from `source.h` that still hold, and are already honoured in `PadSource`:

- **self-echo** (spec §5.8): a delta whose lane is one of the resident's own seats must never be
  fed back, or the nucleus deliberates about interrupting itself. In a pad the resident writes into
  the buffer it reads, so this filter lives in `PadSource`, at the source, and is case-insensitive.
- the lane string is **train ≡ serve**; the trunk sees `[lane] text` byte-identically to the tune
  format, so lane naming is not a UI decision.

---

## 6 · Open loops and hazards, right now

**6.1 Resolved 2026-09-04.** `nib.exe` was locked by an instance the operator had open from a build
predating save; the session built to `nib-next.exe` rather than kill the window. That window has
since been closed by the operator, `nib-next.exe` is gone, and `nib.exe` is the current build. The
standing rule survives the incident: **if `nib.exe` is locked, build beside it — never kill a
window that may hold unsaved text.**

**6.2 `compose` and `follow` are not ported, deliberately.** Act I needs neither. Before either is
trusted, build the **differential harness**: `pnpm install` the Etherpad tree and run the real
`Changeset.ts` over random inputs, comparing byte for byte. Hand-picked vectors are not sufficient
for those two — their failure modes are rare and structural.

**6.3 Public and still true:** everywho's page says it cannot name files yet (the ETW tier is
unbuilt). That is an honest promise with an implied follow-through.

**6.4 glance's open items** are in `C:\glance\docs\BACKLOG.md`: change-to-line on real remote
content is unmeasured; `--mcp` and the strip have no automated tests.

**6.5 The estate's biggest unmeasured number** is the twin race — a resident against a
maximally-good turn-based twin on identical weights. **nib's Stage 4 toggle produces it as a
by-product of ordinary work**, one paired sample per flip, provided the seat, seed and sampler stay
identical across the toggle and only the trigger differs.

---

## 7 · The line that governs everything

**If the operator, alone with nib for a week of real work, does not leave it turned on, the idea is
wrong.** Nothing in Act II rescues that, and the finding gets published beside the laws it bought.

Everything before Stage 5 exists to make that week possible and to make its answer trustworthy.
