# HANDOFF — read this first

*Written 2026-09-05 at nib **0.10.0**, Stages 0a–3 green, immediately after the un-say landed and
the repository went public. It replaces the 2026-09-04 handoff, which is kept whole at
`docs/HANDOFF_2026-09-04.md` because it is the record of how the project started.*

**This file is three things at once.** A **continuation prompt** for the session that is already
building; an **initialisation** for a session that has never seen this estate; and a
**rehydration** for a session whose context was trimmed mid-flight. Read §1 and §2, then §12 for
what to do next. Everything load-bearing is either in this file or named by absolute path.

---

## 1 · What nib is, in one paragraph

**A writing surface with no send key on either side.** You type; a locally-resident 9B model
perceives as you type, judging at every thought boundary whether this instant deserves a word. It
writes; the words appear as they are sampled. Neither of you takes a turn. If your next sentence
contradicts one it has begun, it stops mid-word and the words are withdrawn — and what reached the
screen, what it would have gone on to say, and why it died are all on an append-only hash-chained
tape. One Windows executable, no runtime, no browser, and no network stack at all: the build fails
if a socket is ever linked, and the resident refuses to start if a network module has entered the
process. Later, on a LAN, other people join the same pad. That is Act II and it waits.

**The thesis, which sets the build order.** Etherpad did not fail as technology; it failed as a
*matching problem* — its value needed two people free in the same instant, the scarcest thing in
collaboration. A resident mind is the first thing that is always free. So the resident half carries
the entire thesis and the network half is upside. And there is no system prompt: a system prompt is
an artefact of amnesia, and a resident does not have amnesia. The interaction is the prompt.

**The falsifier for the whole idea** arrives at Stage 5 and not before: *if the operator, alone with
nib for a week of real work, does not leave it turned on, the idea is wrong.* Nothing in Act II
rescues that, and the finding gets published beside the laws it bought.

---

## 2 · Where it stands, exactly

| | |
|---|---|
| version | **0.10.0**, commit `e9cdbd9`, branch `main`, working tree clean |
| public | **https://github.com/bochen2029-pixel/nib** — MIT, pushed 2026-09-05, 21 commits |
| stages | 0a, 0b, 0c, 0d, 0e, 1a, 1b, 1c, 1d, 2, 3 — **all green, every falsifier fired at** |
| next | **Stage 4** — the two switches and the paired record |
| oracles | `--selftest` **213** · `tools/drive.py` **30** · `tools/drive.py --ai` **65** |

```bash
build.bat                          # /W4 /WX, zero warnings, two gates
nib.exe --selftest                 # 213 passed, 0 failed        (no model, no GPU)
python tools/drive.py              # 30 passed, 0 failed         (no model, no GPU)
python tools/drive.py --ai         # 65 passed, 0 failed         (needs the card, ~6 min)
nib.exe --about                    # the serve pin, the backends by name, the module gate
```

**What works today, end to end.** Open a file. Press `Ctrl+Shift+A`. The model loads on its own
thread while the window keeps painting; if a checkpoint is beside the document the mind that was
there comes back, told how long it was away and shown what happened meanwhile. Type. The gutter
brightens where a seat wanted to speak. Pause two seconds and a seat writes a block of its own,
prefixed with its name. Contradict a sentence while it is forming and it is withdrawn mid-word.
Press `Ctrl+Shift+A` again and the trunk is saved and the card comes back.

---

## 3 · Read these, in this order

1. `CLAUDE.md` — the hard rules and the refusals, written as laws with reasons. **Rule 2 (no
   network), rule 3 (forming text never persists), rule 4 (never write into a block a human is
   touching), rule 5 (never simulate a tell that is not a real internal event), rule 8 (every state
   that changes what the resident is goes on the tape), rule 11 (one process, two clocks), rule 12
   (a driven window never takes the keyboard).** Plus the versioned-edits rule (§10 below).
