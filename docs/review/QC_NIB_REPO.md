# QC report — C:\nib (nib 0.6.0, Stages 0a–1b)

*Audit performed 2026-09-04 by a reviewing session. Nothing inside `C:\nib`, `C:\auricle` or
`C:\etherpad-develop` was modified. The repository was copied (excluding `.git` and build
artefacts) to
`C:\Users\user\AppData\Local\Temp\claude\C--nib\fdcdbabd-d901-49c8-a923-83eb5319b86a\scratchpad\nibqc\`
and built there, so the operator's `nib.exe` was never touched. Every number below was measured on
this box today against that build. Every defect marked "reproduced" was demonstrated against the
built binary through nib's own driver seam (`WM_CHAR` / `WM_APP+1` / `NIB_LOG`) — no global input
was synthesised, no GPU work was run, and `--resident` was never invoked.*

> **Concurrency note, recorded because it affects what this report describes.** All source and
> documentation was read between 15:47 and 16:00, against commit `faecea0` with a clean working
> tree. Between 16:09 and 16:15 — while this report was being written — **another session wrote new
> untracked files into `C:\nib`**: `docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md`, `docs/review/`
> (three files) and `tools/etherpad_harness/` (five `.mjs` files plus `stubs/`, evidently the
> differential harness SPEC 3.3.4 calls for). I did not create them and have not read them.
> `git diff --stat HEAD` over `src/`, `build.bat`, `tools/drive.py` and every doc cited below is
> **empty** — no tracked file this report audits has changed — so every finding stands as written.
> But the tree is no longer the one described in §F.1, and a reader should re-run `git status`
> before acting on this report.

---

## A · Measured results, verbatim

### A.1 Build

Command (from Bash, absolute path — `cmd` does not search the cwd):

```
cd <scratch>/nibqc && cmd //c <scratch>/nibqc/build.bat
```

Full output, verbatim:

```
'vswhere.exe' is not recognized as an internal or external command,
operable program or batch file.
nib.cpp
changeset.cpp
doc.cpp
edit.cpp
ingest.cpp
resident.cpp
selftest.cpp
Generating Code...
OK: nib.exe  (gate: no network DLL)
```

Exit code **0**. **Zero warnings, zero errors** under `/W4 /WX /permissive- /std:c++20 /MT`.
The `vswhere.exe` line is emitted by Microsoft's own `vcvars64.bat` (it is not on PATH in this
shell), not by `build.bat`; the toolchain initialised correctly regardless.

### A.2 `nib.exe --selftest`

```
120 passed, 0 failed
```

Exit code 0. `grep -c "^  ok"` = **120**; `grep "FAIL"` returned **no lines at all**. Sample of
the tail, verbatim:

```
the resident's serve format - train equals serve, as a run that fails
  ok    the serve bytes hash to 0xe7ffa5704ba31076 and fusord pinned 0xe7ffa5704ba31076 on 2026-08-12
  ok    three seats (3)
  ok    named SPEAKER, SKEPTIC, SENTINEL, in that order - the order is part of the hash
  ok    an unstarted resident is inert, so the battery never needs a GPU (0 judgments)

refusals
  ok    a string that is not a changeset is refused
  ok    an invalid opcode is refused: invalid operation: !3
  ok    an opcode with no length is refused: invalid operation: =

120 passed, 0 failed
```

The battery ran with **no llama DLL touched and no GPU**, confirming SPEC 11.2 and the delay-load
design.

### A.3 `python C:/nib/tools/drive.py --exe <scratch build>` — three runs

| run | result | exit |
|---|---|---|
| 1 | `22 passed, 0 failed` | 0 |
| 2 | `22 passed, 0 failed` | 0 |
| 3 | `22 passed, 0 failed` | 0 |

**All three runs byte-identical.** MD5 of each captured transcript:
`54d46514bc9cdf3663c70cb858d2664c` (×3); `diff run1 run2` and `diff run2 run3` both empty.
Run 1, verbatim:

```
nib driver · <scratch>/nibqc/nib.exe

typing and saving
  ok    the save command reached the window and the log says so: ['saved', '24', '24']
  ok    the bytes are exactly what was typed: b'hello world\nsecond line\n'
  ok    the log's byte count matches the file

selection
  ok    one character typed over a selection replaced it: b'H\nsecond line\n'
  ok    and it cost ONE revision (24 -> 25)

undo and redo, by group
  ok    a burst of typing: b'HELLO\nsecond line\n'
  ok    ONE undo took back the whole burst, and the replaced selection with it: b'hello world\nsecond line\n'
  ok    and the log GREW rather than shrank — undo is appended (34)
  ok    and one redo put the whole burst back: b'HELLO\nsecond line\n'

backspace and delete
  ok    backspace then typing: b'bye\nsecond line\n'
  ok    delete forward: b'\nsecond line\n'

the log replays
  ok    the window folded 50 revisions and the text matched byte-exact

ingest
  ok    the window reports its ingest arithmetic: ['ingest', '13', '0', '32', '32', '13']
  ok    typing produced percepts (13)
  ok    every byte that entered the compiler left it: in 32 == out 32
  ok    and none were dropped on the way to the ring (0)
  ok    every percept reached the ring: 13 pushed of 13

close and reopen
  ok    the file survived the close unchanged
  ok    reopening loaded the saved file, and typing lands in it: b'X\nsecond line\n'

CRLF and BOM are preserved
  ok    the UTF-8 BOM came back: b'\xef\xbb\xbfZal'
  ok    CRLF stayed CRLF and the edit landed where it was typed: b'\xef\xbb\xbfZalpha\r\nbeta\r\n'

a new file
  ok    a path that did not exist was created on save: b'made from nothing\n'

22 passed, 0 failed
```

### A.4 `nib.exe --ingest C:/nib/README.md --counts`

```
3350 bytes -> 81 percepts (0 ticks) · longest 102 · N=160 T=500ms tick=30s
bytes in 3350 == out 3350  (nothing lost)
```

Exit code 0. This reproduces the devlog's 2026-09-04 figure exactly (81 percepts, longest 102).

### A.5 `dumpbin /dependents`

```
Image has the following dependencies:

    KERNEL32.dll
    USER32.dll
    GDI32.dll
    COMDLG32.dll

Image has the following delay load dependencies:

    llama.dll
    ggml.dll
    ggml-base.dll

  Summary

        3000 .data
        1000 .fptable
        3000 .pdata
       19000 .rdata
        1000 .reloc
       4F000 .text
