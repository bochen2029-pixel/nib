# nib — SPECIFICATION

*Rev 0.5 · 2026-09-04 · normative. Where this document and `docs/BLUEPRINT.md` disagree, this one
governs the built artefact and the blueprint governs the intent. Where either disagrees with
`docs/ASSEMBLY.md` on Etherpad or on the sync model, ASSEMBLY governs. Revs 0.1–0.4 were headed
2026-09-03; git says they were written 2026-09-04, and this file dates by git. Rev 0.5 is the QC
pass of that day (`docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md`, §3 and §8): every clause it
touches carries the date.*

Terms: **MUST**, **MUST NOT**, **SHOULD**, **MAY** carry their usual force. A line marked
**[BUILT]** exists and is covered by `--selftest`; **[SPECIFIED]** is settled but unbuilt;
**[OPEN]** is a decision not yet made, and the open questions are collected in §14.

---

## 1 · Scope

nib is a single Windows executable providing a plain-text editing surface in which a resident
language model, running locally, participates as a second author in real time, without either
party taking a turn. Act II extends the same surface across an IPv4 local network.

**Out of scope, permanently:** rich text, layout, a plugin system, a browser, a cloud service, an
account, telemetry.

**Out of scope for Act I:** the network, other people, more than one resident seat.

---

## 2 · The document

### 2.1 Model **[BUILT]**

2.1.1 A document MUST be represented as an ordered log of Etherpad Easysync changesets, plus the
text those changesets produce. The text is a cache of the log and never the other way round.

2.1.2 Every mutation of the text MUST occur by appending a changeset to the log. No code path may
modify the text directly. (`Doc::splice` and `Doc::apply` are the only mutators.)

2.1.3 Folding the log from the empty document MUST reproduce the text byte for byte, at every
point in the document's life. This is the Stage 0 falsifier and is checkable at runtime with
`Ctrl+R`.

2.1.4 The document MUST hold LF line endings only. Rationale: the changeset format counts `\n`
within each op, and a CRLF document makes every op's line count disagree with its character count.

### 2.2 Encoding **[BUILT]**

2.2.1 Text is UTF-8. Offsets throughout the document model are **byte** offsets.

2.2.2 Caret movement and deletion MUST operate on whole UTF-8 sequences: one keystroke moves or
removes one character, never one byte. **[BUILT 2026-09-04, for every movement]** — a column is a
character, not a byte (`LineIndex::col_of` / `offset_of`), for vertical movement and mouse placement
as well as for the arrows; and `Doc::splice` snaps its bounds to sequence boundaries, so no code
path can commit half a character. Until that day a Down-arrow could land the caret inside a
sequence and the next Backspace removed one byte of it, while `Ctrl+R` still read byte-exact — the
replay check verifies the log, not the text. The text's validity is therefore its own falsifier: a
thousand random edits at random byte offsets over a multi-byte alphabet, `utf8_valid` after every
one (§11.3).

2.2.3 A changeset's `chars` field counts bytes, consistent with Etherpad's own treatment of the
document as a string of code units. **[OPEN — §14.1]** whether this must become code points for
interoperability with a real Etherpad server.

### 2.3 Undo **[BUILT]**

2.3.1 Undo MUST be implemented as a new changeset that inverts the previous edit, appended to the
log. The log MUST NOT be truncated, rewritten or compacted by undo.

2.3.2 An edit's inverse MUST be computed against the text the edit *produces*, not the text it
consumed, since that is the state the inverse will be applied to.

2.3.3 A new edit after one or more undos discards the redo stack. The log retains every revision
regardless.

2.3.4 Undo MUST operate on **groups**, not single edits. A burst of typing is one thing a person
did, and an editor that unpicks it a character at a time is correct against the log and unusable.
An edit joins the open group only if **all** of: the same author, contiguous with the previous
edit, within `Doc::kGroupMs` (700 ms) of it, and of the same kind (inserting or deleting). A
newline closes the group on the edit that inserted it, since a person who pressed Enter has
finished a thought.

2.3.5 The **author** clause of 2.3.4 is load-bearing beyond convenience: from Stage 1 the resident
writes into the same buffer a person is typing into, and its edits MUST NOT fuse into that
person's undo. `Doc::apply` — the path a non-local changeset takes — closes the open group
unconditionally, **and ends the undo history** (2026-09-04). Every stored inverse was computed
against text the foreign change has just altered; a same-length replacement passes every length
guard and would corrupt the document on the next Ctrl+Z. Dropping the stacks is the honest Act I
rule; keeping them needs Etherpad's `follow` (§14.7). `apply` records no inverse of its own.

