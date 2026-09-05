# nib — ROADMAP

*Rev 0.7 · 2026-09-05. Each stage carries a **falsifier**: the observation that would say the stage
failed. A stage is not done because its code exists; it is done when its falsifier has been fired
at and did not go off. Dates are the day a stage went green on this box — by git: every stage
below Stage 1c went green on 2026-09-04, though revs 0.1–0.4 of this file said 09-03 for the first
four; Stage 1c went green in the small hours of 2026-09-05.*

**Legend** — ✔ done · ◑ in progress · ○ not started · ⨯ deliberately not doing

---

## Where it stands

| act | stage | state | version |
|---|---|---|---|
| **I** | 0a · the changeset port, read half | ✔ 2026-09-04 | 0.1.0 |
| **I** | 0b · the write half | ✔ 2026-09-04 | 0.2.0 |
| **I** | 0c · the editor shell | ✔ 2026-09-04 | 0.3.0 |
| **I** | 0d · save, selection, the theme | ✔ 2026-09-04 | 0.4.0 |
| **I** | 0e · the window driver | ✔ 2026-09-04 | 0.4.1 |
| **I** | 1a · ingest — the compiler and PadSource | ✔ 2026-09-04 | 0.5.0 |
| **I** | 1b · a resident that only holds | ✔ 2026-09-04 | 0.6.0 |
| **I** | the QC pass — two criticals, the runtime gate, six Stage 2 breakers | ✔ 2026-09-04 | 0.6.1 |
| **I** | 1c · the wire — the resident in the window, the gutter, the AI switch, the tape | ✔ 2026-09-05 | 0.7.0 |
| **I** | 1d · the trunk as an asset — the checkpoint beside the document, word wrap | ✔ 2026-09-05 | 0.8.0 |
| **I** | 2 · emission, with floor control | ✔ 2026-09-05 | 0.9.0 |
| **I** | 3 · un-saying, made visible | ✔ 2026-09-05 | 0.10.0 |
| **I** | the read of 0.10.0 — the span memory, own speech through the document, the manners across the switch | ✔ 2026-09-05 | 0.10.1–0.10.3 |
| **I** | 4 · the two switches, and the paired record | ○ | |
| **I** | 5 · **a week of real work** — the falsifier for the whole idea | ○ | |
| **II** | 6 · discovery | ○ | |
| **II** | 7 · rooms | ○ | |
| **II** | 8 · convergence | ○ | |
| **III** | 9 · three seats | ○ | |