```

Four hard dependencies, all OS. Three delay-loaded. **No `ws2_32`, `wininet`, `winhttp`,
`urlmon` or `dnsapi` in either list.** CLAUDE.md rule 2 and SPEC 9.1 hold as built.

### A.6 Executable size

| exe | bytes | KiB | md5 |
|---|---|---|---|
| `<scratch>/nibqc/nib.exe` (this audit's build) | **439 296** | 429 | `567d96d1888f66733d22c799bfa9ad5d` |
| `C:\nib\nib.exe` (operator's build, 14:53) | 439 296 | 429 | `c171adcb7b00a4bcd6462f7e6e74676a` |

Byte-for-byte identical **size**; the hashes differ only because MSVC embeds a build timestamp and
absolute PDB path. The committed source reproduces the shipped binary.

*(Note for the record: ROADMAP/HANDOFF do not state a size; the devlog's dated 350 KB / 391 KB
entries are historical and correctly dated.)*

### A.7 Four additional measurements taken during this audit

All four were run against the scratch build through the documented `WM_APP+1` / `NIB_LOG` seam
(scripts left in the scratch dir: `probe_utf8.py`, `probe_dpi_astral.py`, `probe_undo_ingest.py`).

```
probe 1 — vertical caret movement over a 2-byte character
  after typing        : b'ab\n\xc3\xa9\n'
  after Down + Back   : b'ab\n\xa9\n'
  is the file still valid UTF-8?  False   ('utf-8' codec can't decode byte 0xa9 in position 3)
  replay says byte-exact: ['replay', '1', '6']        <-- the falsifier still reports green

probe 2 — process DPI awareness
  nib process DPI awareness : PROCESS_DPI_UNAWARE  (hr=0x0)
  GetDpiForWindow(nib hwnd) : 96

probe 3 — an astral character delivered as Windows delivers it (two WM_CHARs, D83D DE00)
  bytes on disk             : b'\xef\xbf\xbd\xef\xbf\xbd'
  expected (UTF-8 U+1F600)  : b'\xf0\x9f\x98\x80'
  VERDICT                   : ASTRAL CHARACTER DESTROYED

probe 4 — does an undo reach the percept stream?
  after typing 26 chars   : percepts=1 dropped=0 typed_in=27 typed_out=27 pushed=1
  after ONE undo          : percepts=1 dropped=0 typed_in=27 typed_out=27 pushed=1
  document on disk now    : b''  (0 bytes)
  VERDICT: THE UNDO WAS NEVER PERCEIVED
```

Plus two CLI observations:

```
$ nib --check "Z:5>1=2+1=2$X"      # a changeset --selftest itself proves is NOT canonical
<prints the usage banner>
exit for non-canonical --check = 0

$ nib --unpack "Z:z>1|2=m=b*0|1+1$"
nib 0.1.0 - a writing surface with no send key on either side
...
```

---

## B · Findings, ranked by severity

Severity is judged against what nib says about itself: it is a **plain-text editor** whose central
law is that no text is ever lost, and a **research instrument** whose central law is that a wrong
answer must be a refusal rather than a number.

---

### CRITICAL

#### C-1 · Vertical caret movement and mouse hit-testing use byte offsets as if they were columns; a Down-arrow then a Backspace silently destroys a non-ASCII character

**Reproduced.** `C:\nib\src\doc.cpp:179-184` (`LineIndex::offset_of`), `C:\nib\src\edit.cpp:198-201`
(`col_of`), `C:\nib\src\edit.cpp:278-284` (`move_vertical`), `C:\nib\src\edit.cpp:302-310`
(`offset_at_point`).

`offset_of(line, col, text)` returns `start[line] + min(col, line_len)` — **pure byte arithmetic**.
`col_of(offset)` returns `offset - start[line]` — also bytes. But the painter derives the caret's
x-position from `widen(t.substr(...)).size() * g->cw`, i.e. **UTF-16 code units**
(`edit.cpp:514-515`, `edit.cpp:534-535`). The two coordinate systems agree only for ASCII.

Failing scenario, reproduced end to end:

```
document: "ab\né\n"          (é is C3 A9 — line 1 is 2 bytes, 1 glyph)
Ctrl+Home                    caret 0, want_col 0
Right                        caret 1, want_col 1
Down                         offset_of(1, 1) = start[1] + 1 = byte 4  <-- INSIDE the é
Backspace                    at = 3; text[3] = 0xC3 is a LEAD byte, not a continuation,
                             so the walk-back loop (edit.cpp:258) stops immediately and
                             one byte is removed
Ctrl+S                       file on disk = b'ab\n\xa9\n'  — invalid UTF-8
```

The character is gone and cannot be recovered by re-reading the file. Three aggravating facts:

1. **`Ctrl+R` still reports "byte-exact".** The op log faithfully records the corrupting splice, so
   Stage 0's headline falsifier is structurally blind to this entire class of defect. Probe 1 above
   shows `replay 1` on the corrupted document.
2. The same arithmetic drives **mouse click placement** (`offset_at_point`, `edit.cpp:302-310`) and
   **click-drag selection**, so an ordinary click on a line containing an accented character puts
   the caret in the wrong place *and* possibly inside a sequence.
3. It propagates: `sel_text()` (`edit.cpp:193-196`) can then hand `widen()` a partial sequence, so
   **Ctrl+C/Ctrl+V of such a selection substitutes U+FFFD** — a second, independent corruption.

`--selftest`'s random generators use only `a`–`z` and `\n` (`selftest.cpp:326-327`,
`selftest.cpp:492-493`), and the LineIndex section (`selftest.cpp:508-520`) uses ASCII only, so
nothing in 120 checks can see it.

**Suggested fix.** Introduce an explicit *display column* type distinct from the byte offset.
Minimum viable: give `LineIndex` a `col_to_offset` / `offset_to_col` pair that walks UTF-8
sequences (and counts East-Asian-wide glyphs as two columns to match the monospace cell), and route
`move_vertical`, `offset_at_point`, `col_of` and the painter's x-computation through it. Belt and
braces: make `Doc::splice` snap `start` and `start+ndel` to UTF-8 sequence boundaries and assert
that the resulting text is well-formed, so no code path can ever commit a split sequence. Add a
selftest that runs the existing 1,000-random-edit property test over a multi-byte alphabet and
asserts `utf8_valid(doc.text())` after every step — the validity walk already exists at
`selftest.cpp:627-643`.

---

#### C-2 · Any astral character (emoji, most CJK extension blocks, many symbols) typed into the editor is replaced by two U+FFFD

**Reproduced.** `C:\nib\src\edit.cpp:605-616`.

```cpp
case WM_CHAR: {
    const wchar_t c = (wchar_t)wp;
    ...
    const wchar_t w[2] = { c, 0 };
    char utf8[8]{};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, 1, utf8, sizeof utf8, nullptr, nullptr);
    if (n > 0) insert_text(h, std::string(utf8, (size_t)n));
```

Windows delivers a non-BMP character as **two consecutive `WM_CHAR` messages**, one per UTF-16
surrogate. Each is converted in isolation; `WideCharToMultiByte` with `CP_UTF8` and no
`WC_ERR_INVALID_CHARS` replaces a lone surrogate with U+FFFD. Probe 3 above shows U+1F600 landing on
disk as `EF BF BD EF BF BD`.

This is data destruction on a completely ordinary input path (emoji picker, IME, Alt+X, any
paste-by-keyboard). It is not covered by any check.

**Suggested fix.** Buffer a high surrogate in the `View` and combine it with the following low
surrogate before converting; discard an unpaired surrogate rather than emitting U+FFFD. ~10 lines.
Add a selftest or driver check that posts `D83D DE00` and asserts `F0 9F 98 80` on disk.

---

### HIGH

#### H-1 · `--check`, `--unpack` and `--ops` are advertised everywhere but dispatched nowhere; `nib --check <invalid changeset>` exits 0

**Reproduced.** `C:\nib\src\nib.cpp:22-26` (usage text), `nib.cpp:35-54` (`do_unpack`, `do_ops`
defined), `nib.cpp:203-228` (`main` — dispatches only `--selftest`, `--edit`, `--ingest`,
`--resident`, `--splice`, `--apply`).

`do_unpack` and `do_ops` are **dead code**: nothing calls them. `--check` has no implementation at
all — `check_rep` is never reachable from the CLI. All three fall through to the usage banner and
`return 0`. A script or a person using `nib --check CS` as a validator gets **exit 0 on a changeset
`--selftest` itself proves is non-canonical** (A.7 above). That is a validator that always says
"fine".

These three commands are documented as working in `nib.cpp:23-26`, `HANDOFF.md:184` and are
implied by SPEC 3.2.

MSVC's `/W4 /WX` did not catch the two unreferenced functions because C4505 does not fire for
anonymous-namespace functions.

**Suggested fix.** Add the three dispatch lines to `main`, implement `--check` over `check_rep`
returning 2 on failure, and add a driver/selftest check that `--check` on each of the four negative
vectors already in `selftest.cpp:171-188` exits non-zero.

---

#### H-2 · Undo, redo and file-open bypass ingest entirely: the resident's world diverges from the document, and the stated falsifier cannot see it

**Reproduced.** `C:\nib\src\edit.cpp:239-243` is the only place `PadSource` is fed, and it lives
inside `edit_splice` (`edit.cpp:223-245`). `Doc::undo` / `Doc::redo` are invoked at `edit.cpp:653-667` and
`edit.cpp:689-698` and reach `after_edit(h, false)` **without touching `g->ingest`**.
`load_into` (`edit.cpp:393-412`) likewise replaces the whole document with no percept.

Probe 4: 26 characters typed, then one undo emptied the document — and the compiler's counters did
not move at all. The resident's model of the world still contains text the document no longer has.

This violates CLAUDE.md rule 7 ("percepts are never dropped, and deletions are percepts") and
SPEC 5.1.3 / 5.1.5 (the world is never edited) at the point where they matter most: a person
backspacing a sentence is exactly the case those rules were written for, and Ctrl+Z is how people
actually do it.

The conservation identity in SPEC 5.1.11 (`typed_in == typed_out`, `pushed + dropped == percepts`)
**still reads green through this hole**, because it audits the compiler's internal bookkeeping, not
the feed. That is worth saying plainly in the spec: the falsifier proves the compiler loses nothing
it is *given*; it says nothing about what it is given.

Secondary case: `Ctrl+O` swaps in a different document while `Compiler::pending_` still holds the
previous document's half-clause, which will then be fused with the new document's first words into
one bracketed line.

**Suggested fix.** Route undo/redo through the same funnel: have `Doc::undo`/`Doc::redo` report the
`(start, ndel, ins)` they effected (or have the editor diff before/after) and feed
`ingest->removed(...)` / `ingest->typed(...)` accordingly. On `load_into`, call `ingest->flush()`
and emit a document-change marker. Then strengthen the falsifier: assert that the concatenation of
percepts reconstructs the *document's* edit history, not merely the compiler's own inputs.

---

#### H-3 · The resident goes deaf when the context window fills — silently — and then keeps recording judgments as if it had perceived

`C:\nib\src\resident.cpp:313` and `C:\nib\src\resident.cpp:339`:

```cpp
if (npast_ + (long long)wt.size() >= cfg_.n_ctx - 512) return;   // Stage 1b does not molt yet
```

Both are bare `return`s. Nothing is counted, printed, flagged, or surfaced. With `n_ctx` 8192 and
roughly 400 tokens of seed, an ordinary writing session reaches this in a few thousand words and
the mind stops perceiving with **no signal of any kind**.

Worse than deafness: `Resident::feed` appends to `clause_` unconditionally
(`resident.cpp:352`) whether or not `ingest_word` actually decoded, and then
`if (!clause_.empty()) judge("f", 0.0f, out);` (`resident.cpp:357`) probes all three seats on a
clause **the trunk never saw**, at a frozen `npast_`. The judgments are recorded with the clause
text attached, so the corpus contains rows asserting "these three seats judged this clause" when
they judged a stale context. This is precisely the failure mode ROADMAP/devlog say the CPU-fallback
lesson generalises to: *a wrong answer that looks like a right one.*

`words()` under-counts (the early return precedes `++words_` at `resident.cpp:316`), which is the
only visible symptom, and only if you separately count the input.

Related, same lines: `cfg_.n_ctx - 512` is `int` arithmetic. `nib --resident F --ctx 256` makes the
bound `-256`, so the condition is always true, nothing is ever ingested, and the program prints
`0 words` with no error (`nib.cpp:124` accepts any `atoi` result unvalidated).

**Suggested fix.** Until the molt lands, make the full window a **refusal**, not a return: count
`words_dropped_`, set a flag, print `WINDOW FULL — N words unperceived` in the summary, and either
stop the run or refuse to `judge` on a clause whose words were not decoded. Clamp `--ctx` to a sane
minimum (e.g. ≥ 2048) and reject anything smaller with a message.

---

#### H-4 · `Doc::apply` leaves the existing undo stack in place; a subsequent Ctrl+Z can silently corrupt the document

`C:\nib\src\doc.cpp:89-100`. `apply` sets `group_open_ = false` (line 98) but does **not** touch
`undo_`. Every inverse already sitting in `undo_` was computed against a text that `apply` has now
changed.

Failing scenario: the person types a burst (one undo group, inverses computed against text *T*).
The resident applies a changeset that **replaces** some text with text of the same length (a
correction, a re-word — the most likely Stage-2 emission after floor control). `text_.size()` is
unchanged, so `apply_to_text`'s length guard (`changeset.cpp:146-149`) passes, the person's inverse
applies **at offsets that no longer mean what they meant**, and the document is silently mangled.
If the length differs, the failure is louder but still bad — see H-5.

`Doc::apply` has **zero test coverage**: no line of `selftest.cpp` calls it, and `drive.py` cannot
reach it. It is the exact function Stage 2 is about to depend on.

**Suggested fix.** On `apply`, either (a) clear `undo_`/`redo_` (simplest, honest: a foreign change
ends what you could take back), or (b) transform the pending inverses through the incoming
changeset with `follow` — which is not ported (SPEC 3.3.2) and would be the first thing Act II
needs anyway. Whichever is chosen, say it in SPEC 2.3.5 and cover it with tests before Stage 2
writes a single character.

---

#### H-5 · A failed undo or redo leaves the document half-changed, with the group already popped — the text is then unrecoverable

`C:\nib\src\doc.cpp:102-122` and `doc.cpp:124-143`.

```cpp
const std::vector<std::string> group = undo_.back();
undo_.pop_back();                                   // popped BEFORE anything is applied
...
for (size_t i = group.size(); i-- > 0;) {
    if (!apply_to_text(group[i], text_, after, err)) return false;   // partial state, group gone
    if (!push(group[i], redo_cs, "undo", err)) return false;         // ditto
}
```

Any failure part-way through a multi-inverse group leaves earlier inverses applied, the group
removed from `undo_`, and nothing pushed to `redo_`. There is no rollback. Combined with H-4 this is
reachable from Stage 2; on its own it is reachable from a corrupt log.

**Suggested fix.** Validate the whole group against a scratch copy first (or snapshot `text_`,
`log_.size()`, and restore on failure), and only pop `undo_` once the group has been applied
successfully.

---

### MEDIUM

#### M-1 · The window is `PROCESS_DPI_UNAWARE`; SPEC 4.1.2 [BUILT] is false and the `WM_DPICHANGED` handler is dead code

**Reproduced** (probe 2): `GetProcessDpiAwareness` on the running nib process returns
`PROCESS_DPI_UNAWARE`; `GetDpiForWindow` returns 96.

There is **no** manifest, no `.rc`, no `/MANIFEST*` linker option, and no
`SetProcessDpiAwarenessContext` / `SetProcessDpiAwareness` call anywhere in `src/` or `build.bat`
(grep confirms zero hits). Windows therefore virtualises the process at 96 DPI, bitmap-upscales the
window, and **never sends `WM_DPICHANGED`** — so `edit.cpp:592-599` can never run and
`edit.cpp:571` always reads 96.

On the operator's 225 % box, the editor is a blurry upscaled 96-DPI window — the exact outcome
`edit.cpp:1-6` says must not happen ("a text editor that is blurry on the second screen is a text
editor nobody uses on the second screen").

**Suggested fix.** Call `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`
at the top of `run_editor` (Windows 10 1703+, `user32`, no manifest needed), or add an embedded
manifest. Then add a check to the driver: launch, call `GetProcessDpiAwareness`, assert
per-monitor — a two-line addition to `drive.py`, and the only way this stays true.

#### M-2 · `sentence_cut` splits on abbreviations, contradicting its own comment; and never closes a thought ending in a quote or bracket

`C:\nib\src\ingest.cpp:28-45`. Comment at `ingest.cpp:31`: *"'3.14' and 'e.g. ' must not split a
thought in half."* Only the first is true. Reproduced with `--ingest`:

```
[bo] Use a tool, e.g.
[bo] a hammer, when you must.
[bo] He said "stop." Then he left.
[bo] See Fig.
[bo] 3 for details.
[bo] Mr.
[bo] Smith arrived (late.) Nobody cared.
```

`e.g. `, `Fig. ` and `Mr. ` all split (the terminator is followed by a space, which is the only test
at `ingest.cpp:41`). Conversely `stop."` and `(late.)` do **not** close, because the character after
the terminator cluster is `"` or `)` rather than whitespace — so the percept runs on past the real
sentence end. SPEC 5.1.2.1 says a closed thought is "`.`, `!`, `?` **at a word end**", and `."` is a
word end.

Both directions cost the thing fusord's boundary set was tightened for: spurious boundaries cost
three probes each, and missed boundaries hand the seats over-long clauses.

**Suggested fix.** After the terminator cluster, skip a run of closing punctuation (`"` `'` `)` `]`
`}` `»`) before testing for whitespace; and suppress a split when the terminator is preceded by a
short capitalised or single-letter token (a small abbreviation list, or the heuristic "≤ 3
alphanumerics since the last space, and no vowel-space after"). Either way, fix the comment to match
whatever ships, and add the four cases above as selftest vectors — the current `ingest - where a
percept ends` section (`selftest.cpp:543-573`) tests `3.14` but not `e.g.`.

#### M-3 · A lane change or a deletion swallows the idle tick, so silence stops entering as world exactly when the world changed hands

`C:\nib\src\ingest.cpp:138-139` and `ingest.cpp:150-151`:

```cpp
if (!pending_.empty() && lane != pending_lane_) push_pending(now_ms, out);   // sets last_percept_ms_
maybe_tick(now_ms, out);                                                     // gap is now 0
```

`push_pending` → `emit` → `last_percept_ms_ = now_ms` (`ingest.cpp:74`). `maybe_tick` then computes
`gap = now_ms - last_percept_ms_ = 0` and returns without emitting (`ingest.cpp:82-83`). So after a
long silence, if the first thing that arrives is on a different lane, or is a deletion, **the tick
is lost**. SPEC 5.1.9 says the tick "is emitted **before** the percept that broke the silence"; here
the flush of the *previous* percept eats it.

**Suggested fix.** Compute the gap from `last_input_ms_` (or capture `last_percept_ms_` before the
flush) and emit the tick first, then the flush, then the new text — which is also fusord's order
(`fusord.cpp:709-726`: tick, then the lane prefix, then the words).

#### M-4 · The tick reaches the trunk framed differently from fusord's, and unlike fusord's it triggers three probes

`C:\nib\src\ingest.cpp:84-95` produces the percept text `[tick +45s]` with a lane, so
`Resident::feed` (`resident.cpp:338`) prepends `\n[bo] ` and the trunk sees:

```
nib:     \n[bo] [tick +45s]
fusord:  \n[tick +45s]          (fusord.cpp:712-715 — a bare line, no lane bracket)
```

Two consequences. (a) SPEC 5.1.9 and `ingest.cpp:80-81` claim byte-identity with `fusord.cpp:712`;
that is true of the 11-byte tick string only after dropping fusord's leading `\n`, and is not true of
the trunk stream. (b) fusord decodes the tick and calls `read_frontier()` and **nothing else** — it
never judges on a tick. In nib the tick is an ordinary percept, so `feed` ends with
`if (!clause_.empty()) judge("f", ...)` (`resident.cpp:357`) and **three seats are probed on every
idle tick**, at ~250 ms a time, on a clause consisting of the tick text. That is both a cost and a
train ≢ serve divergence in the one place the project treats byte-identity as load-bearing.

**Suggested fix.** Carry `Percept::kind` through to the resident and handle `'t'` the way fusord
does — decode the bare `\n[tick +Ns]` onto the trunk, read the frontier, do not accumulate it into
`clause_`, do not judge. Then the claim in SPEC 5.1.9 becomes true of the stream and not only of the
string.

#### M-5 · A lone-CR file, and a mixed CRLF/LF file, are silently converted — SPEC 4.3.2 says they must not be

`C:\nib\src\edit.cpp:396-402`:

```cpp
g->crlf = raw.find("\r\n") != std::string::npos;      // ANY CRLF makes the whole file CRLF
...
lf += raw[i] == '\r' ? '\n' : raw[i];                 // a lone CR becomes LF, unconditionally
```

A classic-Mac (CR-only) file is rewritten with LF or CRLF on save. A file with mixed endings (common
in patched configs, concatenated logs) is rewritten **entirely** as CRLF. Neither is announced; the
status line says `opened · CRLF`. SPEC 4.3.2: *"nib MUST NOT silently convert a file's
conventions."*

Related, same path: `read_all` (`edit.cpp:351-367`) reads any file as bytes with no validation, so
opening a UTF-16 or binary file loads mojibake and saving writes it back mangled.

**Suggested fix.** Record the *dominant* convention plus a `mixed` flag; when mixed, say so on the
status line and preserve per-line endings (or refuse to save without an explicit choice). Detect a
CR-only file as its own convention and restore it. Detect a UTF-16 BOM and refuse rather than
mangle.

#### M-6 · Unsaved work is lost on Windows shutdown or logoff

`C:\nib\src\edit.cpp:568-775`: the window procedure handles `WM_CLOSE` (→ `ok_to_discard`) but has
**no `WM_QUERYENDSESSION` / `WM_ENDSESSION` case** (grep confirms zero hits). At shutdown or logoff
Windows sends `WM_QUERYENDSESSION`; `DefWindowProc` returns TRUE and the process is terminated with
the document unsaved and no prompt. SPEC 4.3.3 ("Closing with unsaved changes MUST prompt") is
satisfied only for the close button.

**Suggested fix.** Handle `WM_QUERYENDSESSION`: if dirty, call `ShutdownBlockReasonCreate` and
return FALSE (or prompt). Clear the block on `WM_ENDSESSION`.

#### M-7 · `Doc::apply`'s whole-document inverse, and `replay()`'s O(n²) fold, do not survive Stage 5

`C:\nib\src\doc.cpp:97`: `log_.back().inverse = make_splice(text_, 0, (int64_t)text_.size(), before);`
— every foreign changeset costs a changeset whose char-bank is **the entire previous document** and
whose ops delete the entire new one. At Stage 2's expected rate (an emission every minute or two
into a document that grows all week), the log grows by ~2× the document size per emission. `undo`
and `redo` do the same at `doc.cpp:114` and `doc.cpp:137`.

`Doc::replay` (`doc.cpp:145-156`) copies the whole document once per revision. After the 965
revisions the ROADMAP already reports on a small document that is fine; on a 200 KB week-long
document with a few thousand revisions, `Ctrl+R` blocks the UI thread for seconds. The comment at
`doc.cpp:92-94` says "cheap enough at this size" — which is honest, and the size is about to change.

**Suggested fix.** Compute the true inverse of an arbitrary changeset op-by-op (walk the ops
against `before`, emitting `-` for each `+` and `+` (from `before`) for each `-`) — ~40 lines, and
it is also what a tape reader will want. For `replay`, keep a periodic checkpoint (text + revision
index) so `Ctrl+R` folds only the tail, and say so in SPEC 2.1.3.

#### M-8 · The self-echo seat is `"watcher"`, which is not the name of any seat the resident has

`C:\nib\src\edit.cpp:576`: `g->ingest->add_seat("watcher");`
`C:\nib\src\resident.cpp:38-45`: the seats are `SPEAKER`, `SKEPTIC`, `SENTINEL`.
`fusord.cpp:706-707` filters on `MINDS[m].name` (the three seat names) **and** the literal `"fusor"`.

`"watcher"` is the *reply prefix* inside the probe frame (`resident.cpp:61`), not a lane. When
Stage 2 writes into the buffer it will write on some lane; unless that lane is literally `watcher`,
SPEC 5.1.6's filter will miss and the nucleus will deliberate about interrupting itself — the exact
failure `source.h:16-17` warns about. Today the filter is unreachable anyway because
`edit_splice` hard-codes lane `"bo"` (`edit.cpp:241-242`).

**Suggested fix.** Seed the seats from `nib::seats()` (plus `"fusor"`, as fusord does) rather than a
string literal, and add a check that every name in `seats()` is filtered.

#### M-9 · `nib.cpp` uses ANSI `main(int, char**)`, so non-ASCII arguments and file paths are mangled or replaced with `?`

`C:\nib\src\nib.cpp:203`. Windows converts `argv` through the ANSI code page. Observed:
`nib --splice "" 0 0 "é"` produced `Z:0>1+1$<single byte>` (one CP-1252 byte, not two UTF-8 bytes),
and `"😀"` arrived as the two ASCII bytes `??`.

Concretely: `run_editor(argv[2])` (`nib.cpp:207`) passes the path to `widen()`
(`edit.cpp:799`), which decodes it **as UTF-8** — so any path containing a non-ASCII character
either fails to open or opens the wrong file, and `--ingest`/`--resident` on such a path report
"cannot open".

**Suggested fix.** Use `wmain(int, wchar_t**)` (or `GetCommandLineW` + `CommandLineToArgvW`) and
convert to UTF-8 internally.

#### M-10 · A save failure never reports *why*, and the temp file is not unique

`C:\nib\src\edit.cpp:371-391`. Three failure paths (`"cannot create the temporary file"`,
`"the write failed"`, `"the replace failed"`) discard `GetLastError()`. A read-only target — where
`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` fails with `ERROR_ACCESS_DENIED` — is
indistinguishable from a full disk or a lost network share. The user is told only "save failed: the
replace failed".

The atomicity itself is sound: the temp is created in the target's own directory
(`path + L".nib-tmp"`), so a cross-volume rename is impossible; `FlushFileBuffers` precedes the
rename; `MOVEFILE_WRITE_THROUGH` is correct; the temp is deleted on both failure paths; the previous
file survives every failure. Two lesser points: the temp name is fixed, so two nib instances saving
the same file collide (the second fails cleanly, because `dwShareMode` is 0 — acceptable, but a
`.nib-tmp` orphan survives a crash), and `MoveFileEx` replaces the file rather than its contents, so
the target's **ACLs, alternate data streams, creation time and attributes are silently reset** —
a form of "silently changing somebody's file" that SPEC 4.3.2 is otherwise careful about.

**Suggested fix.** Include `GetLastError()` in the message. Use `ReplaceFileW` (which preserves the
target's ACL/attributes/streams) with a `MoveFileEx` fallback. Add the process id to the temp name.

#### M-11 · `--resident` results are not reproducible, by construction

`C:\nib\src\resident.cpp:329`: the flush law fires on `wall_ms()` (a real steady clock), while
`Resident::feed`'s `wall_ms` parameter — the percept's own time — is **unnamed and discarded**
(`resident.cpp:334`). `nib.cpp:163-169` feeds a synthetic clock into the compiler and then ignores
it at the resident.

This is the same mechanism the devlog identifies as having corrupted the record during the CPU
fallback (30 boundaries instead of 18). It is still present for GPU runs: the same input file on a
busier card produces a different number of `"t"` boundaries and therefore a different judgment
corpus. The Stage 1b margin table in ROADMAP is not reproducible from `tests/margins.txt` by
construction.

**Suggested fix.** Use the percept's `wall_ms` for the flush law (that is what the parameter is
for), so an offline run over a file is deterministic; keep the real clock only for the live path.
Add a `--deterministic` flag that pins it, and re-take the Stage 1b table under it.

#### M-12 · `dec()`'s return value is discarded at every call site; a failed decode is recorded as a judgment

`C:\nib\src\resident.cpp:285`, `:314`, `:340`. `dec` returns `false` when `llama_decode` fails
(`resident.cpp:106-107`), and every caller ignores it. Execution then falls through to
`llama_get_logits_ith(p_->ctx, -1)` (`resident.cpp:260`, `:286`) — which reads **stale logits from
the previous decode**, or returns `nullptr`, which is dereferenced immediately at
`resident.cpp:290` (`l[p_->emit_tok]`).

The result is either a null dereference or, worse, a plausible-looking margin computed from another
clause's frontier, written into the corpus with this clause's text attached. (fusord has the same
shape at `fusord.cpp:466`; nib inherits it. But fusord prints to a console a human is watching,
whereas nib is building a record.)

**Suggested fix.** Propagate the failure: `if (!dec(...)) { ++decode_failures_; return; }`, null-check
`llama_get_logits_ith`, and surface the count in the summary. A judgment that could not be computed
must be absent, not approximated.

---

### LOW

#### L-1 · base-36 parsing overflows int64 as signed UB on a long number

`C:\nib\src\changeset.cpp:48-58`: `v = v * 36 + d` on `int64_t`, unbounded. 13 base-36 digits
already exceed 2^63. Observed: `nib --apply 'Z:zzzzzzzzzzzzzz>1=1+1$X' abcde` →
`mismatched apply: 5 / -1823562080465190913`. The refusal is correct, but the overflow itself is
undefined behaviour, and the negative value then flows into `check_rep`'s arithmetic
(`changeset.cpp:277`, `:289`, `:298-300`), where the `=` branch has no bound check at all (faithfully
mirroring Etherpad, see C-3 in section C).

Today this is only reachable from a hand-typed CLI argument. **The day Act II accepts a changeset
off the wire, it is an attacker-controlled integer overflow in the parser.**

**Suggested fix.** Parse into `uint64_t` with a saturating cap (e.g. refuse anything above
`1<<40`), and return failure rather than a value. Add negative-handling to `parse_num` so that
`parse_num(num_to_string(n)) == n` holds for all `n` (today `parse_num("-5")` returns 0).

#### L-2 · `Doc::set` is a third mutator, and it bypasses validation

`C:\nib\src\doc.cpp:11-29` assigns `text_` directly and pushes a `Rev` without going through
`push()`, so the opening changeset is never `check_rep`'d and never `apply_to_text`'d. SPEC 2.1.2's
parenthetical names only `splice` and `apply`. The changeset is canonical by construction today, so
this is a latent asymmetry rather than a live bug. The `err` local at `doc.cpp:24` is written to
nothing and `(void)err`'d at line 27; the stored inverse at line 26 can never be used (`undo_` is
empty after `set`).