2. `docs/SPEC.md` — **normative**, rev 0.9. Every line marked BUILT / SPECIFIED / OPEN. Where this
   handoff and the SPEC disagree, the SPEC governs the artefact.
3. `docs/ROADMAP.md` — the stages, each with its falsifier and whether it fired, and every measured
   number with its date.
4. `docs/BACKLOG.md` — what is open, newest section first.
5. `docs/devlog.md` — the lab notebook: what was tried, what was measured, what was decided, and
   **traps the day they bit**. Never rewritten; corrections are appended in the same voice.
6. `docs/ASSEMBLY.md` — why the Easysync library is ported and why OT rather than CRDT.
   Supersedes BLUEPRINT §2 and §7 where they disagree.
7. `docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md` — the review that set the work order. §3 is the QC,
   §5 the design at the edge, §6 the falsifiers and metrics, §8 the remediation list.
8. `docs/review/LIFT_MAP_K5_2026-09-05.md` — **what to lift from the converged kernel and what to
   refuse.** Read before touching the resident's loop.

---

## 4 · The machine — rules that cost real time

These are not style preferences. Each one was paid for.

**4.1 File content NEVER travels through a shell literal.** Heredocs are blocked by a hook on both
the Bash and PowerShell tools; they mangle apostrophes and backslashes and silently truncate past
~10 K. Author files with the **Write** tool, change them with **Edit**, and use the shell to *run*
things.

**4.2 Forward slashes in every shell command.** Git Bash eats backslashes in unquoted arguments:
`python C:\x\y.py` arrives as `C:xy.py`.

**4.3 `build.bat` needs an absolute path from PowerShell:** `cmd /c C:\nib\build.bat`. And a bare
`find` or `more` inside a batch file launched from Git Bash resolves to Git's Unix tools — which is
why the build script names `%SystemRoot%\System32\find.exe` explicitly. `find /c /v ""` once walked
the whole drive for ten minutes.

**4.4 Do not load this machine.** It is the operator's working box, with llama-server, a speech
stack and other agent sessions live on it. No load generators, ever.

**4.5 Never synthesise global input.** `keybd_event` / `SendInput` land wherever the focus happens
to be. nib has a proper seam instead: `WM_APP+1` commands and the `NIB_LOG` file.

**4.6 A driven window must never take the keyboard, and must never read the thread's key state.**
`NIB_DRIVER` makes the window no-activate. And `GetKeyState` answers for the *thread*, so a driven
window reads whichever modifier the operator is holding in another program — which silently
swallowed five characters out of a typed sentence on 2026-09-05. Both halves are in `WM_CHAR` now.

**4.7 One model on the card at a time.** `drive.py --ai`, `nib --resident` and any `--script` run
each load the 9B; two at once with llama-server resident do not fit in 16 GB. **And the probe cost
moves by 2.7× with what else is on the card** — a red `--ai` run is worth re-running once before it
is believed. Never kill a nib window to free the card: it may hold unsaved text.

**4.8 A DPI-unaware process measuring a DPI-aware window is told a lie, and the lie is
self-consistent.** `GetWindowRect` answered 887 where nib correctly saw 1997 physical pixels, so a
screen capture rendered a 1997-pixel window into an 887-pixel bitmap and clipped it — which read
exactly like a word-wrap bug. Call `SetProcessDpiAwarenessContext(-4)` first in any tool that
measures, sizes or captures nib's window. nib prints its own geometry when the wrap switch moves;
when the two disagree, the unaware one is wrong.

**4.9 A crash inside a DLL says nothing unless you make it.** Three instruments exist and cost
nothing: llama and ggml errors always reach stderr; the window writes a `crash` line to `NIB_LOG`
from a terminate handler and an unhandled-exception filter; `NIB_TRACE=1` prints the resident's
start-up steps. Bisect with an env knob (`NIB_CHUNK` found the cuBLAS cliff) rather than rebuilding
per guess.

**4.10 A test that restores state from a previous run is testing the previous run.** The un-say case
failed twice because it inherited a tape and a checkpoint from an earlier `--keep` run: the
resident restored, had already said its piece, and the manners refused every repeat. Every driver
case that involves the resident clears its own slate now.