2.3.7 An undo or redo group MUST be validated against a scratch copy before a byte of the document
moves, and popped only once it has been applied. A failure leaves the document as it was.

2.3.8 An undo or a redo is attributed to the hand that performed it (`Rev.author`), and `Rev.kind`
records that it was one (`u`, `r`; `e` edit, `o` open, `a` a foreign change). An author field never
carries a verb.

2.3.6 The undo and redo stacks carry **opposite orderings**, and both are normative. An undo group
is stored in edit order and applied newest-first, because each inverse was computed against the
text its own edit produced (2.3.2) and they compose only in that order. A redo group is stored in
apply order and applied forwards. Reversing either half-restores a burst.

---

## 3 · The changeset library

### 3.1 Conformance **[BUILT]**

3.1.1 The port MUST accept and emit Etherpad's Easysync wire format without extension. A changeset
produced by nib and one produced by Etherpad for the same edit MUST be byte-identical **for ASCII
text.** nib counts bytes and Etherpad counts UTF-16 code units (§2.2.3, decided in §14.1), so for
non-ASCII text every count differs and the wire is not shared; the differential harness
(`tools/etherpad_harness`, 66/66 against the real `Changeset.ts` on 2026-09-04) generates ASCII
corpora, as Etherpad's own generators do.

3.1.2 `check_rep` MUST verify canonical form by re-serialising the parsed ops and comparing bytes.
A changeset that means the correct thing but serialises differently MUST be refused.

3.1.3 Canonical form comprises: adjacent ops sharing an opcode and attribute string are fused;
within a run, deletes are emitted before inserts; a trailing keep carrying no attributes is left
implicit; a multiline op ends on a newline and any in-line tail is a separate op.

3.1.4 Numbers are lower-case base 36, matching JavaScript's `Number.prototype.toString(36)`.

### 3.2 Built surface **[BUILT]**

| function | source |
|---|---|
| `num_to_string` / `parse_num` | `ChangesetUtils.ts` |
| `Op`, `Op::str` | `Op.ts` |
| `deserialize_ops` | `Changeset.ts` (as a scanner, not `std::regex`) |
| `unpack` / `pack` | `Changeset.ts` |
| `apply_to_text` | `Changeset.ts` |
| `check_rep` | `Changeset.ts` |
| `MergingAssembler` / `SmartAssembler` | `MergingOpAssembler.ts` / `SmartOpAssembler.ts` |
| `ops_from_text` / `make_splice` | `Changeset.ts` |
| `Builder` | `Builder.ts` |

### 3.3 Unbuilt surface **[SPECIFIED]**

3.3.1 `compose(cs1, cs2, pool)` — folds consecutive changesets. Required for revision compaction,
not for correctness. Act I does not need it.

3.3.2 `follow(cs1, cs2, reverseInsertOrder, pool)` — the transform. Required for Act II and for
nothing before it.

3.3.3 `AttributePool`, `AttributeMap`, atext — required for authorship (§7) and any formatting.

3.3.4 Before `compose` or `follow` is trusted, a **differential harness** MUST be built: the real
`Changeset.ts` run over random inputs, compared byte for byte against the port. Hand-picked vectors
are not sufficient for these two functions, because their failure modes are rare and structural.

---

## 4 · The editor

### 4.1 Text surface **[BUILT]**

4.1.1 A fixed-pitch face MUST be used, so a column is arithmetic. The caret's screen position is
derived from the model on every paint and is never tracked independently.

4.1.2 The window MUST be per-monitor DPI aware and MUST rebuild its font on `WM_DPICHANGED`.
**[BUILT 2026-09-04]** — the process sets per-monitor-v2 awareness at startup, resolved by name so
the SDK's version gate does not bind the build. Until that day the process was
`PROCESS_DPI_UNAWARE`, `GetDpiForWindow` answered 96 and this clause was false as built; the driver
now asserts the awareness of the process it launched.

4.1.3 Painting order per line: the selection band, then the glyphs, then the caret.

### 4.2 Input **[BUILT]**