#### L-3 · A pasted block fuses into the following typing as one undo group

`C:\nib\src\doc.cpp:78`: `group_open_ = ins.find('\n') == std::string::npos;`. A paste with no
newline leaves the group open, so Ctrl+V followed by typing within 700 ms is a single Ctrl+Z. Most
editors treat a paste as its own atom. Also, backspace and delete-forward at the same offset both
satisfy the contiguity test (`doc.cpp:68`) and fuse, though they are opposite motions.

#### L-4 · `Ctrl+C` / `Ctrl+V` fail silently when the clipboard is busy

`C:\nib\src\edit.cpp:313-348`: `if (!OpenClipboard(h)) return;` in both, with no status message.
Another application holding the clipboard makes copy and paste do nothing, visibly indistinguishable
from working. Retry briefly and set a status on failure.

#### L-5 · `PadSource`'s counters are non-atomic and will race the moment a resident thread exists

`C:\nib\src\ingest.h:178`: `uint64_t pushed_ = 0, dropped_ = 0, echoes_ = 0, trunc_lanes_ = 0;` —
plain, and `scratch_` (line 177) is a shared member vector reused by all four producer entry points.
Today the producer and the status-line reader are the same thread, so this is correct. The header's
own contract ("Producer side (the editor thread)… Consumer side (the resident thread)") means the
first thing Stage 2 does breaks it. `SpscRing::size()` is fine (`C:\auricle\src\core\ring.h:59-63`,
acquire loads). Make the four counters `std::atomic<uint64_t>` with relaxed ordering, as
`FileTailSource::dropped_` already is (`source.h:174`).