**236 checks green in the exe, 30 more from the window driver, and 68 with the resident switched on
inside the window** (2026-09-05, three identical driver runs). The exe links kernel32, user32,
gdi32, comdlg32 and bcrypt (the model's SHA-256) — no network DLL, enforced at build — plus
llama.cpp and ggml, all three **delay-loaded**, which the build asserts rather than assumes. Nothing touches a llama symbol until `--resident` asks for one, so `--selftest`,
`--ingest` and the editor still run on a machine with no model and no card. That is CLAUDE.md's
rule that a battery needing a 9B is a battery that stops being run. `nib --about` loads the
backends by name and prints the runtime module gate's verdict — 57 modules, no network DLL —
without loading a model.

**Operator ruling, 2026-09-04: the shipped product autodiscovers on the LAN by default**, toggled
off, never absent. The build order below is unchanged — the resident half carries the thesis and
the editor must be sound before either the network or emission stands on it — but Act II is no
longer "after Stage 5": Stages 6 and 7 land before the week of real work, so that the week runs on
the build that ships.

---

## Act I — one person, one resident, no network

### ✔ Stage 0a — the changeset port, read half · 0.1.0

base36, `Op`, `deserialize_ops`, `unpack`/`pack`, `apply_to_text`, the three assemblers,
`check_rep`. Ported from `Changeset.ts` and companions rather than designed.

*Falsifier: a changeset Etherpad accepts that nib rejects, or a re-serialisation that differs by a
byte.* — Did not fire. 41 checks against Etherpad's own vectors, including four negative cases
that pin canonical form (a written-out trailing keep, unfused adjacent keeps, a wrong claimed
length, excess bank characters).

### ✔ Stage 0b — the write half · 0.2.0

`ops_from_text`, `make_splice`, `Builder`.

*Falsifier: a splice whose changeset is not canonical, or which does not apply to the string
ordinary surgery produces.* — Did not fire over **ten thousand random splices**, from a
deterministic generator. Exact bytes are pinned too, so a drift in the encoding is caught rather
than tolerated.

### ✔ Stage 0c — the editor shell · 0.3.0

`doc.cpp` (the document as an op log) and `edit.cpp` (Win32/GDI). Typing, navigation, undo, redo,
the status line, `Ctrl+R`.

*Falsifier: replaying the op log does not reproduce the document byte-exact.* — Did not fire over
**a thousand random edits interleaved with undos** (767 edits, 214 undos, 965 revisions), checked
after every single one. The runtime check is `Ctrl+R`, so the person using it can fire the
falsifier themselves at any moment.

### ✔ Stage 0d — save, selection, the theme · 0.4.0

Atomic save, open, CRLF and BOM preservation, the unsaved-changes prompt. Selection by keyboard and
mouse, clipboard. `nib.theme` read at startup, `tools/theme_detect.py` to derive it from an image.
The `WM_APP+1` / `NIB_LOG` driver seam.

*Falsifier: a save that can lose the previous file, or a file whose conventions nib silently
changed.* — **Fired at on 2026-09-04 and did not go off.** Stage 0e's driver reaches every save
path through the command seam and compares the bytes on disk; a CRLF file came back CRLF, a BOM
came back, and the file survived close-and-reopen byte for byte. Until that day this stage was
marked done ahead of its evidence, and the ROADMAP said so rather than hiding it.

### ✔ Stage 0e — the window driver · 0.4.1

`tools/drive.py`: launches the window with `NIB_LOG` set, posts `WM_CHAR` to type and `WM_APP+1`
for the chords, then reads the log and the bytes on disk. **17 checks** across eight cases — type
and save, a selection typed over, undo and redo by group, backspace and delete, the live replay,
close and reopen, CRLF and BOM, and a path that did not exist.

**It must not synthesise global input.** The first attempt did, and global keystrokes land wherever
the focus happens to be — which can type into another application's window. That is why the seam
exists.

*Falsifier: a save path that the driver cannot reach without global input, or a save that loses a
byte across a close-and-reopen cycle.* — Did not fire. Every path was reachable by posting
messages; no byte was lost.

**The driver earned its keep on its first run**, which is the argument for writing one at all. It
found two defects that 71 unit tests had not:

- **Undo unpicked typing one character at a time.** Technically correct and unusable. `Doc` now
  groups: a burst by the same hand, contiguous, within 700 ms and of the same kind is one thing a
  person did. A pause, a newline, a jump elsewhere, a switch between typing and deleting, or a
  different author closes the group.
- **Redo replayed a group backwards.** The two stacks carry opposite conventions — an undo group is
  stored in edit order and applied newest-first, a redo group is stored in apply order and applied
  forwards — and mixing them up half-restored a burst.

It also caught **its own** flakiness, which is worth recording because the failure looked like the
editor's: `FindWindow` by class name alone returns *any* nib window, including one left over from
an earlier case or the operator's own. Two runs in three failed for that reason. The driver now
enumerates windows and binds to the pid it launched, and runs three times for three identical
results.

### ✔ Stage 1a — ingest: the compiler and PadSource · 0.5.0

`src/ingest.h/.cpp`. `PadSource` implements `auricle::fusor::StreamingTextSource`; nib includes
that header from `C:\auricle\src` **unmodified** (SPEC 5.1.1) rather than copying it, so the two
cannot drift, and `build.bat` fails if it is missing. The compiler turns the document's edits into
percepts: a closed thought, N characters or T ms of quiet, always ending on a word boundary,
chunked so a `Delta` never truncates, with deletions delivered intact and silence entering as
`[tick +Ns]`. Self-echo is filtered at the door. The editor feeds it from `edit_splice`, the one
funnel every edit already passes through, and the status line carries the percept count.

*Falsifier, first half: a percept dropped without a loud count.* — **Did not fire.** Stated as
arithmetic (SPEC 5.1.11) so it is checkable rather than merely asserted: bytes in equal bytes out,
and `pushed + dropped == percepts`. Fired at over 4,000 random typings, removals and idles with a
deterministic generator, and again from **inside the running window** by `drive.py`'s `ingest`
command. A deliberately overflowed ring counted 1,976 drops rather than swallowing one.

**Four traps, all found by building it rather than by reading the header.** Each is a place where
the obvious code loses data with no signal, and they are written up in SPEC 5.1.7 and the devlog:

- `sizeof(Delta)` is **528**, not the 512 its own comment claims.
- `fill_delta` **silently truncates at 495**, not 496 — it reserves the last byte for a NUL. The
  spec said to chunk at 496, which would have lost one byte per full chunk, quietly.
- a `PadSource` is ~528 KB and **cannot be a stack local**; declaring one crashed the selftest
  instantly with no output at all.
- and one of my own tests asserted an appearance rather than a property — it checked whether a
  chunk's last byte was a UTF-8 continuation byte, which the last byte of a correct em dash is.

### ✔ Stage 1b — a resident that only holds · 0.6.0

`src/resident.h/.cpp`. The trunk ingests what the pad compiled; the segmenter reads the frontier;
at a thought boundary each of the three seats is probed and its margin — `logit(emit) −
logit(hold)` — is computed and recorded. `nib --resident FILE` runs it.

**Emission is absent, not disabled.** There is no generation code in the file to switch off: no
speak-cue is decoded, no sampler is constructed, no token is produced. fusord's next block — the
one that composes a line for every seat whose margin cleared zero — is not copied, not commented
out and not behind a flag. A margin above zero here means the seat wanted to speak and the program
has no way to let it. That is the estate's preference for hazards unreachable by construction over
hazards forbidden by a boolean, and it is why 1b is its own stage rather than a flag inside 2.

**The lift is proved mechanically.** `serve_hash()` covers the seed, the six worked examples, the
stream opener, all three seats' names and mandates, the probe frame and the speak-cue frame, and it
computes `0xe7ffa5704ba31076` — fusord's own pin from 2026-08-12. The resident refuses to start if
it moves. The check is pure string arithmetic, so it runs in every `--selftest` on any machine,
with no model and no card.

*Falsifier: margins that do not move with content.* — **Did not fire.** They move, and they move
**per seat, by mandate**, on prose deliberately chosen not to be in the worked examples:

| the stream | SPEAKER | SKEPTIC | SENTINEL |
|---|---|---|---|
| "The coffee machine in the kitchen was refilled this morning." | −7.10 | −6.80 | −7.20 |
| "The build finished green about a minute ago…" | −6.62 | −6.42 | −5.96 |
| "Actually, the Pacific is the smallest ocean on Earth." | −6.19 | **+5.56** | −6.09 |
| "I am going to drop the users table to free up some disk space." | −1.04 | +4.29 | **+5.88** |
| "We agreed last week that the retry limit was three, so I set it to twelve." | −3.54 | **+4.50** | +5.63 |

A false claim moves the seat that catches false claims by about twelve logits and leaves the other
two where they were. 21 of 54 probes wanted to speak. None could.

**Measured on this box, 2026-09-04**, `Qwen3.5-9B-emit-v11-Q5_K_M` at `n_ctx` 8192 with q8_0 KV:
model load 11.6 s · 34/34 layers offloaded · CUDA0 model buffer 5657 MiB + compute 501 MiB ·
**222–260 ms per boundary for all three seats**, i.e. ~74–87 ms per probe against fusord's ~60 ms.
Total VRAM was not attributable and is not claimed: the card is shared with llama-server and a
speech stack, and the baseline moved during the run.

**The trap that cost the most, and it was silent.** The first run took **556 s** instead of 11.8 s
— 47× slower — because `ggml-cuda.dll` could not resolve its own CUDA dependencies and ggml
**fell back to the CPU without an error**, offloading nothing. The cause was one missing
`SetDllDirectory` on `C:/llama.cpp`. Two consequences worth keeping: it saturated a CPU the
operator had explicitly asked not to be loaded, and it silently corrupted the numbers — words
arrived so slowly that the 1500 ms flush law fired constantly, inventing 30 boundaries where the
correct run finds 18. The resident now enumerates the ggml devices, prints them, and **refuses to
start** if GPU layers were asked for and no GPU backend came up, unless `--allow-cpu` says so.

Why emission is disabled here: it is the cheapest possible way to find out whether the resident
perceives sanely, and it cannot embarrass itself while you are finding out.

### ✔ The QC pass · 0.6.1

The review of 2026-09-04 (`docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md`) read every line, re-ran
the oracles from a scratch copy, and found that **everything that was checked was correct, and
everything that was not checked was where the bugs were** — in each case one level below where
the falsifier looked. Fixed, each with a check that would have caught it:

- **Two criticals in the editor.** A Down-arrow then a Backspace over an accented character
  destroyed it (columns were bytes, the painter counted UTF-16 units) and `Ctrl+R` still read
  byte-exact, because the log recorded the cut faithfully. Any emoji typed became two U+FFFD (two
  surrogate `WM_CHAR`s converted alone). Columns are characters now, `Doc::splice` snaps to
  sequence boundaries, surrogates pair, and a thousand random edits at random byte offsets over a
  multi-byte alphabet leave valid UTF-8 after every one.
- **The Act I network law held for the exe and not the process.** The backend loader pulled
  `ggml-rpc.dll` — which imports ws2_32 — from `C:/llama.cpp` at `--resident` time, past the build
  gate. Backends are loaded by name now, the resident refuses to start if a network module is in
  the process, and `nib --about` prints the receipt.
- **Six things Stage 2 would have walked into:** undo, redo and open bypassed ingest (an undo
  emptied the document while the conservation identity read green); the self-echo filter guarded
  a lane no seat uses; ticks fired a full probe round, which the estate forbids; the resident
  dropped words silently at the context wall and then judged clauses the trunk never saw;
  `Doc::apply` left the undo stacks pointing at text it had changed; a long deletion's tail chunks
  reached the trunk as newly typed text.
- **And the window was DPI-unaware**, so SPEC 4.1.2 was false as built on a 225 % box.

*Falsifier: the oracles green with every new check, and `--about` refusing when `ggml-rpc.dll` is
loaded.* — 144 in the exe, 27 from the driver, three identical runs; the gate passes with real
backends loaded. **One incident, kept:** the driver's windows took the foreground and ate part of
a sentence the operator was typing to another program; a driven window is created no-activate
now (CLAUDE.md rule 12).

### ✔ Stage 1c — the wire · 0.7.0

`src/wire.h/.cpp`, `src/tape.h/.cpp`, `src/util.h`, and `edit.cpp` rewritten around them. The
resident on its own thread inside the window; percepts out and judgments back over two lock-free
rings, with the revision and byte span of what was judged; the gutter mark whose brightness is the
strongest margin at the last boundary in the line; the AI switch (`Ctrl+Shift+A`), where on folds
the document's history through the compiler and loads the model while the window keeps painting,
and off joins the thread, unloads the model and returns the card; the tape in the family's format
beside the document, verified by `nib --verify` and `glance --verify`; the model named by SHA-256 on
the session row; a latency instrument in the paint path. The first moment the operator writes with
something present, before a single word is emitted.

*Falsifier: keystroke-to-repaint moves measurably with the resident on; a judgment's clause is not
byte-identical to the compiled percept; AI-off leaves VRAM held.* — **Fired at on 2026-09-05. The
second and third did not go off; the first needed a number, and has one now.**

| measured, 2026-09-04/05 | resident off | resident on |
|---|---|---|
| keystroke → painted, p50, two runs | 607 · 605 µs (129 keys) | 1189 · 455 µs (164 · 166 keys, typed while the mind was judging) |
| keystroke → painted, p95, two runs | 1115 · 1094 µs | 1709 · 1037 µs (2912 µs in an earlier run over 54 keys) |
| VRAM used: before / loaded / after off, two runs | 5716 · 6291 MiB | 12727 / 5959 · 13334 / 6548 MiB |
| model load on the resident's thread | | 5.7–8.1 s |
| the model's SHA-256, 6.64 GB | | 17.1 s, 388 MB/s |

The instrument cannot separate on from off: the resident-on p50 landed 0.6 ms above the off figure
in one run and 0.15 ms below it in the next, so the difference is inside the run-to-run spread, and
every figure is under a fifth of a 60 Hz frame. "Measurably" was the wrong word for a falsifier;
the threshold is stated now as p95 under 8 ms with the resident on, and the worst p95 seen is
2.9 ms. The judgment's clause is the compiled percept
byte for byte — the tape's `judgment` rows carry the clause beside the `percept` rows they span, and
the driver reads the SKEPTIC's catch off the log with the clause in it. And the card comes back.

**Two silent defects in the judged record, found by the wire and fixed:**

- **Every sentence cost two boundaries.** The lifted loop appended a word to the clause after the
  probe that word triggered, so a `b` fired by "Earth." was labelled "ocean on", and the lone
  "Earth." left behind was re-judged by the line's `f` at the same trunk position — bit-identical
  margins, three probes for nothing. The word joins the clause first now. The margins script fell
  from 19 boundaries and 57 probes to 11 and 33 with every catch intact. K5 (below) made the same
  change on 09-04 for the same measured reason; found independently.
- **A double newline before every probe.** The compiler keeps a percept's newline because it
  conserves bytes, and the resident decoded it; fusord's source strips it before a Delta exists.
  Stripped at the serve boundary now. Stage 1b's table was measured on the wrong format; re-measured
  on the right one, every catch moved by under a logit:

| the stream, corrected format | SPEAKER | SKEPTIC | SENTINEL |
|---|---|---|---|
| "Actually, the Pacific is the smallest ocean on Earth." | −6.20 | **+5.93** | −5.46 |
| "I am going to drop the users table to free up some disk space." | +0.32 | +5.11 | **+5.43** |
| "We agreed last week that the retry limit was three, so I set it to twelve." | −3.39 | **+5.09** | +5.82 |
| "The coffee machine in the kitchen was refilled this morning." | −6.90 | −6.57 | −7.06 |

**The measurements the review asked for (§6.2), on the GPU, 2026-09-05.** Identical prefixes give
identical logits, so in an A/B the difference is the manipulation and nothing else.

- **Deletions move the margins, a little, and the marker costs a boundary.** A scripted stream with
  a removed line against the same stream without it: the following margins move by under a logit
  in no consistent direction (the correction line SKEPTIC +2.66/+2.81 against +1.91/+3.09; the lunch
  line +3.45 against +2.85); the removed claim re-fires the SKEPTIC at +4.87 against +5.83 for the
  claim itself; and `(removed)` closes a thought of its own at boundary mass 0.95, three probes on
  one word. The marker does not neutralise a claim; its wording stays open (SPEC 14.9).
- **Ticks are perceived and cost no probes.** A 3600 s tick before a line against no tick: the three
  boundaries after it shift by at most 0.42 logits, mostly toward speaking, nothing crosses zero,
  21 probes in both arms.
- **KV at 16k is cheap; the fold is not.** q8_0 KV: 136 MiB at 8192, 272 MiB at 16384, so 16k is the
  default. Switching on over the README (4.2 KB, 749 words) costs 39 s: 94 percepts, 104 boundaries,
  312 probes, 12.7 s of probing, about 35 ms a word of decode. At the 50 KB fold budget that is
  minutes, and it is Stage 1d's problem.
- **The probe's cost is the card's.** 118–123 ms per boundary for three seats on a quiet card;
  312–327 ms on the same binary an hour earlier with llama-server busy. A factor of 2.7 from
  co-tenancy alone; the number prints beside the card's state from Stage 1d on.
- **The T sweep was not run.** It needs a real typing tape; the synthetic cadence never pauses.
- **Modules after the backends load: 57, none of them network** (`nib --about`).

**Read during the stage:** `C:\fusor1\converge\src\fusord.cpp`, K5, the convergence of the two
fusord lineages that came after the 08-12 kernel this project lifted from — written on the evening
of 09-04 while nib's review ran, in flux in another session, same pin. By operator ruling a source of
ideas and not of bytes. Taken for the plan: a judgment is always about now, because the model's
recurrent state cannot be rewound (SPEC 6.2.8); the trunk is an asset and a resident rebuilt from
its log is the twin (SPEC 6.2.11, Stage 1d); the seam Stage 2 lifts (SPEC 6.3.4); free VRAM beside
every probe; torn-row recovery. What nib has that K5 lacks is in `docs/BACKLOG.md`.

**One trap in the driver, kept:** the AI case never saved, its close hit the unsaved-changes prompt,
the process was killed after eight seconds and the tape ended without `session_close`. It saves
first now. A tape without `session_close` means what it says.

### ✔ Stage 1d — the trunk as an asset · 0.8.0

The checkpoint beside the document: the trunk's KV and token list saved atomically at switch-off
and at quiet, with a sidecar carrying the model's SHA-256, the serve hash, the token count, the
document revision, the tape head and the last percept's wall time; restored at switch-on when
every field agrees, else the fold and the label *twin*; the resume tick for the wall gap; the
model's hash cached on size and mtime; free VRAM on every judgment row; torn-row recovery in the
tape; word wrap with a toggle, by the operator's word (`docs/BACKLOG.md`). The switch becomes cheap
to flip, which Stage 4 needs.

*Falsifier: a restored resident's first margins differ from a no-restart control by more than the
run's margin noise (K5's T8); a checkpoint that loads against a swapped model or a moved document;
a crash inside a checkpoint that leaves no loadable generation; a torn tape that silences a
session; a line that leaves the window without a scroll or a wrap.* — **Did not fire.** The driver
switches off (the trunk is saved: 58.8 MB, 350 tokens, 68 ms), switches on (restored, the model's
hash remembered, 0 ms), and the restored SKEPTIC catches a *new* false claim at +4.84. A sidecar
with one byte appended is refused and the resident that follows is the twin, folded from the log,
with the reason on the tape and the status line. Word wrap conserves every byte, and the caret
round-trips through row and column at every offset of a wrapped line.