---

## 5 · The architecture, file by file

One executable. `build.bat` compiles nine translation units and links kernel32, user32, gdi32,
comdlg32, bcrypt — **no network DLL, enforced at build** — plus llama.cpp and ggml, all three
**delay-loaded**, which the build asserts rather than assumes. Nothing touches a llama symbol until
the resident is switched on, so `--selftest`, `--ingest` and the editor run on a machine with no
model and no card.

| file | what it is, and the law it carries |
|---|---|
| `src/changeset.h/.cpp` | Etherpad's Easysync format in C++: base36, `Op`, `deserialize_ops` (a scanner, not `std::regex`), `unpack`/`pack`, `apply_to_text`, the three assemblers, `check_rep`, `ops_from_text`, `make_splice`, `Builder`. **Canonical form is the real test:** `check_rep` re-serialises what it parsed and demands byte-identity. |
| `src/doc.h/.cpp` | The document is an **op log**, not a string. Every edit is a changeset; undo is an inverse changeset *appended*, never a truncation. `LineIndex` (logical lines) and `RowIndex` (visual rows — word wrap). `Doc::text_at` folds to any revision. Columns are **characters**, and `splice` snaps to UTF-8 sequence boundaries. |
| `src/ingest.h/.cpp` | The compiler and `PadSource`. Edits become percepts; percepts become `Delta`s on auricle's lock-free ring, with a second ring of `PerceptMeta` in lockstep carrying id, revision and byte span. Byte conservation is the falsifier. A full ring **spools**, never drops. Self-echo is filtered at the door. |
| `src/resident.h/.cpp` | The trunk, the segmenter, the three seats, the probe, **the mouth**, the manners, and the seam. The loop is **lifted** from fusord, not re-derived, and `serve_hash()` proves it byte-for-byte. |
| `src/wire.h/.cpp` | The resident on its own thread inside the window. Two rings out (judgments, emissions), a forming *state*, the floor gate, the checkpoint, `fold_log` and `fold_tape`. |
| `src/tape.h/.cpp` | The family's append-only hash-chained record: BLAKE2b-256, canonical JSON, six keys a row. Ported from fray, which is glance's, which is REGISTRAR's. Torn-row recovery. Also SHA-256 (the model's hash) and the atomic file helpers. |
| `src/edit.cpp` | The Win32/GDI window. Every edit goes through `Doc::splice`. Per-monitor DPI, word wrap, the gutter, two status rows, the forming plane, the floor's second gate, block placement, the driver seam. |
| `src/selftest.cpp` | The oracle: 213 checks, none of which need a model. |
| `src/nib.cpp` | The console verbs. |
| `tools/drive.py` | The window battery: posts messages, reads artefacts, never synthesises input and never looks at the screen. |
| `tools/snap.py` | Gitignored snapshots with an MD5 manifest (§10). |

### The two clocks (CLAUDE.md rule 11)

The editor thread owns the document, the view, the pad and the tape. The **wire** owns a second
thread that owns the resident. They meet only at lock-free rings; the editor never calls the model
and the resident never touches the window; neither ever waits on the other. The price of one
process is that a driver fault takes the editor with it, and the mitigations are: forming text is
never persisted, the save is atomic, and the tape is durable as it goes.

---

## 6 · The laws that are structural, not advisory

- **Emission is absent, not disabled, when it is off.** With `Config::emit` false no sampler is
  constructed, so no path in the process can produce a token. Stage 1b was a whole stage to
  establish this property and it survives for every build that does not ask for a mouth.
- **Percepts are never dropped; judgment may be delayed.** The ring spools rather than drops; the
  spool's own cap is counted loudly. Inside a generation, judgment is delayed and ingest is not.