#### L-6 · `truncated_lanes()` is counted but never surfaced

`C:\nib\src\ingest.cpp:197` increments it; nothing in `edit.cpp`'s status line
(`edit.cpp:552-556`), `nib --ingest`'s summary (`nib.cpp:103-109`) or `drive.py` ever reads it. Rule
7 says a loss is counted *loudly*; this one is counted quietly. Same for `echoes()`.

#### L-7 · `NIB_LOG` is taken from the environment and opened unvalidated

`C:\nib\src\edit.cpp:783`: `if (const char* lp = getenv("NIB_LOG")) g->log = fopen(lp, "ab");` —
no path validation, no failure report, appended to for the life of the process. It is the user's own
environment so this is not a privilege boundary, and only counters (never document text) are
written, so there is no encoding hazard. Worth one line: report when the open fails, so a driver
that mis-spells the path fails loudly instead of hanging on `wait_for`.

#### L-8 · A pathological run of continuation bytes can stall `Compiler::drain`

`C:\nib\src\ingest.cpp:117-127`: if `utf8_safe_cut` returns 0 (≥ 495 leading continuation bytes) and
`last_word_cut` returns 0, `drain` breaks and `pending_` never shrinks until the next `flush()`
(which does have the `take = 1` fallback at `ingest.cpp:61`). Bounded and self-healing, but
`pending_` grows unboundedly in the meantime.

#### L-9 · Dead state in the resident

`C:\nib\src\resident.h:116-117` (`logZ_`, `have_logZ_`) is computed at `resident.cpp:269-270` and
never read — fusord uses it for the surprisal "nerve" (`fusord.cpp:657-659`), which nib did not lift.
Either lift the nerve or drop the fields. `Judgment::wall_ms` is set from the probe's clock, not the
percept's (`resident.cpp:288`), which will matter for SPEC 8.1.2's tape.

#### L-10 · Unchecked `MultiByteToWideChar` into `MAX_PATH` buffers in the DLL loader

`C:\nib\src\resident.cpp:151-159`: two conversions into `wchar_t[MAX_PATH]` with the return value
discarded. A `--llama-dir` longer than 259 characters leaves the buffer un-terminated and
`SetDllDirectoryW` / `LoadLibraryExW` read past it. Also `SetDllDirectoryW` is never reset, which
permanently removes the current directory from the process's DLL search path — documented behaviour,
worth a comment.

---

### NIT

- **N-1** `C:\nib\src\doc.cpp:163`: `if (text[i] == '\n' && i + 1 <= text.size())` — the second
  condition is always true for `i < text.size()`. Dead.
- **N-2** `C:\nib\src\changeset.cpp:34`: `(uint64_t)(-n)` is signed-overflow UB for `INT64_MIN`.
  Use `0u - (uint64_t)n`.
- **N-3** `C:\nib\src\changeset.cpp:10-13`: `fail()` is defined and never used.
- **N-4** `deserialize_ops`' error message is `"invalid operation: " + ops.substr(start)` where
  `start` is the start of the whole op; Etherpad's is `ops.slice(regex.lastIndex - 1)`, i.e. from the
  offending character. Cosmetic divergence only.
- **N-5** `C:\nib\.gitignore:9` ignores `fray.ini` — a leftover from a different tool in the estate.
- **N-6** `C:\nib\tools\theme_detect.py:40` hard-codes `--out` default to `C:/nib/nib.theme`, so
  running it from anywhere writes into the repository.
- **N-7** `C:\nib\src\edit.cpp:102`: the trailing-whitespace strip in `load_theme` removes LF, CR and
  space but not TAB, so a theme line ending in a tab fails `parse_hex`.