| binding | effect |
|---|---|
| printable character | insert; replaces the selection if there is one |
| `Enter` | insert LF |
| `Tab` | insert four spaces **[OPEN — §14.2]** |
| `Backspace` / `Delete` | delete the selection, else one character |
| arrows, `Home`, `End`, `PgUp`, `PgDn` | move; `Ctrl` extends Home/End to the document |
| `Shift` + any movement | extend the selection |
| click, click-drag | place the caret, select |
| `Ctrl+A` / `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | select all, copy, cut, paste |
| `Ctrl+S` / `Ctrl+Shift+S` / `Ctrl+O` | save, save as, open |
| `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` | undo, redo |
| `Ctrl+R` | fold the log and report whether it is byte-exact |

4.2.1 Typing over a selection MUST produce exactly one changeset, hence one revision and one undo.

4.2.2 Pasted CR and CRLF MUST be normalised to LF on entry (§2.1.4).

### 4.3 Files **[BUILT]**

4.3.1 A save MUST be atomic: write a temporary in the target's directory, flush it to disk, then
replace. A failure at any point MUST leave the previous file intact.

4.3.2 A file's line-ending convention and UTF-8 BOM MUST be detected on open and restored on save.
nib MUST NOT silently convert a file's conventions.

4.3.3 Closing with unsaved changes MUST prompt. Discarding MUST be an explicit choice. So MUST
shutdown and logoff (`WM_QUERYENDSESSION`, 2026-09-04): a cancelled prompt holds the session.

4.3.4 Opening a file restarts the log, with the file's content as revision 1. A different file is
a different document; pretending otherwise would make §2.1.3 false.

### 4.4 The status line **[BUILT]**

4.4.1 The status line MUST show, at all times: caret line and column, selection size when non-zero,
document size, revision count, and whether there are unsaved changes.

4.4.2 When the resident exists it MUST additionally show, permanently and without user action: the
AI switch state, the mode (§6.1), and the egress counter (§9.4).

### 4.5 The theme **[BUILT]**

4.5.1 Colours and the font MUST be read from `nib.theme` beside the executable at startup. Missing
or unparsable entries fall back to compiled defaults.

4.5.2 `tools/theme_detect.py <image>` derives a palette from a screenshot and writes that file.

### 4.6 Not built **[SPECIFIED]**

Find and replace; line numbers; word wrap; multiple documents; a tab bar. None is required by any
stage below Act II.

---

## 5 · Ingest — the compiler

**[BUILT 2026-09-04]** — `src/ingest.h/.cpp`, 39 checks in `--selftest` and 5 more fired from
inside the running window by `tools/drive.py`. The resident that consumes it is §6 and does not
exist yet; everything below is the producing half, and it runs with no model in the process.

5.1.1 The resident's input MUST be the document's op stream, delivered as `Delta` records over
`auricle::fusor::StreamingTextSource` (`C:\auricle\src\fusor\source.h`), unmodified.

5.1.2 A percept MUST be emitted at whichever comes first: a **closed thought**, N characters, or T
milliseconds of quiet. It MUST end on a word boundary wherever the text allows one. N and T are
configuration; their defaults are **[OPEN — §14.3]** and are to be measured, not chosen.

> **CORRECTED 2026-09-04.** This clause previously named *a word boundary* as a trigger. It cannot
> be one. The resident prepends `\n[lane] ` to each `Delta` and runs a final judge when the line
> ends (`fusord.cpp:723-746`), so one percept per word would hand the trunk one bracketed line per
> word and fire three probes on every one — the over-segmentation that `fusord.cpp:242-254` was
> tightened to escape after it was measured on 2026-08-12. A word boundary is where a percept may
> **end**, never a reason for it to end.

5.1.2.1 A closed thought is `.`, `!`, `?` at a word end, or a newline. `;` and `:` are syntax and
MUST NOT close one — fusord measured them firing 3.3×/line on a code paste, handing the probe
fragments like `2);` at higher boundary-mass than real prose. This is the same boundary set, so
train ≡ serve holds.

5.1.2.2 A lane change MUST close the pending clause. Two authors' words are never fused into one
bracketed line.

5.1.3 Deletions MUST be delivered as percepts. A person removing a sentence is information and MUST
NOT be reconciled away silently.

5.1.4 Ingest MUST be unconditional. Only judgment cadence may be modulated under load. A full ring
MUST count the loss loudly; a silently dropped percept is a turn reborn inside the loop.

5.1.5 The compiler MUST NOT summarise, clean, annotate or interpret. The world is never edited.

5.1.6 **Self-echo, both halves.** The GATE half: a delta whose lane is one of the resident's own
seats MUST NOT be fed back as world to judge (`source.h`, spec §5.8). In a pad the resident writes
into the buffer it reads, so this filter MUST live in `PadSource` at the source, not downstream;
its set is the seat set from one source (`register_seats`), and `--selftest` asserts it. The TRUNK
half (2026-09-04): the resident's own emission MUST be committed to the trunk on its seat's lane
(`fusord.cpp:555-561`) so that the mind knows it spoke — without it the SKEPTIC re-fired one catch
at ten consecutive boundaries, and say-it-once is structurally unlearnable ("L-GATE has no self
exception"). nib is a room, not `--pure`; the tape says so.

5.1.7 The compiler MUST chunk at **495 bytes**, and a chunk boundary MUST NOT split a UTF-8
sequence or, where the text allows it, a word.

> **CORRECTED 2026-09-04, measured on this box.** This clause previously read "`kPayloadMax` is 496
> bytes so a `Delta` is 512 bytes on the ring; the compiler MUST chunk at that bound." Both numbers
> were wrong, and one of them dangerously.
>
> - `sizeof(Delta)` is **528**, not 512: 8 (`wall_ms`) + 16 (`lane`) + 2 (`len`) + 496 (`payload`)
>   = 522, padded to 528 by the `uint64_t`'s alignment. The claim originates in a comment in
>   `source.h` and was repeated here without being checked. It is harmless but it is not true.
> - `fill_delta` reserves the final payload byte for a NUL, so handing it exactly `kPayloadMax`
>   bytes yields `len == 495` **with no signal at all**. Chunking "at that bound" would therefore
>   have lost one byte per full chunk, silently — the precise failure 5.1.4 and CLAUDE.md rule 7
>   exist to prevent. The safe bound is `kPayloadMax - 1`, which is what `nib::kChunkMax` is.
> - `lane` is a `strncpy` into 16 bytes, so a lane over 15 characters is also truncated silently.
>   `PadSource` counts those rather than letting them pass.

5.1.7.1 A `PadSource` embeds the 1024-slot ring by value and is therefore ~528 KB. It MUST NOT be
declared as a stack local: a default 1 MB thread stack overflows at construction, which presents
as an immediate crash with no output (`0xC00000FD`) and no clue as to the cause.

5.1.8 The lane string is train ≡ serve: the trunk sees `[lane] text` byte-identically to the soak
and tune format. Lane naming is therefore not a user-interface decision.

5.1.9 Silence MUST enter as world — idle ticks — and not as a question. The tick's text is
`[tick +Ns]`, and it is emitted **before** the percept that broke the silence, as fusord does it.

> **CORRECTED 2026-09-04.** This clause claimed byte-identity with `fusord.cpp:712`, and the stream
> was not identical: fusord decodes `\n[tick +Ns]` raw onto the trunk, while nib wrapped the tick in
> a speaker's line (`\n[bo] [tick +45s]`) — and then judged it, firing three probes on silence,
> which the estate forbids (anti-turn exemption 4: ticks never trigger a probe round). Now a tick
> rides the **empty lane** (`delta_lane`); the resident decodes an empty-lane Delta raw and judges
> nothing. A `Delta` carries no `kind` (5.1.7), so the lane carries the one bit that matters until
> auricle's `Delta` gains a `kind` in its padding. A flushed clause is stamped with the time of its
> last byte, not of the flush, so the silence after it is never overwritten.

5.1.9.1 Because the tick is produced here, at the source, the lifted resident loop MUST run with
its own `--idle-tick-s` set to 0 or the silence is counted twice. The pad is the right place for
it: the pad knows what a typing pause is and a generic resident does not.

5.1.10 A deletion reaches the trunk as the removed text, intact, behind a marker saying it left —
**on every chunk** of a long removal (2026-09-04; a bare tail chunk would read as newly typed text,
the opposite of what happened). The marker is nib's and not the world's, so it is excluded from the
byte-conservation arithmetic of 5.1.11. **[OPEN]** — the marker's wording is off-distribution for v11 and is a decision, not a
finding; it is configuration until it has been looked at against a real trunk.

5.1.11 **The falsifier, stated as arithmetic.** With nothing pending, the bytes that entered the
compiler MUST equal the bytes that left it, and every percept MUST be either pushed to the ring or
counted as dropped: `typed_in == typed_out`, `removed_in == removed_out`, `pushed + dropped ==
percepts`. This is checkable after every keystroke and is fired from inside the running window by
`drive.py`'s `ingest` command, not only in unit tests.

5.1.12 **Undo, redo and open are percepts** (2026-09-04). They change the text without passing
through `edit_splice`; what they remove and restore is perceived by diffing the text before and
after, on the hand's lane. Until that day an undo emptied the document while every counter of
5.1.11 stood still and read green — the identity audits the compiler, not the feed, so the driver
now asserts that an undo moves the removed-bytes count. A different file is a different world: the
compiler starts over on open and the file's text is perceived as the hand's own, until the tape
(§8) can offer better provenance.

---

## 6 · The resident

**[PARTLY BUILT 2026-09-04]** — `src/resident.h/.cpp`. §6.2 (the loop) is built and its seed is
hashed against fusord's pin. §6.3 (emission) and §6.4 (un-saying) are **structurally absent**, not
merely unbuilt: there is no generation code in the file. §6.1's two switches are not built.

6.0.1 The resident MUST refuse to start if no GPU backend came up while GPU layers were requested.
A `ggml` build that cannot load `ggml-cuda.dll` reports no error and offloads nothing; the model
then runs entirely on the CPU at roughly 1/47th of the speed. Measured on this box 2026-09-04:
556 s against 11.8 s for the same 101 words. It also **corrupts the judgment record**, because the
1500 ms flush law fires on wall-clock time — the slow run invented 30 boundaries where the correct
one finds 18. A silent fallback is therefore not a degraded mode, it is a wrong answer, and
`Resident::start` enumerates the devices, names them, and refuses unless `allow_cpu` is set.

6.0.2 **[BUILT 2026-09-04]** The backends MUST be loaded by name, never by directory, and the
resident MUST refuse to start if a network module is in the process (the runtime half of §9.1).
`ggml_backend_load_all_from_path` loads every backend it knows, including `ggml-rpc.dll`, which
imports ws2_32, and any DLL named in `GGML_BACKEND_PATH`. nib loads `ggml-cuda.dll` if present and
the best-scoring `ggml-cpu-*.dll`, then enumerates the process's modules; `nib --about` prints the
verdict without loading a model.

6.0.3 **[BUILT 2026-09-04]** A full context window is a REFUSAL: the resident counts the words it
did not perceive, judges nothing further — never a clause the trunk only partly saw — and the run
is reported as not a record (exit 4). A failed decode is likewise reported, never a discarded return
value. `n_ctx` below 2048 is refused. The molt that makes the window a non-event is Stage 2's.

### 6.1 The two switches

6.1.1 **AI — on/off.** Off means no ingest, no trunk, no context held. Not muted, not paused. The
state MUST be on the status line and on the tape.

6.1.2 **RESIDENT / TURN-BASED.** The seat, the seed, the sampler and the mandate MUST be identical
across the toggle. **Only the trigger may differ**: evidence at a thought boundary, versus an
explicit keystroke. Any other difference makes the toggle a preference instead of an experiment.

6.1.3 Every transition of either switch MUST be recorded on the tape with its timestamp.

### 6.2 The loop

6.2.1 The loop MUST be lifted from `fusord.cpp`, not re-derived.

6.2.2 `SEED_SYS`, `SEED_EXAMPLES`, `SEED_OPEN`, the seat probe and the speak-cue MUST be copied
verbatim. The build MUST hash them and refuse to run if a character has drifted, because v11's
dial-0 calibration is off-distribution otherwise.

6.2.3 Judgment MUST ride the free tail of the ingest pass. The model MUST NOT be polled.

6.2.4 **[BUILT]** The pinned serve hash is `0xe7ffa5704ba31076`, computed over `SEED_SYS`,
`SEED_EXAMPLES`, `SEED_OPEN`, the three seats' names and mandates, the probe frame and the
speak-cue frame. nib computes the same number as fusord, which is what makes 6.2.2's "copied
verbatim" a checked fact rather than a promise. The check is pure string arithmetic and runs in
every `--selftest`, with no model and no GPU, because a gate that only fires when a 9B is loaded
is a gate that stops being checked.

6.2.5 **[BUILT]** The speak-cue is carried and hashed even though Stage 1b never decodes it. The
pin covers it, and a constant that exists only to be hashed is the cheapest way to keep Stage 2
from drifting before it is written.

6.2.6 **[BUILT]** `n_ctx` is 8192 by default, not fusord's 65536. This card is shared with
llama-server and a speech stack; a 64k q8_0 window costs roughly 3 GB of KV and would evict them.
It is configuration. Stage 1b needs a window long enough to judge in, not long enough to live in —
the resident window and the molt are Stage 2's problem.

### 6.3 Emission

6.3.1 The resident MUST write into its own blocks, never inside a human's paragraph.

6.3.2 An emission targeting a block that has received a human keystroke within the floor window
MUST be refused before it is composed. The default floor window is 2 s.

6.3.3 Emission rate MUST express a real internal state. Simulating a human tell that does not
correspond to an internal event — invented hesitation, fake typos, a "thinking…" indicator that
is not thinking — is forbidden.

### 6.4 Un-saying

6.4.1 A forming sentence occupies a provisional region: rendered, visually distinct, never
committed, never saved, never present in the file.

6.4.2 On abort, the characters MUST be withdrawn from the view and the abort MUST be committed to
the tape with what was formed, what reached the air, and why it died.

6.4.3 `Ctrl+S` during formation MUST write the committed document only. Reflex partials never
persist, enforced by the file format and not by a code path.

---

## 7 · Authorship

7.1.1 **[SPECIFIED]** Every character MUST carry its author as an Easysync attribute, via the
attribute pool (§3.3.3).

7.1.2 Authorship MUST be visible in the view, preserved in the log, and recoverable from the tape.

7.1.3 It MUST never be ambiguous who is on the other end. This holds when the document is shared
with people who did not start it.

---

## 8 · The tape

**[SPECIFIED]**

8.1.1 The tape MUST be append-only and hash-chained in the family's format (BLAKE2b-256 over the
previous digest, a NUL, and the canonical JSON payload), so that `glance --verify` reads it.

8.1.2 It MUST record: every changeset, every judgment with its margin, every hold, every abort,
every switch transition, the model's hash, and the seat's mandate.

8.1.3 A reader of the tape MUST be able to determine which machine was on the other end at any
moment in the session.

---

## 9 · Boundaries

9.1 **Until Stage 6, the build MUST link no network stack and the process MUST hold none.** Two
gates, both mechanical **[BUILT 2026-09-04]**: `build.bat` fails on `ws2_32`, `wininet`, `winhttp`,
`urlmon`, `dnsapi` among the dependents, and asserts that the llama DLLs are delay-loaded; and at
run time the resident refuses to start if any of those modules, or `ggml-rpc.dll`, is in the
process (6.0.2). The first gate proves the file; the second proves the process, because a
`LoadLibrary` at `--resident` time is invisible to `dumpbin`.

9.1.1 **Operator ruling, 2026-09-04: LAN autodiscovery is ON by default in the shipped product** —
toggled off, never absent. §9.1 is therefore a build-phase gate that ends at Stage 6, where 9.2
replaces it. The build order is unchanged (Stages 1c–4 before 6); the product ships with the LAN on.

9.2 **[SPECIFIED]** Stage 6 narrows rather than removes this gate: no HTTP client, no DNS resolver,
a socket layer that refuses any destination outside the machine's own subnet by construction, and
the runtime module gate of 6.0.2 kept for `wininet`, `winhttp`, `urlmon` and `dnsapi`.

9.3 No frontier API in Act I or Act II. When one arrives it is behind a flag, and the room shows a
permanent badge.

9.4 **[SPECIFIED]** The status line MUST carry an egress counter. Until Stage 6 it reads zero
because that is structurally true, not because nothing has been sent yet — and it may read zero
only while both gates of 9.1 hold. It is not built; it belongs with Stage 1c's status line.

9.5 The resident MUST NOT be able to act outside the document: no shell, no file system beyond the
open document, no input synthesis, no window manipulation.

---

## 10 · Act II — the network

**[SPECIFIED]** — see `docs/ASSEMBLY.md` §3 for why OT and not CRDT. **Operator ruling,
2026-09-04: discovery is ON by default in the shipped product**, toggled off, never absent; the
toggle's state is on the status line and on the tape like the other two switches (§6.1).

10.1 Discovery: IPv4 only. A periodic UDP beacon and a peer table with a TTL. No IPv6, no mDNS
dependency, no configuration. On by default.

10.2 A room has a host that linearises. **Consequence, recorded as a cost:** a room needs its host,
and a host that leaves ends the room or hands it over. Handoff is a state transfer, not a consensus
protocol.

10.3 Rooms are listed, unlisted (joinable by exact name), or hidden.

10.4 A room passphrase MUST gate readability, not merely admission: derive a key and encrypt the
room's traffic. Otherwise anyone on the segment reads the room and "password-protected" is theatre.

10.5 A resident's tape in a shared room records *what that mind perceived and did*, not what the
room objectively was. This is a real weakening of the tape's claim and MUST be stated in the
product, not only here.

10.6 On a healed partition, edits arrive from the past. A resident that has already emitted on an
incomplete view MUST withdraw (§6.4). Partition-heal is wired to the abort path deliberately.

---

## 11 · Testing

11.1 `--selftest` MUST pass before every commit. **[BUILT: 144 checks, 2026-09-04]**

11.2 Every stage below Stage 2 MUST remain runnable with no model in the process. A battery that
needs a 9B on a busy card is a battery that stops being run.

11.3 Property tests over random input are required where the space is large: ten thousand random
splices (canonical form and applied result), one thousand random edits interleaved with undos
(replay equality). Generators MUST be deterministic so a failure names a case to re-run.

11.4 The window MUST be verified by a driver that posts window messages and reads an artefact —
`WM_APP+1` commands and the `NIB_LOG` file. **Synthesising global input is forbidden**: it lands
wherever the focus happens to be, which can type into another application's window. **[BUILT]** —
`tools/drive.py`, 27 checks, 2026-09-04.

11.4.3 **A driven window MUST NOT take the keyboard** (2026-09-04). The driver sets `NIB_DRIVER`,
and the window it drives is created no-activate and shown without activation: posted messages
still arrive, the keyboard never does. Before this a scratch window took the foreground and ate
what the operator was typing to another program, and the fragments turned up in the scratch files.

11.4.1 The driver MUST bind to the window of the process it launched, not to a class name.
`FindWindow` by class alone returns any nib window, including one left over from an earlier case or
one the operator has open; two runs in three failed that way, and the failure presents as a defect
in the editor rather than in the harness.

11.4.2 The driver MUST wait on artefacts — a further line appearing in `NIB_LOG` — rather than
sleeping a fixed interval. A battery that sleeps is a battery that is flaky on a loaded box.

11.5 No performance number appears in any document or on any surface unless it was measured on the
machine it claims, with its date.

---

## 12 · Non-goals

Collaborative rich text · a plugin API · a server anyone else runs · mobile · a web build ·
compatibility with Etherpad's *server* (the wire format is shared; the product is not) · any
feature whose absence Act I's falsifier (§13) would not notice.

---

## 13 · The falsifier

**If the operator, alone with nib for a week of real work, does not leave it turned on, the idea is
wrong.** No amount of Act II rescues that, and the finding is published beside the laws it bought.

---

## 14 · Open questions

14.1 **Decided 2026-09-04: bytes.** `chars` counts UTF-8 bytes; that is deliberately not
Etherpad-wire-compatible for non-ASCII text, and Act I does not need it. Code points would buy
neither interoperability (Etherpad counts UTF-16 units, so an emoji is 2 there and would be 1
here) nor simplicity, and are rejected. The named future work, for the day a real Etherpad server
is on the other end: an adapter at the wire that maps byte offsets to code-unit offsets over the
current text and re-`pack`s. (`docs/review/ETHERPAD_DEEP_READ.md` §3.)

14.2 Tab: four spaces, a real tab, or configurable. Currently spaces, unjustified.

14.3 The ingest compiler's N and T. To be measured against real typing, not chosen.

14.4 Whether the resident writes in the same buffer or an adjacent lane, once §6.3 has been used in
anger.

14.5 Where a room's identity comes from in Act II — a name, a key, or both.

14.6 Whether the tape should be encrypted at rest, as caseclock's is.

14.7 Undo across a foreign change. Act I drops the history (2.3.5); the transform that would keep
it is Etherpad's `follow`, which Act II ports. Whether losing undo across a resident's emission
hurts in practice is a Stage 5 measurement, not a guess.

14.8 A fourth percept kind, *open fragment*. A Delta the compiler flushed on quiet mid-sentence is
judged as a final today, exactly as a bridge's complete line would be; whether that over-segments
at pad grain (three probes per half-second pause) is to be measured against real typing before a
kind is added (`docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md`, F8 and §6.2).
