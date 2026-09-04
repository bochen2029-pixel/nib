# nib — SPECIFICATION

*Rev 0.4 · 2026-09-03 · normative. Where this document and `docs/BLUEPRINT.md` disagree, this one
governs the built artefact and the blueprint governs the intent. Where either disagrees with
`docs/ASSEMBLY.md` on Etherpad or on the sync model, ASSEMBLY governs.*

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
removes one character, never one byte.

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
unconditionally.

2.3.6 The undo and redo stacks carry **opposite orderings**, and both are normative. An undo group
is stored in edit order and applied newest-first, because each inverse was computed against the
text its own edit produced (2.3.2) and they compose only in that order. A redo group is stored in
apply order and applied forwards. Reversing either half-restores a burst.

---

## 3 · The changeset library

### 3.1 Conformance **[BUILT]**

3.1.1 The port MUST accept and emit Etherpad's Easysync wire format without extension. A changeset
produced by nib and one produced by Etherpad for the same edit MUST be byte-identical.

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

4.3.3 Closing with unsaved changes MUST prompt. Discarding MUST be an explicit choice.

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

5.1.6 **Self-echo:** a delta whose lane is one of the resident's own seats MUST NOT be fed back as
input (`source.h`, spec §5.8). In a pad the resident writes into the buffer it reads, so this
filter MUST live in `PadSource` at the source, not downstream.

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
`[tick +Ns]`, byte-identical to `fusord.cpp:712`, and it is emitted **before** the percept that
broke the silence, as fusord does it.

5.1.9.1 Because the tick is produced here, at the source, the lifted resident loop MUST run with
its own `--idle-tick-s` set to 0 or the silence is counted twice. The pad is the right place for
it: the pad knows what a typing pause is and a generic resident does not.

5.1.10 A deletion reaches the trunk as the removed text, intact, behind a marker saying it left.
The marker is nib's and not the world's, so it is excluded from the byte-conservation arithmetic
of 5.1.11. **[OPEN]** — the marker's wording is off-distribution for v11 and is a decision, not a
finding; it is configuration until it has been looked at against a real trunk.

5.1.11 **The falsifier, stated as arithmetic.** With nothing pending, the bytes that entered the
compiler MUST equal the bytes that left it, and every percept MUST be either pushed to the ring or
counted as dropped: `typed_in == typed_out`, `removed_in == removed_out`, `pushed + dropped ==
percepts`. This is checkable after every keystroke and is fired from inside the running window by
`drive.py`'s `ingest` command, not only in unit tests.

---

## 6 · The resident

**[SPECIFIED]** — nothing in this section is built.

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

9.1 **Act I MUST link no network stack.** `build.bat` fails on `ws2_32`, `wininet`, `winhttp`,
`urlmon`, `dnsapi` among the dependents. **[BUILT]**

9.2 **[SPECIFIED]** Act II narrows rather than removes this gate: no HTTP client, no DNS resolver,
and a socket layer that refuses any destination outside the machine's own subnet by construction.

9.3 No frontier API in Act I or Act II. When one arrives it is behind a flag, and the room shows a
permanent badge.

9.4 The status line MUST carry an egress counter. In Act I it reads zero because that is
structurally true, not because nothing has been sent yet.

9.5 The resident MUST NOT be able to act outside the document: no shell, no file system beyond the
open document, no input synthesis, no window manipulation.

---

## 10 · Act II — the network

**[SPECIFIED]** — see `docs/ASSEMBLY.md` §3 for why OT and not CRDT.

10.1 Discovery: IPv4 only. A periodic UDP beacon and a peer table with a TTL. No IPv6, no mDNS
dependency, no configuration.

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

11.1 `--selftest` MUST pass before every commit. **[BUILT: 71 checks]**

11.2 Every stage below Stage 2 MUST remain runnable with no model in the process. A battery that
needs a 9B on a busy card is a battery that stops being run.

11.3 Property tests over random input are required where the space is large: ten thousand random
splices (canonical form and applied result), one thousand random edits interleaved with undos
(replay equality). Generators MUST be deterministic so a failure names a case to re-run.

11.4 The window MUST be verified by a driver that posts window messages and reads an artefact —
`WM_APP+1` commands and the `NIB_LOG` file. **Synthesising global input is forbidden**: it lands
wherever the focus happens to be, which can type into another application's window. **[BUILT]** —
`tools/drive.py`, 17 checks, 2026-09-04.

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

14.1 Whether `chars` must count code points rather than bytes for server interoperability. Affects
§2.2.3. Deferred until a real Etherpad server is on the other end.

14.2 Tab: four spaces, a real tab, or configurable. Currently spaces, unjustified.

14.3 The ingest compiler's N and T. To be measured against real typing, not chosen.

14.4 Whether the resident writes in the same buffer or an adjacent lane, once §6.3 has been used in
anger.

14.5 Where a room's identity comes from in Act II — a name, a key, or both.

14.6 Whether the tape should be encrypted at rest, as caseclock's is.