- **N-8** `SmartAssembler` has no `clear()` (Etherpad's does, `SmartOpAssembler.ts:102-108`), so
  `out_` and `length_change_` never reset. Not reachable — every instance is used once — but it
  makes the class non-reusable in a way the JS original is not.
- **N-9** `WM_TIMER` id 1 is never `KillTimer`'d on `WM_DESTROY`. Harmless.

---

## C · Port-fidelity table — `changeset.cpp` vs Etherpad

Verdict summary: **the port is unusually faithful.** Every ordering rule, every assertion, and every
canonical-form decision matches. The divergences below are (a) two places where nib is stricter than
Etherpad, (b) one place where nib is looser, (c) benign representational differences, and (d) the
byte-vs-UTF-16 issue, which is systemic and is treated separately in C.10.

| # | function | nib | Etherpad | verdict |
|---|---|---|---|---|
| C.1 | `num_to_string` | `changeset.cpp:31-46` | `ChangesetUtils.ts:74` | **Match** for all reachable values. Diverges above 2^53 (nib exact, JS lossy) and is UB above 2^63 — see L-1. Negative sign emitted identically. |
| C.2 | `parse_num` | `changeset.cpp:48-58` | `ChangesetUtils.ts:66` (`parseInt(s,36)`) | **Match** on well-formed input (both stop at the first non-radix character). Diverges on `""` (nib 0, JS NaN) and on a leading `-` (nib 0, JS negative). Not reachable from the scanner. |
| C.3 | `Op::str` | `changeset.cpp:61-68` | `Op.ts:72-77` | **Match** byte for byte. nib returns `""` for a null opcode where JS throws `TypeError('null op')`; unreachable in both (`MergingAssembler::flush` guards, `changeset.cpp:186`). |
| C.4 | `deserialize_ops` — attribs, `\|lines`, opcode, chars | `changeset.cpp:75-109` | `Changeset.ts:118-132`, regex `/((?:\*[0-9a-z]+)*)(?:\|([0-9a-z]+))?([-+=])([0-9a-z]+)\|(.)/g` | **Match** token for token: `*`+≥1 base36 repeated; optional `\|`+≥1 base36; one of `-+=`; ≥1 base36. `lines` defaults to 0 as `parseNum(match[2] \|\| '0')` does. |
| C.5 | `deserialize_ops` — the `(.)` error branch | `changeset.cpp:85, 92, 96, 99, 104` | `Changeset.ts:123-124` | **Two divergences, both narrow.** (a) JS `.` does not match `\n`, so a newline inside the ops string is *silently skipped* by Etherpad; nib refuses it (`invalid operation:`). Verified: `nib --apply 'Z:5>1=2\n+1$X' abcde` → refused. nib is stricter, and both end up rejecting via canonical form. (b) `'$'` is accepted by nib **after** attribs or `\|lines` have been consumed (`changeset.cpp:98` is reached with `op.attribs`/`op.lines` already set), where Etherpad's regex fails the first alternative and reports `invalid operation`. So `deserialize_ops("*0$")` returns success in nib and errors in Etherpad. Unreachable through `unpack`, which splits at the first `$` (`changeset.cpp:128`). |
| C.6 | `unpack` header regex | `changeset.cpp:112-134` | `Changeset.ts:360-377`, `/Z:([0-9a-z]+)([><])([0-9a-z]+)\|/` | **Match.** The JS regex is unanchored but its empty alternative always matches at index 0, so a non-`Z:` string yields `headerMatch[0] === ''` and errors — identical to nib's `cs[0]!='Z'` test. `opsStart`, `opsEnd = indexOf('$')` (or length), and `charBank = substring(opsEnd+1)` all match, including the no-`$` case. nib's extra `cs.size() < 5` guard is safe (the shortest legal header is `Z:0>0`). |
| C.7 | `pack` | `changeset.cpp:136-140` | `Changeset.ts:388-395` | **Match.** |
| C.8 | `apply_to_text` | `changeset.cpp:143-182` | `Changeset.ts:404-440` | **Match** on the newline-count assertions for `+`, `-`, `=`, and on the implicit trailing keep (`changeset.cpp:180` ≡ `Changeset.ts:438`). nib adds two bound checks Etherpad gets from `StringIterator` asserts (`changeset.cpp:159`, `:165`, `:170`) and a `default:` that rejects an unknown opcode where JS's switch silently ignores it. Both make nib stricter, never looser. |
| C.9 | `check_rep` — assertions and their ordering | `changeset.cpp:260-308` | `Changeset.ts:245-291` | **Exact match, including the gaps.** `=` → `oldPos += chars; calcNewLen += chars` with **no bound assertion** (faithfully reproduced, `changeset.cpp:271-274`). `-` → `oldPos += chars` then `oldPos <= oldLen`. `+` → bank-length, then newline count, then `endsWith('\n')`, then `calcNewLen <= newLen` — in that order (`changeset.cpp:280-289` ≡ `Changeset.ts:267-276`). Then `calcNewLen += oldLen - oldPos`, `calcNewLen === newLen`, `charBank === ''`, `endDocument()`, re-`pack` with the **original** bank, byte compare. All present, all in order. The `chars==0 && lines!=0` case that would index `char_bank[bank-1]` out of range at `changeset.cpp:283` is unreachable, because the newline-count check fires first — same order as JS. |
| C.10 | `MergingAssembler` / `bufOpAdditionalCharsAfterNewline` | `changeset.cpp:185-221`, `extra_` (`changeset.h:86`) | `MergingOpAssembler.ts:23-58` | **Semantic match.** The three-way `append` (multiline fuses everything buffered; both in-line fuses chars; in-line after multiline accumulates into `extra_`) is line-for-line. `flush(isEndDocument)` drops a final attribute-less bare keep and otherwise emits `bufOp` followed by the buffered in-line tail. Representational difference only: JS mutates `bufOp` in place to emit the tail, nib copies (`changeset.cpp:195-198`); the emitted string and the post-state are identical. JS's `clear()` does **not** reset `bufOpAdditionalCharsAfterNewline` while nib's does — provably immaterial, since the counter is non-zero only while `bufOp.opcode` is non-empty and `flush` therefore always zeroes it. Verified by `selftest.cpp:214-228` (`[xxx\n, yyy, zzz\n] → \|2+b`, `[xxx\n, yyy] → \|1+4+3`). |
| C.11 | `SmartAssembler` flush ordering | `changeset.cpp:223-257` | `SmartOpAssembler.ts:43-78, 96-100` | **Match.** `flushPlusMinus` emits **minus before plus** (`changeset.cpp:230-233`); `-`/`+` flush keeps when `lastOpcode == '='`; `=` flushes plus/minus when `lastOpcode != '='`; `toString` is `flushPlusMinus` then `flushKeeps`. `lastOpcode` is set after the early returns in both, so a zero-`chars` op does not update it. Pinned by `selftest.cpp:200-205` (`-2+1`) and `make_splice("abcde",1,2,"YZ") == "Z:5>0=1-2+2$YZ"` (`selftest.cpp:268`). |
| C.12 | `lengthChange` | `changeset.cpp:241, 245`; `changeset.h:94` | `SmartOpAssembler.ts:64, 70, 114` | **Match.** Updated only in the `-` and `+` branches, after the `!opcode`/`!chars` guards. nib has no `clear()` so it never resets — unreachable (N-8). |
| C.13 | `endDocument` trailing-keep rule | `changeset.cpp:190-191`; `changeset.h:92` (`keep_.end_document()`) | `MergingOpAssembler.ts:25-26`; `SmartOpAssembler.ts:110-112` | **Match**, including the detail that `SmartOpAssembler::endDocument` calls `endDocument` on the **keep assembler only**. Pinned negatively by `selftest.cpp:171-173` (a written-out trailing keep is refused). |
| C.14 | `ops_from_text` multiline split | `changeset.cpp:311-331` | `Changeset.ts:216-234` | **Behaviourally equivalent; representationally different.** Both split at `lastIndexOf('\n')`, assign the newline count of the **whole** text to the first op, and emit the post-newline tail as a separate in-line op. nib **suppresses zero-length ops** (`changeset.cpp:319`, `:330`) where Etherpad always yields them. Immaterial for every in-tree caller, because both assemblers drop `chars <= 0` before touching any state (`changeset.cpp:206`, `:237`; `MergingOpAssembler.ts:40`, `SmartOpAssembler.ts:56-57`). It *is* observable to a direct caller of the generator — worth a comment, since `ops_from_text` is a public header symbol (`changeset.h:112`). |
| C.15 | `make_splice` clamping | `changeset.cpp:333-351` | `Changeset.ts:824-839` | **Divergence, deliberate and documented in the .cpp — but the .h misdescribes Etherpad.** Etherpad **throws `RangeError`** on `start < 0` or `ndel < 0` (`Changeset.ts:825-826`) and clamps only the upper bounds. nib clamps all four (`changeset.cpp:337-340`). The comment at `changeset.cpp:336-337` states this correctly; `changeset.h:115` says start/ndel are "clamped to the document rather than rejected, **as Etherpad clamps them**", which is false for the negative cases. Upper-bound clamping, `deleted`, the three `opsFromText` calls, attribs applied to `+` only, and `pack(orig.length, orig.length + ins.length - ndel, ...)` are an exact match. Verified: `nib --splice abcde -3 -3 Q` → `Z:5>1+1$Q` (clamped to 0,0). |
| C.16 | `Builder` bank accumulation | `changeset.cpp:353-391` | `Builder.ts:23-106` | **Match.** `charBank` is appended **only** in `insert` (`changeset.cpp:374` ≡ `Builder.ts:81`); `keep`/`keepText`/`remove` never touch it; `remove` forces empty attribs; `toString` is `endDocument()` → `oldLen + getLengthChange()` → `pack`. Pinned by `selftest.cpp:286-294`, which asserts the Builder and `make_splice` reach the same bytes. |
| C.17 | base 36 for numbers > 2^53 or negative | `changeset.cpp:31-58` | `ChangesetUtils.ts:66, 74` | **Diverges above 2^53** (JS doubles lose integer precision; nib is exact to 2^63 then UB). Negative: `num_to_string` matches JS's `-`-prefixed form, but `parse_num` cannot read it back. Unreachable from any legal changeset; see L-1 for the hardening. |

### C.18 · UTF-16 code units vs UTF-8 bytes — every place it matters

Etherpad counts **JavaScript string length**, i.e. UTF-16 code units. nib counts **UTF-8 bytes**
(`std::string::size()` everywhere). This is not a bug in the port; it is a unit change that the port
applies consistently. But it has consequences, and the documentation gets the unit wrong.

Every site where the unit is load-bearing:

| site | file:line | effect |
|---|---|---|
| `Op::chars`, `Op::lines` | `changeset.h:41-42` | `chars` is a byte count. `é` = 2, `😀` = 4; Etherpad would write 1 and 2. |
| `pack`'s `oldLen` / the sign+magnitude | `changeset.cpp:136-140` | Header numbers are byte counts. |
| `unpack`'s `old_len`/`new_len` | `changeset.cpp:120-126` | Interpreted as bytes. |
| `apply_to_text`'s length guard | `changeset.cpp:146` (`str.size()`) | A changeset from a real Etherpad server would be refused as `mismatched apply` for any non-ASCII document. |
| `check_rep`'s bank arithmetic | `changeset.cpp:280, 288, 300` (`char_bank.size()`) | Bytes. |
| `ops_from_text` chars/tail | `changeset.cpp:317, 323, 328` (`text.size()`) | Bytes. |
| `make_splice` start/ndel/lengths | `changeset.cpp:337-350` (`orig.size()`, `ins.size()`) | `start` and `ndel` are byte offsets, not character indices. |
| `Doc::splice` clamping and inverse | `doc.cpp:50-60` | Byte offsets; no UTF-8 alignment (see C-1). |
| `LineIndex` | `doc.cpp:159-184` | Byte offsets throughout. |
| the caret, selection, painting | `edit.cpp:198-201, 302-310, 514-515, 534-535` | **Mixes the two units** — this is C-1, and it is the only place where the byte choice actually breaks. |
| the status line's "chars" | `edit.cpp:557-560` (`g->doc.size()`) | Labelled `chars`; it is bytes. |

**Direct consequence for a stated MUST:** SPEC 3.1.1 says *"A changeset produced by nib and one
produced by Etherpad for the same edit MUST be byte-identical."* For `é` inserted into an empty
document, nib emits `Z:0>2+2$é` and Etherpad emits `Z:0>1+1$é`. **The clause is false for any
non-ASCII edit**, and nothing in `--selftest` can detect it, because every changeset and document
vector in the battery is pure ASCII (`selftest.cpp:63-306`, `:326-327`, `:492-493`, `:510`).

**Does the documentation acknowledge it?** Partially, and with the wrong unit:
- SPEC 2.2.3 says `chars` counts bytes *"consistent with Etherpad's own treatment of the document as
  a string of **code units**"* — bytes are not code units, so the justifying clause is
  self-contradicting.
- SPEC 14.1 defers *"whether `chars` must count **code points** rather than bytes for server
  interoperability."* Code points would still not match: `😀` is 1 code point and **2** UTF-16 code
  units. Answering §14.1 as written would not achieve interoperability.
- Nothing anywhere marks SPEC 3.1.1 as conditional on ASCII.

**Suggested fix (documentation first, code later).** Restate SPEC 2.2.3 as "byte offsets, which
differ from Etherpad's UTF-16 code units for any non-ASCII text"; restate §14.1 as "code **units**,
not code points"; and add an explicit ASCII qualifier to SPEC 3.1.1 with a pointer to §14.1. If
interop ever matters, the conversion belongs at the wire edge, not in the document model.

---

## D · Spec-vs-code drift table

Every clause SPEC marks **[BUILT]**, checked against the code. `✔` = the code does it;
`✗` = it does not; `~` = it does it partially or with a caveat that changes the meaning.

| SPEC | claim | verdict | evidence |
|---|---|---|---|
| 2.1.1 | document is an ordered changeset log + cached text | ✔ | `doc.h:27-64`, `doc.cpp:31-40` |
| 2.1.2 | *"`Doc::splice` and `Doc::apply` are the only mutators"* | ~ | `doc.cpp:49`, `:89` — but `Doc::set` (`doc.cpp:11-29`) also assigns `text_` directly and bypasses `push()`/`check_rep`. L-2 |
| 2.1.3 | folding the log reproduces the text byte for byte, at every point | ✔ | `doc.cpp:145-156`; `Ctrl+R` at `edit.cpp:668-678`; 1,000-edit property test `selftest.cpp:462-506`. **Caveat: it is green on a document C-1 has already corrupted** — the log faithfully records the corrupting splice |
| 2.1.4 | the document holds LF only | ✔ | `edit.cpp:396-402` (load), `:418-425` (save), `:340-347` (paste) |
| 2.2.1 | UTF-8; offsets are byte offsets | ✔ | throughout |
| 2.2.2 | *"one keystroke moves or removes one character, never one byte"* | **✗** | True for Backspace/Delete/Left/Right (`edit.cpp:254-268`, `:286-300`). **False for Up/Down and for a mouse click** — `move_vertical` (`edit.cpp:278-284`) and `offset_at_point` (`edit.cpp:302-310`) pass a byte column to `LineIndex::offset_of` (`doc.cpp:179-184`). **C-1, reproduced** |
| 2.2.3 | `chars` counts bytes, "consistent with Etherpad's… code units" | ~ | Code is consistent (bytes everywhere); the *justification* is wrong — bytes ≠ code units. See C.18 |
| 2.3.1 | undo is an appended inverting changeset; the log is never truncated | ✔ | `doc.cpp:111-121`; asserted by `selftest.cpp:408-409` and `drive.py:207-208` |
| 2.3.2 | the inverse is computed against the text the edit *produces* | ✔ | `doc.cpp:58-60` |
| 2.3.3 | a new edit discards the redo stack | ✔ | `doc.cpp:84` |
| 2.3.4 | undo groups: same author + contiguous + within 700 ms + same kind; a newline closes | ✔ | `doc.cpp:64-82`; the five closing conditions each pinned at `selftest.cpp:419-460`; `drive.py:195-214` |
| 2.3.5 | `Doc::apply` closes the open group unconditionally | ~ | `doc.cpp:98` does close it — but it leaves the *stored* groups in place, which is H-4. `Doc::apply` has **zero test coverage** |
| 2.3.6 | undo and redo stacks carry opposite orderings | ✔ | `doc.cpp:111-119` (reverse), `doc.cpp:134-140` (forward) |
| 3.1.1 | a nib changeset and an Etherpad changeset for the same edit are byte-identical | **✗** | False for any non-ASCII edit: nib counts bytes, Etherpad counts UTF-16 code units. See C.18. Not detectable by the current battery (all vectors ASCII) |
| 3.1.2 | `check_rep` verifies canonical form by re-serialising and comparing bytes | ✔ | `changeset.cpp:301-306`; four negative vectors at `selftest.cpp:168-188` |
| 3.1.3 | canonical form: fuse, deletes-before-inserts, implicit trailing keep, multiline ends on `\n` | ✔ | `changeset.cpp:205-251`; `selftest.cpp:191-229` |
| 3.1.4 | lower-case base 36 matching JS | ✔ | `changeset.cpp:31-46`; `selftest.cpp:60-76` |
| 3.2 | the nine ported entry points exist | ~ | All nine exist and are correct. But `check_rep` has **no CLI verb**, and `--unpack`/`--ops` are defined and never dispatched. **H-1** |
| 4.1.1 | fixed pitch, so a column is arithmetic; caret derived from the model each paint | ~ | `edit.cpp:165-178`, `:531-541` ✔ for the font and the derivation. "A column is arithmetic" holds only for Latin-1: `paint` measures in UTF-16 units × `cw`, so CJK (1 unit, 2 cells) and astral (2 units, 1 cell) both misplace the caret |
| 4.1.2 | *"MUST be per-monitor DPI aware and MUST rebuild its font on `WM_DPICHANGED`"* | **✗** | **Measured `PROCESS_DPI_UNAWARE`, `GetDpiForWindow` → 96.** No manifest, no `SetProcessDpiAwarenessContext`, no `/MANIFEST` (grep: zero hits). `WM_DPICHANGED` (`edit.cpp:592-599`) can never be delivered. **M-1, reproduced** |
| 4.1.3 | paint order: selection band, glyphs, caret | ✔ | `edit.cpp:509-528` then `:531-541` |
| 4.2 | the fourteen bindings in the table | ✔ | `edit.cpp:605-681`; `Ctrl+Y` also present (`:661`) though the table lists it |
| 4.2.1 | typing over a selection is exactly one changeset | ✔ | `edit.cpp:249-252`; verified live by `drive.py:186-190` |
| 4.2.2 | pasted CR/CRLF normalised to LF | ✔ | `edit.cpp:340-347` |
| 4.3.1 | atomic save: temp in the target's directory, flushed, then replaced; failure leaves the previous file intact | ✔ | `edit.cpp:371-391` — `CREATE_ALWAYS`, `FlushFileBuffers`, `MoveFileEx(REPLACE_EXISTING\|WRITE_THROUGH)`, temp deleted on both failure paths. (Diagnostics and ACL preservation: M-10) |
| 4.3.2 | line-ending convention and BOM detected on open, restored on save; **MUST NOT silently convert** | ~ | ✔ for a uniformly-CRLF file and for a BOM (`edit.cpp:394-425`; `drive.py:271-284`). **✗ for a lone-CR file and for a mixed CRLF/LF file** — both silently rewritten. **M-5** |
| 4.3.3 | closing with unsaved changes MUST prompt | ~ | ✔ on `WM_CLOSE` (`edit.cpp:471-480`, `:764-767`). **✗ on `WM_QUERYENDSESSION`** — not handled, work is lost at shutdown/logoff. **M-6** |
| 4.3.4 | opening restarts the log with the file as revision 1 | ✔ | `edit.cpp:403-405`, `doc.cpp:11-29` |
| 4.4.1 | status line: line/col, selection size, size, revisions, dirty | ✔ | `edit.cpp:544-562` |
| 4.4.2 | when the resident exists, also show the AI switch, mode, egress | n/a | The editor never constructs a `Resident`; the clause is not yet in force. But see 9.4 |
| 4.5.1 | theme read from `nib.theme` beside the exe, with compiled fallbacks | ✔ | `edit.cpp:91-122`, `:784` |
| 4.5.2 | `theme_detect.py` derives it from an image | ✔ | `tools/theme_detect.py` |
| 5 (preamble) | *"39 checks in `--selftest` and 5 more … by `tools/drive.py`"* | ✔ | Counted: 39 ingest checks (`selftest.cpp:522-817`), 5 ingest checks in `drive.py:243-255` |
| 5.1.1 | `auricle::fusor::StreamingTextSource`, included unmodified | ✔ | `ingest.h:23`; `build.bat:14-16` fails if `source.h` is absent |
| 5.1.2 | percept at a closed thought, N chars or T ms; ends on a word boundary where possible | ✔ | `ingest.cpp:104-130`; `selftest.cpp:575-598` |
| 5.1.2.1 | closed thought = `.`/`!`/`?` at a word end, or newline; `;`/`:` do not | ~ | `ingest.cpp:28-45` ✔ for `;`/`:` and `3.14`. **✗ for `e.g.`/`Fig.`/`Mr.` (over-split) and for `stop."`/`(late.)` (never closed).** **M-2, reproduced** |
| 5.1.2.2 | a lane change closes the pending clause | ✔ | `ingest.cpp:138`; `selftest.cpp:702-710`. Side effect: it swallows the tick (M-3) |
| 5.1.3 | deletions MUST be delivered as percepts | ~ | ✔ for Backspace/Delete/Cut via `edit_splice` (`edit.cpp:226-243`, the removed bytes captured *before* the splice at `:228-229`). **✗ for undo, redo and file-open, which remove text and emit nothing. H-2, reproduced** |
| 5.1.4 | ingest is unconditional; a full ring counts loudly | ✔ | `ingest.cpp:198-202`; `selftest.cpp:801-817` (1,024 pushed / 1,976 dropped) |
| 5.1.5 | the compiler never summarises, cleans, annotates or interprets | ✔ | `ingest.cpp` — the only added bytes are `removed_mark` (declared [OPEN]) and the tick |
| 5.1.6 | self-echo filtered at the source, in `PadSource` | ~ | `ingest.cpp:178-191`, `:207` ✔ mechanically and case-insensitively. **But the only seat registered is `"watcher"` (`edit.cpp:576`), which is not the name of any seat in `resident.cpp:38-45`. M-8** |
| 5.1.7 | chunk at **495 bytes**, never splitting a UTF-8 sequence or (where possible) a word | ✔ | `ingest.h:32`, `ingest.cpp:52-73`; `selftest.cpp:600-654` |
| 5.1.7.1 | a `PadSource` must never be a stack local | ✔ | `edit.cpp:78` (`unique_ptr`), `selftest.cpp:780`, `:805`; size asserted at `selftest.cpp:538` |
| 5.1.8 | the lane string is train ≡ serve | ✔ | `resident.cpp:338` produces `\n[lane] ` exactly as `fusord.cpp:724` |
| 5.1.9 | silence enters as world; `[tick +Ns]` byte-identical to `fusord.cpp:712`, emitted **before** the percept that broke the silence | ~ | `ingest.cpp:77-95`; `selftest.cpp:677-700`. The string matches `fusord.cpp:712` minus its leading `\n`; **the trunk stream does not** — nib wraps it in `[bo] …` and then judges on it, where fusord does neither. **M-4.** And the "before" ordering fails after a lane change or a deletion. **M-3** |
| 5.1.9.1 | *"the lifted resident loop MUST run with its own `--idle-tick-s` set to 0"* | n/a (clause describes a control that does not exist) | `Resident::Config` (`resident.h:53-66`) has no idle-tick field and `resident.cpp` contains no tick code — so the double-count hazard is structurally absent, which is *better* than the clause requires. The clause should be restated as "the lifted loop MUST NOT carry its own tick", which is what was actually done |
| 5.1.10 | removed text arrives intact behind a marker, excluded from the arithmetic | ✔ | `ingest.cpp:154-160`; `selftest.cpp:656-675` |
| 5.1.11 | `typed_in == typed_out`, `removed_in == removed_out`, `pushed + dropped == percepts`; fired from inside the window | ✔ | `ingest.h:94-100`; `selftest.cpp:766-774`; `edit.cpp:716-732`; `drive.py:246-255`. **Caveat: green through H-2** — it audits the compiler, not the feed |
| 6.0.1 | refuse to start if GPU layers were requested and no GPU backend came up | ✔ | `resident.cpp:174-190` (enumerate, name, refuse unless `allow_cpu`) |
| 6.2.1 | the loop is lifted, not re-derived | ✔ | `resident.cpp:279-298` ≡ `fusord.cpp:461-470`; `resident.cpp:310-332` ≡ `fusord.cpp:653-684`; `resident.cpp:344-357` ≡ `fusord.cpp:733-746`. The probe **forks the KV** (`llama_memory_seq_cp(TRUNK→DECIDE)`) exactly as fusord does — it does not re-decode the trunk |
| 6.2.2 | seed/examples/opener/probe/cue copied verbatim; the build hashes them and refuses on drift | ✔ | `resident.cpp:19-45` compared line-by-line against `fusord.cpp:103-130` — identical. `serve_hash` (`resident.cpp:50-68`) is character-for-character `fusord.cpp:139-156`, same FNV-1a offset basis `1469598103934665603`, same prime `1099511628211`, same order (seed, examples, opener, then per-seat name+mandate, then `"\n["`, `" — "`, `"]\nwatcher:"`, then the cue frame in three pieces). The refusal is at `resident.cpp:130-139` |
| 6.2.4 | the pin is `0xe7ffa5704ba31076` and nib computes it | ✔ | `resident.h:49` ≡ `fusord.cpp:157`; **measured: `--selftest` prints `0xe7ffa5704ba31076` on both sides** (A.2). This is real mechanical proof and it is the strongest thing in the repository |
| 6.2.5 | the speak-cue is carried and hashed though never decoded | ✔ | `resident.cpp:62-66`; no sampler is created (`resident.cpp:248`), no generation code exists |
| 6.2.6 | `n_ctx` 8192, configurable | ✔ | `resident.h:57`, `resident.cpp:234`. **What happens when the window fills is undocumented and is a silent drop.** H-3 |
| 9.1 | `build.bat` fails on `ws2_32`, `wininet`, `winhttp`, `urlmon`, `dnsapi` | ✔ | `build.bat:34-39` — all five, `/i` case-insensitive, over the whole dumpbin output (which includes the delay-load list). Measured: exe has none (A.5) |
| 9.4 | *"The status line MUST carry an egress counter"* (unmarked, so implicitly in force) | **✗** | `edit.cpp:544-562` — no egress counter, no `0 bytes egress`. CLAUDE.md rule 2 states it as a product property. Either build it or mark 9.4 [SPECIFIED] |
| 9.5 | the resident cannot act outside the document | ✔ | trivially — `resident.cpp` opens no file, spawns nothing, synthesises no input |
| 11.1 | *"`--selftest` MUST pass before every commit. **[BUILT: 71 checks]**"* | ~ | It passes (**120**). The count is stale by 49 |
| 11.2 | every stage below Stage 2 runnable with no model in the process | ✔ | Measured: `--selftest` and `--ingest` ran with llama/ggml delay-loaded and never touched |
| 11.3 | 10,000 random splices; 1,000 random edits with undos; deterministic generators | ✔ | `selftest.cpp:308-359`, `:462-506` — splitmix64, fixed seeds |
| 11.4 | the window verified by a message-posting driver; global input forbidden. **[BUILT — 17 checks]** | ~ | ✔ mechanically (`drive.py`; no `keybd_event`/`SendInput` anywhere). The count is stale: **22** |
| 11.4.1 | the driver binds to the pid it launched | ✔ | `drive.py:65-81` |
| 11.4.2 | the driver waits on artefacts, not on sleeps | ~ | `drive.py:106-115` ✔ for `wait_for`. But `type()` and `key()` still `time.sleep` a computed interval (`drive.py:87`, `:94`), and the `cmd()` for `top`/`end`/`sel_to_home` (`drive.py:96-97`) has no artefact to wait on — mitigated by a bare `time.sleep(0.2)` at `drive.py:183`. Not currently flaky (three identical runs), but the clause is stronger than the code |
| 11.5 | no performance number appears anywhere it was not measured, with its date | ✔ | Spot-checked: ROADMAP's Stage 1b table, the devlog's 2026-09-04 entry, and the 47× figure all carry the box and the date; VRAM is explicitly **not** claimed |

---

## E · Doc-vs-doc inconsistencies

| # | where | what | severity |
|---|---|---|---|
| E-1 | `README.md:7-8` | *"**Status: blueprint (0.0.0).** Nothing is built."* The repository is at 0.6.0 with seven stages green, 120 + 22 checks, and a working editor. This is the **first file a reader opens** and it is the most wrong document in the tree | **High** — it is the front door |
| E-2 | `src/nib.cpp:227` | `printf(kUsage, "0.1.0")` — the CLI reports **0.1.0**. ROADMAP, HANDOFF and the git tags say 0.6.0 | **High** — the binary misreports its own version |
| E-3 | `SPEC.md:429` vs measured | `11.1 … **[BUILT: 71 checks]**`; actual **120**. (HANDOFF §4.3 and ROADMAP both say 120, so SPEC alone is stale) | Medium |
| E-4 | `SPEC.md:441` vs measured | `11.4 … **[BUILT]** — tools/drive.py, **17 checks**, 2026-09-04`; actual **22** | Medium |
| E-5 | `HANDOFF.md:208-219` vs `HANDOFF.md:134-140` and `:90` | §5's "Resume here" still opens with the Stage 0e block and says `nib.exe --selftest  **77 passed**` / `drive.py  **17 passed**`, while §3's table (line 90) and §4.3 (lines 134-140) say **120 + 22**. **The handoff contradicts itself by 43 checks**, and §5 is the section the file's own header tells a new session to read for where to resume | **High** — this is the cold-start document |
| E-6 | `docs/ASSEMBLY.md:106-107` | Still states *"`kPayloadMax = 496`, so a `Delta` stays a tidy 512 bytes… Pad edits chunk at that boundary"*. Both numbers were corrected on 2026-09-04 (SPEC 5.1.7, devlog): `sizeof(Delta)` is **528** and chunking at 496 loses a byte per chunk **silently**. ASSEMBLY is named in HANDOFF §4.2 as reading item 4 and is declared to *supersede* BLUEPRINT — so it is still handing a new session the exact number the Stage 1a story exists to correct | **High** |
| E-7 | `docs/BLUEPRINT.md:106` | Still lists *"a word boundary"* as a percept trigger. Corrected in SPEC 5.1.2 on 2026-09-04 with a boxed note explaining it cannot be one. BLUEPRINT is reading item 5 and governs "intent" per SPEC's front matter | Medium |
| E-8 | `docs/BLUEPRINT.md:173-175` | *"The host is a bouncer, not a bottleneck… the room survives the host closing their laptop."* Directly contradicted by ASSEMBLY §3, SPEC 10.2 and ROADMAP. ASSEMBLY declares the supersession, but BLUEPRINT's text carries no marker at the point of the error | Low (declared) |
| E-9 | `CLAUDE.md:35-36` vs `build.bat:34` and `SPEC.md:386` | CLAUDE.md rule 2 names four network DLLs (`ws2_32`, `wininet`, `winhttp`, `urlmon`); the build and SPEC 9.1 check **five** (adding `dnsapi`). The build is stricter than the rule it implements — good, but the rule text is stale | Low |
| E-10 | `SPEC.md:3` and `ROADMAP.md:3` | Both are headed *"Rev 0.4 · 2026-09-03"* while carrying 2026-09-04 content (SPEC's 5.1.7 and 6.x corrections; ROADMAP's Stages 0e/1a/1b). Neither revision nor date was bumped | Low |
| E-11 | `SPEC.md:261` / `src/ingest.cpp:80-81` | *"byte-identical to `fusord.cpp:712`"*. `fusord.cpp:712` is `snprintf(tb, sizeof(tb), "\n[tick +%llus]"` — the format carries a leading `\n` that nib's tick omits, and fusord's tick reaches the trunk as a **bare line** while nib's is wrapped in `[lane] `. The claim is true of a substring, not of the stream. See M-4 | Medium |
| E-12 | `src/ingest.cpp:31` | Comment: *"`3.14` and `e.g. ` must not split a thought in half."* The code splits `e.g. ` (reproduced, M-2). A comment that states a property the code does not have | Medium |
| E-13 | `src/changeset.h:115` | *"`start` and `ndel` are clamped to the document rather than rejected, **as Etherpad clamps them**."* Etherpad **throws `RangeError`** on negatives (`Changeset.ts:825-826`) and clamps only the upper bounds. The `.cpp` comment (`changeset.cpp:336-337`) gets it right; the header does not | Low |
| E-14 | `SPEC.md:50-52` and `SPEC.md:473-474` | §2.2.3 justifies byte counting as *"consistent with Etherpad's… treatment of the document as a string of **code units**"* (bytes ≠ code units), and §14.1 asks about **code points** — a third unit, which would still not match Etherpad's UTF-16 code units for astral characters. The open question as posed cannot resolve the interop it names. See C.18 | Medium |
| E-15 | `src/nib.cpp:23-26` and `HANDOFF.md:184` | Both document `--unpack`, `--ops` and `--check` as working commands. None is dispatched (H-1) | High (folded into H-1) |
| E-16 | `SPEC.md:264-266` | 5.1.9.1 requires the lifted loop to be run with `--idle-tick-s 0`. nib's `Resident` has no such option and no tick code — the clause governs a knob that does not exist. Restate it as a prohibition rather than a setting | Low |
| E-17 | `docs/devlog.md:213` and `:276` | *"The exe is 350 KB"* / *"391 KB"*. Current build is **429 KB** (439,296 bytes). These are dated historical entries in a lab notebook, so they are correct as written — noted only so a reader does not take them as current | Nit |

**Cross-references that were checked and are correct** (worth recording, because most of them are):
`fusord.cpp:242-254` (the tightened boundary set) ✔; `fusord.cpp:709-716` (tick-before-percept) ✔;
`fusord.cpp:723-746` (per-Delta bracketed line + final judge) ✔; `fusord.cpp:712` (the tick
`snprintf`) ✔ as a line reference; `source.h` §5.8 self-echo ✔; the `0xe7ffa5704ba31076` pin
(`fusord.cpp:157`) ✔; SPEC §5 preamble's "39 + 5" check counts ✔.

---

## F · Build hygiene

### F.1 Are `.obj` and `nib.exe` tracked in git?

**No — correctly ignored.** `C:\nib\.gitignore` lists `*.obj`, `*.exe`, `*.pdb`, `*.ilk`, `*.res`,
`*.log`, `*.png` (with `!docs/*.png`), `obj/`, `runs/`, `build-dependents.txt`,
`build-imports.txt`. `git ls-files` returns **26** files, all source/docs/tools; `git ls-files | grep
-iE '\.(obj|exe|pdb|ilk)$'` returns nothing. `git status` was clean at 15:47 despite seven `.obj`
files and `nib.exe` sitting in the working tree — the build artefacts are correctly invisible to git.

Two small points: `.gitignore:9` carries `fray.ini`, a leftover from a different tool in the estate
(N-5); and `build-imports.txt` is ignored but never produced by `build.bat` — a vestige.

*As of 16:15 the tree is no longer clean: another session added the three untracked paths named in
the concurrency note at the head of this report. Those are documents and a Node harness, not build
output, and `.gitignore` correctly does not hide them — they are genuinely new work awaiting a
decision about whether to commit `tools/etherpad_harness/`'s `.mjs` files and `stubs/` (and, if so,
whether a `node_modules/` ignore line is needed).*