**The defect the stage was built to find, and it was not word wrap.** Switching the resident off
and on again crashed the process — always, from Stage 1c onward, and nobody had switched it twice
before. The abort said only `ggml-cuda.cu:103: CUDA error`. Bisected: the second model loaded into
one process dies in a cuBLAS matmul with *invalid argument* the moment it decodes a batch above
about 64 tokens; a life that *restores* never crashed, because the only decode it makes that large
is the seed it does not do. **The batch is capped at 64 now** (SPEC 6.2.12), and the second reason
is stronger than the first: with the cliff left in, the first life would judge through cuBLAS and
the second through the quantized path, and the same sentence would score differently in the same
session — measured at mean 0.16 and max 0.84 logits apart, with one near-zero seat crossing zero.
Under the cap two seeded lives in one process return **bit-identical margins** (−5.55 on the same
sentence, twice). Three things made the hunt tractable and are kept: llama's and ggml's errors
reach stderr whatever the verbosity, the window logs a crash before it dies, and `NIB_TRACE` prints
the resident's start-up steps.

**Re-measured under the cap, 2026-09-05** — every margin above this line was taken before it and is
off by the drift named above:

| the stream | SPEAKER | SKEPTIC | SENTINEL |
|---|---|---|---|
| "Actually, the Pacific is the smallest ocean on Earth." | −6.19 | **+5.73** | −5.60 |
| "I am going to drop the users table to free up some disk space." | −0.52 | +4.91 | **+5.64** |
| "The parser handles the empty case first and then the general one." | −2.00 | +5.78 | **+5.82** |
| "We agreed last week that the retry limit was three, so I set it to twelve." | −2.91 | +4.77 | **+5.93** |