- **Own speech is a percept, with no self-exception** — the trunk half (a seat's line is committed
  on its own lane, or say-it-once is structurally unlearnable) and the gate half (the same line is
  dropped at the pad's door). Both halves are exercised by one line of text.
- **A seat's line waits for the world's line to close** before joining the trunk, because a
  boundary can fire mid-percept and a line spliced in there is a serve-format drift the tune never
  saw.
- **The floor: pausing is how a person yields it.** A margin above zero records a *want* and
  composes nothing; wants are composed when the hand has been still. Refusing costs nothing because
  the model is never run. Checked again at the moment of writing.
- **The forming plane is never in the `Doc`.** So a save cannot write it and a replay cannot
  reproduce it. The withdrawal is not an undo; nothing was done.
- **Only commits kill.** The world's words are on the trunk before the branch dies.
- **A judgment is always about now.** The 9B is a 3:1 recurrent hybrid; a fork's recurrent state
  cannot be rewound, so there is no such thing as re-judging "as of then".
- **Ticks are world, never a poll.** `[tick +Ns]` rides the empty lane, is decoded raw, and fires
  no probe round.
- **Every state that changes what the resident is goes on the tape** — the AI switch, word wrap,
  every coefficient, the model's SHA-256, the seats' mandates.

---

## 7 · Every measured number, with its date

All on this box: Windows 11 24H2, 225 % DPI, RTX 4070 Ti SUPER (16 GB) **shared** with llama-server
and a speech stack. Model `Qwen3.5-9B-emit-v11-Q5_K_M`, `n_ctx` 16384, q8_0 KV, flash attention.

| what | value | when |
|---|---|---|
| keystroke → painted, AI off | p50 **605–657 µs**, p95 1.1–1.6 ms | 09-05 |
| keystroke → painted, AI on | p50 **455–1713 µs**, p95 1.0–2.7 ms | 09-05 |
| probe, three seats, quiet card | **107–122 ms** per boundary | 09-05 |
| probe, same binary, contended card | **312–327 ms** per boundary | 09-05 |
| q8_0 KV | **136 MiB** at 8192, **272 MiB** at 16384 | 09-04 |
| model load | **4.3–8.1 s** | 09-05 |
| model SHA-256 (6.64 GB) | **17 s** first time, **0 ms** cached | 09-05 |
| checkpoint | **58.8 MB** at 350 tokens, written in **51–68 ms** | 09-05 |
| VRAM taken and returned | 7080 → 14153 → 7311 MiB | 09-05 |
| fold over a 4.2 KB file, word grain | **39 s** (104 boundaries, 312 probes) | 09-05 |
| one sentence composed | **11–19 tokens, 230–820 ms** | 09-05 |
| the un-say | +5.06 → **−1.77**, four words aired | 09-05 |
| CPU fallback penalty | **47×** (556 s against 11.8 s) | 09-04 |

**The batch cap that every margin depends on.** The decode batch is capped at **64 tokens**
(SPEC 6.2.12). Above ~64 rows ggml-cuda leaves its quantized matmul for cuBLAS, and that path
aborts the process on the *second* model loaded into one process — every life of the AI switch
after the first. The cap also makes lives **numerically identical**: without it, life one judges
through cuBLAS and life two through the quantized path, and the same sentence scores differently in
the same session (mean 0.16, max 0.84 logits apart). **Every margin measured before 2026-09-05 dawn
was taken on the other kernel path.** If a margin ever disagrees with a published table, check this
first. `NIB_CHUNK` overrides it for bisection only.

---

## 8 · The stages, and their falsifiers

Every stage carries a falsifier — the observation that would say it failed — and a stage is done
only when the falsifier has been fired at and did not go off.

| stage | property | evidence |
|---|---|---|
| 0a | the port agrees with Etherpad | 41 checks on Etherpad's own vectors + 4 negative canonical-form cases |
| 0b | a splice is canonical and applies | **10,000 random splices**, deterministic generator |
| 0c | the log replays byte-exact | **1,000 random edits interleaved with undos**, checked after every one |
| 0d | no save loses a file or changes its conventions | bytes compared on disk; CRLF stayed CRLF, a BOM came back |
| 0e | every save path reachable without global input | 17 checks, three identical runs |
| 1a | no percept dropped without a loud count | byte conservation over **4,000 random** typings, removals and idles |
| 1b | margins move with content | they move **per seat, by mandate**; a false claim moves the SKEPTIC ~12 logits |
| 1c | the mind in the window costs the hand nothing it can feel | latency table above; VRAM returned; tape INTACT under two verifiers |
| 1d | the trunk survives being switched off | 58.8 MB saved and restored; a disagreeing sidecar is refused into the **twin** |
| 2 | it writes in its own blocks, and not while you type | **nothing written while the hand moved**; a line written once it paused |
| 3 | a sentence begun can be taken back | `+5.06 → −1.77`; neither the aired prefix nor the killed remainder in the file |

### Next: Stage 4 — the two switches, and the paired record

AI on/off (built) and **RESIDENT / TURN-BASED** (not built). The seat, the seed, the sampler and the
mandate MUST be identical across the toggle; **only the trigger may differ** — evidence at a thought
boundary, versus an explicit keystroke. Any other difference makes the toggle a preference instead
of an experiment.

*Falsifier: the two modes differ in seat, seed or sampler; or a scaffold present in one arm and
absent from the other.*

**This stage is worth more than livability.** It is the estate's twin race instrumented inside the
product: the one number the corpus has called decisive for a year and has never run. Every flip
during real work is a paired sample with exactly one variable. The review's §5.5 adds two things
worth building: keep the boundary judgments running **in shadow** during TURN-BASED (so every
period yields what the resident *would* have done beside what the twin did), and `nib --twin
<tape>` to re-drive a recorded stream through a turn-based policy offline. And it carries an honest
limitation to print with every table: nib's pairs are **observational, not matched-input** — the
human types different things in the two modes, and the mode changes what they type.

Then **Stage 5, the week of real work** — the falsifier for the whole idea. Then Act II: discovery
(on by default in the shipped product, by operator ruling), rooms, convergence.

---

## 9 · The estate around nib

| tool | path | state |
|---|---|---|
| **nib** | `C:\nib` | **0.10.0 · the active work · public** |
| facet, vramtop, caseclock, glance, everywho, fray | `C:\facet`, `C:\GPUz`, `C:\caseclock`, `C:\glance`, `C:\Intellect_AI_tools\everywho`, `C:\fray` | shipped tools; **glance's `--verify` reads nib's tape** |

**The substrate.** `C:\NEW\FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md` is what FUSOR is.
`C:\auricle_snapshots\20260813-131533\src\fusor\fusord.cpp` is the loop nib lifted (the 08-12
kernel). `C:\auricle\src\fusor\source.h` is the intake seam, **included unmodified** rather than
copied. `C:\etherpad-develop` is Etherpad's source, on disk.

**K5, the converged kernel.** `C:\fusor1\converge\src\fusord.cpp` (2026-09-04 evening, in flux in
another session) is the convergence of the two fusord lineages that came *after* the kernel nib
lifted from. Same serve pin. **Operator ruling, 2026-09-05: a source of ideas, not of bytes.** What
nib took: a judgment is always about now; the trunk is an asset; the seam; the acceptance-detector
fix (whole words, so "that is incorrect" is not "correct"); atomic writes with a retried rename;
torn-row recovery; free VRAM beside every probe. What nib **refused on merit**: queueing the hand's
keystrokes during a load (nib's log already has them with their clock — queueing perceives them
twice); gear 2 and the counsel loop; the verdict wire as a second file; and its backend loader,
which pulls `ggml-rpc.dll` and with it ws2_32 into the process.

**The serve pin.** `serve_hash()` computes `0xe7ffa5704ba31076` over the seed, the six worked
examples, the stream opener, the three seats' names and mandates, the probe frame and the speak-cue
frame. It is fusord's own pin from 2026-08-12, and nib computing the same number is the mechanical
proof that the lift was verbatim. **The resident refuses to start if it moves.** The check is pure
string arithmetic and runs in every `--selftest`, with no model and no card. Do not "improve",
reflow or reformat any of those literals.

---

## 10 · Session discipline

- **Versioned edits (operator ruling, 2026-09-05).** An existing source file is never rewritten
  whole: change it with surgical edits that fail atomically on a mismatch, write only NEW files
  whole, and commit at every green step. Bought with an incident: a whole-file write of
  `src/wire.cpp` was cut mid-token and the turn that wrote it left the writer's context.
- **Snapshots.** `python tools/snap.py <label>` copies every source, tool and document into
  `versions/<stamp>-<label>/` with an MD5 manifest, **gitignored**, before a risky step and after
  every green commit. `--list` and `--diff <dir>`. A backup git can reset away is not a backup.
- **Both oracles before every commit**, and three identical driver runs when the window changed.
- **Commit messages are prose**, several paragraphs, explaining the decision and the traps. Write
  them to a file and use `git commit -F`. End with the attribution trailer the harness specifies.
- **No performance number anywhere it was not measured, with its date.** No date that is not git's.
- **The devlog is a notebook.** Failures stay printed beside the fixes. Traps go in the day they
  bite.
- **Prose voice:** plain, specific, no marketing. When a claim is unproven, the document says so in
  the same sentence.

---

## 11 · Open gaps, ranked

Full list in `docs/BACKLOG.md`. The ones that matter:

1. **The manners' memory dies with the resident while the trunk survives.** A restored resident has
   its own lines on its trunk but an empty suppression ladder, so a seat can repeat itself once
   after the switch. Three strings in the checkpoint's sidecar. **Should land before the week.**
2. **The seats perseverate.** One driver run composed fourteen sentences: six said, eight refused,
   and the eight were the same two lines. The harness is doing its job and the disposition is not.
   Say-it-once is a fine-tune target; nib now measures its own rate to tune against.
3. **`settled_by_world` has never fired in a measured run** — only `margin_flipped`. The acceptance
   phrase list is small. Sweep the tapes offline before widening it: a false acceptance silences a
   seat.
4. **The floor is global, not per block.** Stricter than the spec requires, therefore safe, but
   writing in one paragraph silences a seat that wanted to speak about another.
5. **The `.prev` checkpoint generation is written and never read.** Needs a fault-injection test.
6. **The first fold over a long document costs 39 s** at word grain. The checkpoint means it is
   normally paid once.
7. **The T sweep** (SPEC 14.3) needs a real typing tape; the synthetic cadence never pauses.
8. **The driver's windows sit on the operator's screen.** Move them off-screen, not hidden — a
   hidden window gets no `WM_PAINT` and the latency instrument measures keystroke to painted.

---

## 12 · Resume here

**If you are continuing the work:** Stage 4, per §8. Read `docs/review/LIFT_MAP_K5_2026-09-05.md`
before touching the resident's loop, and the review's §5.5 for the shadow twin and `--twin`. The
cheap win first, if you want one: gap 1 above (the manners in the sidecar).

**If you are a fresh session:** read `CLAUDE.md`, then `docs/SPEC.md`, then this file's §4 and §10,
then `build.bat && nib.exe --selftest && python tools/drive.py`. Do not run `--ai` until you have
checked the card with `nvidia-smi` and know no other model is resident.

**If you are rehydrating after a trim:** §2 is the state, §7 is every number you may quote, §10 is
how to work, §11 is what is unfinished. The git log is the narrative — each commit message is a
paragraph of prose about what was decided and why.

**Before you start any GPU work:** `nvidia-smi`, and remember that one model on the card at a time
is a rule and that a red `--ai` run is worth re-running once before it is believed.

---

## 13 · The line

**If the operator, alone with nib for a week of real work, does not leave it turned on, the idea is
wrong.** Nothing in Act II rescues that, and the finding gets published beside the laws it bought.

Everything before Stage 5 exists to make that week possible and to make its answer trustworthy.