### F.2 Does the network-DLL gate actually fail the build? Does it check `dnsapi`?

**Yes to both**, and the logic is correct. `build.bat:33-39`:

```bat
dumpbin /nologo /dependents nib.exe > build-dependents.txt || exit /b 1
findstr /i /c:"ws2_32" /c:"wininet" /c:"winhttp" /c:"urlmon" /c:"dnsapi" build-dependents.txt >nul
if not errorlevel 1 (
  echo build: FAIL - a network DLL is among the dependents:
  findstr ... & exit /b 1
)
```

`findstr` exits 0 on a match, 1 on none. `if not errorlevel 1` is true exactly when the errorlevel is
0, i.e. **when a network DLL was found** — so the build fails on a match. Correct, and easy to get
backwards. All five names are checked, including **`dnsapi`**, so SPEC 9.1's claim is accurate (it is
CLAUDE.md rule 2 that lists only four — E-9).

Because `dumpbin /dependents` prints the delay-load list in the same output, the gate covers a
**delay-loaded** network DLL as well as a statically-imported one. That is the right behaviour and it
is not stated anywhere; worth a comment.

Two weaknesses:

- The gate greps the whole dumpbin output, which includes the **absolute path of the exe**. A build
  directory containing any of the five substrings would false-positive. Restricting the search to the
  region between `dependencies:` and `Summary` — or grepping for `.dll` lines only — would remove it.