12 of 33 probes wanted to speak; probe 107–122 ms per boundary on a quiet card. Both A/B findings
hold: the deletion moves the following margins by under 0.4 logits in no consistent direction, and
the removed claim re-fires the SKEPTIC at +5.14 against +5.73 for the claim itself; the tick shifts
the three boundaries after it by at most 0.20 logits and fires no probe.

**One idea taken from K5 and one refused.** Taken: the trunk as an asset, with the atomic write,
the sidecar written last, the previous generation kept, the quiet gate and the resume tick.
Refused: K5 queues the hand's keystrokes while the model loads and compiles them after the history.
nib does not need to — every one of them is already in the document's log with its own clock, and
the fold replays the log. Queueing them would perceive them twice.

### ✔ Stage 2 — emission, with floor control · 0.9.0

**It speaks.** Every seat whose margin clears zero composes one sentence on a fork of the trunk,
and writes it as its own block, prefixed with its name, after the line holding the clause it is
about — never joined onto the end of a human's line, so the file on disk is a valid lane stream and
it is never ambiguous who wrote what. The lift map was refreshed against K5 first
(`docs/review/LIFT_MAP_K5_2026-09-05.md`), as SPEC 6.3.4 required.

*Falsifier: one emission lands inside a block a human touched inside the floor window; or forming
text survives a save or a crash.* — **Did not fire.** The driver types a sentence and asserts that
**nothing at all is written while the hand is still moving**, then pauses and asserts that a seat
writes its line, that the line is in the document in the seat's own block, and that no resident
block is joined onto a human's. Forming text cannot survive a save because there is none: Stage 2
composes and then writes, and the forming plane arrives in Stage 3 with the mechanism that takes a
sentence back (SPEC 6.4).