- The `|| exit /b 1` after `dumpbin` catches a missing `dumpbin`, so the gate cannot be skipped by a
  broken toolchain. Good.

### F.3 Is the delay-load of `ggml`/`llama` verified by the gate?

**No.** `build.bat:25-28` passes `/DELAYLOAD:llama.dll /DELAYLOAD:ggml.dll /DELAYLOAD:ggml-base.dll`
and links `delayimp.lib`, and A.5 confirms all three land in the delay-load list — **but nothing
asserts it.** If a `/DELAYLOAD` were dropped in an edit, the build would still print `OK` and the
gate would still pass; the failure would surface only later, as `nib --selftest` refusing to start on
a machine without `C:\llama.cpp`. That is exactly the property ROADMAP:31-35 and SPEC 11.2 rest on
("a battery that needs a 9B is a battery that stops being run"), and it is the one refusal in this
build that is *not* mechanically enforced.

**Suggested fix**, three lines beside the existing gate:

```bat
findstr /i /c:"llama.dll" /c:"ggml.dll" /c:"ggml-base.dll" build-dependents.txt >nul
rem ... and assert they appear only AFTER the "delay load dependencies:" header
```

Simpler and stronger: `dumpbin /imports nib.exe` and fail if any of the three appears outside the
delay-load section. Strongest: have `drive.py` (or a new `--about`) run `--selftest` with
`SetDllDirectory` pointed at an empty directory and assert exit 0 — that tests the property rather
than the flag.

### F.4 Is `C:\auricle\src` a hard build dependency? What happens without it?

**Yes, hard, absolute, and un-overridable — but it fails loudly and early.**

```bat
set AURICLE=C:\auricle\src
if not exist "%AURICLE%\fusor\source.h" ( echo build: FAIL - intake seam not found at ... & exit /b 1 )
set LLAMA=C:\auricle\third_party\llama.cpp
if not exist "%LLAMA%\lib\llama.lib" ( echo build: FAIL - llama import libs not found at ... & exit /b 1 )
```

On a machine without `C:\auricle` the build stops at `build.bat:16` with a named path and a reason
(and at `:22` for the import libs). That is the right shape. Four observations:

1. **The dependency is deeper than the check.** `source.h:27` includes `"core/ring.h"`, which resolves
   to `C:\auricle\src\core\ring.h` (present, 2,996 bytes). A partial auricle checkout with
   `fusor/source.h` but no `core/ring.h` passes the guard and then fails in `cl` with a compiler
   error. Check `core/ring.h` too, or check the include as a unit.
2. **`AURICLE` and `LLAMA` are set unconditionally**, overwriting any environment value. Change to
   `if not defined AURICLE set AURICLE=...` so the paths can be pointed elsewhere on another box.
3. **The design decision is right and worth keeping.** `ingest.h:7-8` and SPEC 5.1.1 make it a rule
   that `source.h` is *included*, not copied, so the two cannot drift — the build breaking on a
   missing auricle is the price and it is correctly chosen. `build.bat:13-14` says exactly this.
4. **Run-time dependencies are separate and softer.** `C:\llama.cpp` (`resident.h:55`) and
   `C:\models\…` (`resident.h:54`) are hard-coded defaults, both overridable
   (`--llama-dir`, `--model`), and their absence produces a sentence rather than a crash
   (`resident.cpp:160-165`, `:197`) — the whole point of the explicit `LoadLibraryExW` block. Verified
   indirectly: `--selftest` and `--ingest` ran to completion with neither present in the process.

### F.5 Other build-hygiene notes

- **Zero warnings under `/W4 /WX`** — confirmed (A.1). Note that this did **not** catch the two
  dead functions in H-1, because MSVC's C4505 does not fire for anonymous-namespace functions. If
  dead code matters, add `/analyze` or a periodic clang-tidy pass.
- **The build is reproducible in size**: an independent build of the committed source produced an exe
  the same 439,296 bytes as the operator's (A.6). Only the embedded timestamp and PDB path differ.
- **`build-dependents.txt` is deleted on success and left behind on failure** (`build.bat:40`), and is
  gitignored. Correct.
- **No `link /INCREMENTAL:NO` and no object cleanup**, but `cl` recompiles every translation unit
  every time, so stale objects cannot survive. Fine.
- **Static CRT (`/MT`)**, `NOMINMAX`, `/utf-8`, `/Zc:__cplusplus`, `/permissive-` — all correct for a
  one-exe Win32 tool, and `/utf-8` is load-bearing for the em dash inside the byte-frozen probe frame
  (`resident.cpp:61`), which is what makes the `serve_hash` pin reproduce.

---

## Closing assessment

The changeset port is the strongest thing here: I compared it function by function against
`Changeset.ts`, `Op.ts`, `MergingOpAssembler.ts`, `SmartOpAssembler.ts`, `OpAssembler.ts`,
`ChangesetUtils.ts` and `Builder.ts`, and found **no semantic divergence in the canonical-form
machinery at all** — including the places where fidelity means faithfully reproducing Etherpad's own
gaps (the missing bound check on `=` in `checkRep`). The `serve_hash` pin is genuine mechanical
proof that the fusord lift was verbatim, and it is cheap enough to run in every battery, which is
exactly the right shape for that kind of gate.

The defects cluster in one place and have one shape. **Everything that is checked is correct;
everything that is not checked is where the bugs are.** The op log is verified byte-exactly a
thousand times over — and it will faithfully record a splice that destroys a character, because the
verification is of the log, not of the text. The ingest conservation identity is verified over 4,000
random operations — and it reads green while an undo silently empties the document past it. The
serve bytes are hashed — and the resident goes deaf at 8,192 tokens with no counter. In each case the
falsifier is real, and in each case the hole is one level below where the falsifier looks.

The three that would bite a real user in the first hour are C-1 (a Down-arrow and a Backspace destroy
an accented character), C-2 (emoji become two U+FFFD), and M-1 (the window is DPI-unaware on a 225 %
box). The three that would bite the *record* are H-3 (silent deafness), M-11 (non-reproducible
`--resident` runs) and M-12 (judgments computed from stale logits). H-2 and H-4 are the two that
Stage 2 will walk into on its first day.