**The clause needed a correction to be satisfiable at all,** and it is the finding of the stage. A
judgment fires *while the hand is typing* — a percept arrives, a clause closes, the seats are
probed — so an emission refused at that instant for being inside the floor window is refused at
every instant there ever is, and the resident is mute by arithmetic rather than by judgment. So a
margin above zero records a **want** and composes nothing; the wants are composed when the hand has
been still for the floor window. **Pausing is how a person yields the floor**, and the refusal costs
nothing because it never runs the model. The floor is checked again at the moment of writing,
because between composing and arriving there is half a second in which the hand may start again —
and that second gate fires in an ordinary run: two `refused {why: floor}` rows in the driver's tape.

**The first things it wrote, measured 2026-09-05.** In the CLI, on `tests/margins.txt`:

| seat | margin | the line |
|---|---|---|
| SKEPTIC | +5.32 | "The Pacific is actually the largest ocean, not the smallest — that contradicts what we know." |
| SPEAKER | +4.82 | "Friday works — ship it once the final check passes." |
| SENTINEL | +5.06 | "Dropping the users table is irreversible without a tested backup; do not proceed." |

Three seats, three mandates, three correct catches on prose none was tuned on; 9 of 36 probes
wanted to speak, 3 said something, 6 were held by the manners; 11–19 tokens a line and 369–581 ms
of generation. And in the window, the document after a driver run:

```
… Actually, the Pacific is the smallest ocean on Earth. … And the users table can go …
[SKEPTIC] The Pacific is actually the largest ocean on Earth, not the smallest.
[SENTINEL] Deleting the users table is irreversible without a backup; don't.
```

**Two findings kept.** The SPEAKER answered "Watcher, should we ship this on Friday" *before* the
sentence finished, because the boundary fired at b=0.67 on "Friday" — half a question is enough to
answer, which is the segmenter's measured law working, and whether it should be is a Stage 5
question. And a line is judged at a timeout boundary after a seat speaks, because generation takes
half a second and the flush law is wall-clock: **speaking slows perceiving, and the record says so.**

**One gap, found by the run and not yet closed:** the manners' memory dies with the resident while
the trunk survives. A restored resident has its own lines on its trunk (the self-echo commit) but
an empty suppression ladder, so a seat can repeat itself once after a switch. Visible in the
driver's own tape, where the SKEPTIC said the Pacific line in two lives in two phrasings.
`docs/BACKLOG.md` carries it; the fix is three strings in the checkpoint's sidecar.

**The stage contained the genuinely unsolved problem, and here is the answer it gives.** The emit
gate decides *whether* to speak; nothing decided *where the words land and when*, in a buffer
somebody else is typing into. Two humans negotiate that continuously and unconsciously, and nobody
has had to solve it for an entity with no turns sharing a surface with humans who also have no
turns. Act I's answer is deliberately crude and enforced by the serialiser rather than by manners:
**where** is a block of the seat's own, after the line it is about, never inside a human's
paragraph; **when** is the pause. It is not negotiation — the human never announces and the
resident never asks — and the review's §5.1 says what a negotiated version would look like (the
gutter as an announcement, the human disposing by typing, pausing, editing or deleting). Stage 5
is where a week of real work says whether the crude rule is enough.

### ✔ Stage 3 — un-saying, made visible · 0.10.0

**The demonstration the whole project is for, and it works.** A seat begins a sentence; the words
appear as they are sampled, in a plane of the view the document never sees; the human's next
sentence lands on the trunk between two generated tokens; the seat is asked again on a fresh fork
of the updated trunk; and the line dies mid-word. Measured on this box, 2026-09-05:

```
abort   SKEPTIC   +5.06 → −1.77   margin_flipped
aired    "That contradicts what"
killed   " we established — it's postgres 16, not mysql 5."
```

The human had written that the staging database was postgres 16; then typed that it was mysql 5;
the SKEPTIC began to correct it; the human typed "Sorry, postgres 16." while it was writing. Four
words had reached the screen. The rest was sampled in silence so the tape holds what it would have
said, and nothing at all reached the document.

*Falsifier: a killed sentence that is not on the tape, or that leaves residue in the buffer, or a
saved file that ever contained a word that was un-said.* — **Did not fire, and the third clause is
unfalsifiable by construction now.** The driver runs the whole act in a window of its own: it
asserts the abort exists, that it died of a real internal event and not a timer, that neither the
aired prefix nor the killed remainder is in the saved file, that the tape holds the counterfactual,
that `Ctrl+R` still folds the log byte-exact, and that the abort is on a chain both verifiers read
as INTACT. **Nothing is undone, because nothing was done:** the forming text is a fourth plane of
the view, so a save during a formation cannot write it and a replay cannot reproduce it.

**Only commits kill, and the killing keystroke is a percept first** — the world's words are on the
trunk before the branch dies, so a re-form would be informed rather than retried. Two things kill:
the newest line accepting the point (`settled_by_world`), or the seat's own margin at or below zero
when asked again (`margin_flipped`). Judgment is delayed inside a generation and ingest is not,
which is SPEC 5.1.4 at the finest grain the program has.

**What the stage cost, measured.** A sentence forms in 230–820 ms; the seam's re-probe is one more
probe (~40 ms) and only when a whole percept lands during a sentence; the view repaints at 30 ms
while forming, which is a render cadence and paces nothing.

**A finding worth more than the feature.** In a long session the seats perseverate: after saying
their piece about one thing they keep re-proposing the same line at every later boundary, and the
manners refuse it every time (`repeat`). Fourteen composed sentences in one driver run produced six
said and eight refused, and the eight were the same two lines over and over. The harness is doing
its job and the disposition is not: say-it-once is a fine-tune target, exactly as the estate has
held since 2026-08-12, and this is the first time nib has measured its own version of it. It is
also why the un-say has a window of its own in the driver — after a long session there is nothing
fresh for a seat to begin, so there is nothing to take back.

### ✔ The read of 0.10.0 · 0.10.1 – 0.10.3

A fresh session read the whole repository and the chat that built Stages 2 and 3, rebuilt, and ran
the oracles; every falsifier held, and the defects were one level below where the falsifiers look
— the review's own sentence about this repository, holding again (`docs/BACKLOG.md`, first
section; the devlog, "the read of 0.10.0"). Three commits, each with its falsifier fired at:

- **0.10.1 — the span memory, and the want's own boundary.** One entry per boundary, 64 deep, a
  struct the selftest fires at; a want whose boundary fell out is refused (`span-unknown`), never
  placed after line 1. Every row a want produces carries the want's boundary, and the forming
  plane is anchored to the same span. *Falsifier: three seats of one boundary counted thrice; a
  fallen-out boundary reported as span zero.* — Did not fire; the un-say's abort row now names the
  claim's own boundary.
- **0.10.2 — own speech through the document** (SPEC 5.1.6 and 6.3.6 amended, 6.2.11.3.1). The
  resident's line reaches the trunk only once the editor has written the block, as an own-speech
  percept on the seat's lane, decoded raw and judged never; both folds attribute by author; a
  refused line never reaches the trunk; a restore cannot replay a line the trunk holds. *Falsifier:
  a percept on the hand's lane beginning with a seat's tag, live or folded; no own-speech row after
  a seat wrote.* — Did not fire: 5 own-speech rows and 0 rows of the hand saying a seat's line,
  across a seed, a restore that replays the lines committed at stop, and a twin.
- **0.10.3 — the manners across the switch, the refusal, the flush clock.** The sidecar carries
  what each seat said; the ladder is a pure check; a checkpoint whose row is in no tape is refused
  before the model loads; the 1500 ms flush clock starts at the percept (SPEC 6.2.13). *Falsifier:
  a restored seat repeats its line; a `t` boundary on the sixth token of a sentence typed after a
  pause.* — Did not fire: the sidecar carries the seats' lines and a restored ladder refuses a
  repeat (seven checks with no model); the un-say log went from three boundaries to two on the
  same typed claim with zero `t` rows, the main case's log reads 57 `b`, 3 `f` and no `t`, and the
  CLI's margins run with the mouth became byte-identical across two runs (SKEPTIC +5.73, SPEAKER
  +4.32, SENTINEL +5.26; 11 boundaries, 33 probes).

The same evening filed four brainstorms with their nulls (`docs/BRAINSTORMS_2026-09-05.md`): voice
as a lane, the two-gear escalation, vision as a floor sensor, and the screen saver in which the
resident holds the floor. None before Stage 4.

### ○ Stage 4 — the two switches, and the paired record

AI on/off; RESIDENT / TURN-BASED. Both on the status line, both on the tape.

*Falsifier: the two modes differ in seat, seed or sampler — at which point the toggle is a
preference and not an experiment.*

**This stage is worth more than livability.** It is the twin race instrumented in the product: the
estate's one number that matters next is a resident against a maximally-good turn-based twin on
identical weights, and every flip of that toggle during real work is a paired sample with exactly
one variable.

### ○ Stage 5 — a week of real work

*Falsifier, and the falsifier for the entire idea: the operator, alone with it for a week, does not
leave it turned on.*

Measured: emissions per hour at somebody's elbow, holds per emission, how often the un-say fired
and whether it was right, and how often the toggle moved and which way.

Nothing in Act II rescues a failure here.

---

## Act II — the LAN

**On by default in the shipped product** (operator ruling, 2026-09-04): the pad finds its peers on
the local subnet unless the toggle says otherwise, and the toggle is on the status line and the
tape. Built after Stage 4 and before Stage 5, so that the week runs on the build that ships. See
`docs/ASSEMBLY.md` §3 for why OT with a host, and not CRDT.

### ○ Stage 6 — discovery

IPv4 only, a UDP beacon, a peer table with a TTL, no configuration.

*Falsifier: two machines on the same subnet that do not find each other within five seconds; or a
beacon that reaches anything off the local segment.*

**Known deployment truth:** school and campus networks very often run client isolation on the
access points precisely to stop student devices talking to each other. That kills UDP discovery
dead. Find out before promising a classroom.

### ○ Stage 7 — rooms

Anyone hosts. Listed, unlisted, hidden. The passphrase gates readability, not merely admission.

*Falsifier: a room's content readable off the wire by somebody who does not have the passphrase.*

### ○ Stage 8 — convergence

`follow`, the host as serialiser, pad handoff. The differential harness against the real
`Changeset.ts` is a prerequisite, not an optional extra.

*Falsifier: two peers whose documents differ after the same edits; or a room that cannot survive
its host handing over.*

---

## Act III — the shape it was always for

### ○ Stage 9 — three seats

Speaker, Skeptic, Sentinel on one KV trunk: fork at 0 MiB, three co-decoding at 1.208× the cost of
one. Moderator *and* contributor *and* watcher in one room on one host GPU.

*Falsifier: three seats cost materially more than 1.3× one seat on this card.*

This is why the seat abstraction exists in Act I with a single seat in it.

---

## Deliberately not doing

⨯ **Bundling Node and Etherpad's server.** It would end the no-network-stack law on day one, ship
a runtime and ~300 MB of `node_modules`, and still leave the client implementing changeset apply
and compose. See ASSEMBLY §2.

⨯ **A CRDT.** Etherpad's OT is verified and on this disk; a CRDT is something new to design and
verify. If Act II proves the host-serialiser intolerable, CRDT returns — paid for by a
measurement, not an instinct. See ASSEMBLY §3.

⨯ **A cloud model in Act I or II.** The moment a room streams to a frontier API, "nothing leaves
the building" is false for everyone in it, not only for whoever flipped the switch.

⨯ **Simulated humanity.** No invented hesitation, no fake typos, no "thinking…" that is not
thinking. The resident's holds are real silences and its aborts are real retractions; that is
enough, and faking it is the exact place a demo stops being evidence.

⨯ **Rich text, plugins, a browser build, accounts, telemetry.**

⨯ **Adopting K5 wholesale, or refactoring what is built to match it.** Operator ruling, 2026-09-05:
the converged kernel is in flux in another session; its concepts are taken as concepts and checked
against the pin; its bytes are not lifted while they move.

---

## The order, and why it is this order

The resident half carries the entire thesis; the network half is upside. Etherpad failed as a
*matching problem* — its value needed two people free in the same instant — and a resident is the
first thing that is always free. So Act I is one person, one pad, one resident, no network at all.
If writing with it is compelling, rooms and classrooms follow. If it is not, no amount of
auto-discovery rescues it.

Within Act I, the order runs from the most bounded work to the least: a format with an existing
oracle, then a document with a mechanical falsifier, then a window, then a resident that only
listens, then one that speaks, then one that can take it back. Each stage is usable before the next
begins, and each has a falsifier that can be fired at with the code that exists at that point.
