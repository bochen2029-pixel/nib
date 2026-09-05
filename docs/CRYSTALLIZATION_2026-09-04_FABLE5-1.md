# nib — CRYSTALLIZATION, QC, AND THE ARC TO THE EDGE

*2026-09-04 · Claude Fable 5.1 · for Bo Chen · written to disk for the session that remediates and
implements. Register: REVIEW + DESIGN. Claim grammar per the estate: **[M]** measured on this box,
receipt named · **[I]** established by inspection of the code, file:line cited · **[D]** derived, chain
shown · **[SPEC]** designed, unbuilt · **[BET]** kill condition named · **[BUDGET]** a target, not a
measurement. Where this document and a dated receipt disagree, the receipt wins and this document is
the defect.*

**Read in full, one hand, this session:** `README.md` · `HANDOFF.md` · `CLAUDE.md` · `docs/BLUEPRINT.md`
· `docs/ASSEMBLY.md` · `docs/SPEC.md` · `docs/ROADMAP.md` · `docs/devlog.md` · every file in `src/`,
`tools/`, `build.bat`, `nib.theme`, `tests/margins.txt`, `.gitignore` · `C:\auricle\src\fusor\fusord.cpp`
(the 2026-08-13 snapshot, all 791 lines) · `source.h` · `fabric/fabric.h` · `fabric/durable.h` ·
`app/syncytium.cpp` · `app/tray.cpp` · `app/main.cpp` · `app/hud_demo.cpp` · `overlay/hud.h` ·
`tape/tape.h` · the FUSOR master one-pager and addenda A, B, F, Q · the AS-OF one-pager · the
whole-machine implementation spec · `OPEN_PROBLEMS_2026-08-12` · `FUSOR_DECISIONS_2026-08-12` ·
`UNPROMPTED_BENCH_SPEC_v0` §3–§8 · `FUSOR_X_ESTATE_THE-NEEDLE` · `ROADMAP_v2_2026-08-28` ·
`NEXT-STEPS_SURVEYOR-ERA`. **By subagent (four Opus instances, reports on disk beside this file in
`docs/review/`):** the repo QC with the oracles re-run · the auricle lift map for Stages 2–4 · the
Etherpad deep read with a differential-harness feasibility probe · the prior-art survey on floor
control. Their findings are folded into §3, §4, §6 and §8 and are attributed where they appear.

> **Status, 2026-09-04 (later the same day).** §8's **P0a and P0b landed as 0.6.1** — the two
> criticals, DPI, the runtime module gate, the six Stage 2 breakers, the undispatched verbs, the
> delay-load assertion, and every doc correction in §3.3. Oracles: 144 in the exe, 27 from the
> driver, `--about` green. One incident during the pass is recorded in the devlog (a driven window
> took the keyboard). Next is §8's P1, Stage 1c.

---

## §0 · The one paragraph

nib is four hours old and already the best-disciplined thing in the estate: an Easysync port green
on Etherpad's own vectors, a document that is an op log and replays byte-exact under a thousand
random edits, a window driven by messages rather than a keyboard, a compiler that conserves every
byte it is handed, and a resident whose seed is proved verbatim by a hash rather than a promise —
and whose margins move per seat, by mandate, on prose it has never seen. Every stage carries a
falsifier and every falsifier has been fired at. That is the good news, and it is real. The rest of
this document is about three things: **(1)** a set of defects the next session must fix before a
word is emitted — two reproduced criticals in the editor (a Down-arrow and a Backspace destroy an
accented character while the replay check still reads byte-exact; any emoji becomes two U+FFFD),
six latent breakers for Stage 2, and one that quietly breaks the Act I network law at runtime. The
QC's closing sentence is the sharpest thing anyone said today and belongs on the wall: *everything
that is checked is correct; everything that is not checked is where the bugs are — in each case the
hole is one level below where the falsifier looks*; **(2)** a design for floor control and un-saying that is sharper than
the one in the spec, because the op log already gives nib the one mechanism the fabric uses to kill
a forming thought, and nobody has noticed it applies to keystrokes; **(3)** the arc to the edge — the
twin race as a *replay* of the tape rather than a live experiment, the AI-off switch that returns the
card, the document as its own memory, the file as a valid lane stream, and the measurement plan that
makes Stage 5 a number rather than a feeling.

---

## §1 · Bearings — the arc, dated by git rather than by belief

| when (CDT) | what | evidence |
|---|---|---|
| 2026-09-03 01:59 | Etherpad first named in the builder session (a night session that had started 23:28 on 09-02 in `C:\glance`) | session tape, first `etherpad` mention |
| 2026-09-04 10:43 | "a writing surface with no send key on either side" first written | session tape |
| 10:57 | `C:\nib` created; LICENSE, .gitignore, CLAUDE.md | file mtimes |
| 11:01 | 0.0.0 — the blueprint | `51a6bee` |
| 11:51 | the assembly correction (Etherpad is Node; port the changeset; OT not CRDT) | `4bf1da9` |
| 11:57 | 0.1.0 — Stage 0a, read half, 41 checks on Etherpad's vectors | `33be5fb` |
| 12:25 | 0.2.0 — Stage 0b, write half, 10,000 random splices | `92db268` |
| 12:35 | 0.3.0 — Stage 0c, the editor shell, 1,000 random edits with undo | `0100b60` |
| 12:51 | 0.4.0 — save, selection, theme, the driver seam | `73bdac9` |
| 12:54–12:57 | SPEC, ROADMAP, HANDOFF | `b45ab64`, `4485a64` |
| 13:11 | 0.4.1 — Stage 0e, the window driver, which found two real defects | `51f0e51` |
| 13:52 | 0.5.0 — Stage 1a, the compiler and PadSource; the seam lied about its size | `5d7c0a2` |
| 14:58 | 0.6.0 — Stage 1b, the mind holds; the silent CPU fall (47×) | `faecea0` |

**The lineage is older than the repo, and it is two ideas, not one.** Found with `everywhy` and
`everywhen` over the session tapes (the operator's words, verbatim):

- **2026-08-06** (`C--fun`): a swarm of residents "all read the same Etherpad" — HYPERCELLD's bus as
  a pad "in a very Etherpad type of way… 100 different people editing… you can see it in almost near
  real time." The pad as a *fabric for agents*.
- **2026-08-10** (`C--auricle`): "etherpad for the ai agentic era where an agent watches live on the
  warpbus… etherpad of years ago was a great concept except the rate limiting factor was human
  attention!!!! no human can watch a 100 person real time live edit." A web instance the same day
  named the category: *read-side infrastructure — the reader Etherpad never had.*
- **2026-09-03 01:59** (`C--glance`): Etherpad enters the builder session.
- **2026-09-04 10:42**: "using this streaming concept, it's not turn based. And so this matches with
  Etherpad because Etherpad is granular enough that it's also essentially not turn based… it can
  also type in real time, like a human being that you can see it type in real time." **10:51**: "The
  problem with the ether pad is that there was hardly anyone to use it with… for the first time a
  resident mind… there is not even a need to set a system prompt for the AI because it is prompted
  as it goes because **the interaction is the prompt**." **10:56**: the two toggles — AI on/off, and
  turn-based vs resident loop.

So there are **two diagnoses of Etherpad in the estate, and they are both right at different
radii.** The August diagnosis — *human attention was the limit; the missing organ is a reader* — is
Act II/III's claim (a resident that can read a hundred-person pad). The September diagnosis —
*simultaneity was the limit; the missing party is a second author who is always free* — is Act I's.
nib's docs carry only the second. Write both, in order, because the first is why the seat
abstraction exists in a single-user editor.

**A sibling was specified two days earlier.** `C:\notice\SPEC.md` (2026-09-02) is a FUSOR-1 desk
watcher with laws nib should not diverge from without saying so: the twin *and* a model-free null
run in shadow on every install and the resident is admitted to live only while it beats them on the
shadow ledger (law 7); the toggle is a file no code path writes; content never sets the clock; the
number fence; a consistency fence that pairs quantitative claims across documents. §5.5 adopts the
shadow-twin form; §8 adopts the consistency fence as a selftest.

**Where it stopped:** Stage 2 (emission with floor control) is next and nothing of it exists — by
construction, not omission: there is no sampler in `resident.cpp`, and the roadmap says so.

**State, measured 2026-09-04 by the builder [M]:** `--selftest` 120 passed / 0 failed; `drive.py`
22 passed / 0 failed, three identical runs; exe 439 KB, links kernel32/user32/gdi32/comdlg32 plus
three delay-loaded llama DLLs; Stage 1b margins on `tests/margins.txt` per the ROADMAP table
(SKEPTIC +5.56 on the Pacific line, SENTINEL +5.88 on the drop-table line); 222–260 ms per boundary
for three seats at n_ctx 8192, q8_0 KV, 34/34 layers on CUDA0. *The QC subagent re-ran the oracles
this session; its numbers are in §3.0.*

**The date defect, first because it is the family's own law.** Every document says 2026-09-03 for
Stages 0a–0d and the HANDOFF says it was "written 2026-09-03". Git says every commit is 2026-09-04
between 11:01 and 14:58, and the repo did not exist until 10:57 that morning. The builder session
began on the night of the 2nd and carried its start date through a compaction. Numbers carry their
date in this family; here the dates carry the wrong day. Fix by git (§8, P0).

---

## §2 · The idea, re-crystallized — and six places the prose overclaims

### 2.1 The claim in its sharpest form

Etherpad did not fail as technology; it failed as a matching problem. Its value was watching thought
form, and that value was locked behind two people being free at the same instant. A resident mind is
the first thing that is always free. So the first product is not collaboration with an assistant
bolted on; it is **the second party**, and the network can wait.

What makes the fit exact is that a pad is already a non-turn-based instrument in both directions.
Inbound, it is an ordered stream of small edits — the grain a decode-on-delta loop consumes. Outbound,
it already renders another author's characters as they arrive — the grain a streaming mind speaks.
Neither half alone would do. And a pad has one channel no speech system has: **you can watch someone
delete a sentence.** Deletion is the most informative percept in the stream, and it exists only on a
writing surface.

Five properties, none of which a chat box can have, all of which nib either has or is one stage from:

1. **Evidence-triggered speech.** The {hold, emit} decision is read off the free tail of the ingest
   pass at a thought boundary. No poll, no timer, no send key. [M — Stage 1b: the margins move per seat]
2. **Deletions are percepts.** A backspaced sentence reaches the trunk intact, marked as having left.
   [M — Stage 1a: byte conservation over 4,000 random typings, removals and idles]
3. **Un-saying, visible and on the record.** A forming sentence killed mid-word, the withdrawn words
   on the tape with why they died. [SPEC — Stage 3; the kill primitive is one `llama_memory_seq_rm`,
   and nib will measure its own keystroke-to-withdrawn latency rather than inherit a number — see
   §2.2]
4. **Authorship per character, permanent.** Etherpad's attribute pool gives it for free once ported.
   [SPEC — §7 of the SPEC]
5. **Structurally local.** The build fails if a socket is linked; the status line reads `0 bytes egress`
   because it is true. [M — build.bat gate; **but see §3.1, finding 1: true of the exe, not yet of the
   process**]

### 2.2 Six overclaims to repair in the prose before anyone quotes them

**"There is no system prompt."** The seed is byte-frozen and it begins `<|im_start|>system`. The true
claim is narrower and better: *there is no per-document prompt.* The seat's mandate is the only
imported purpose, it is frozen with the weights, and everything else the resident knows about this
document it learned by watching it be written. Say that. A reviewer who opens `resident.cpp:19` will
otherwise stop reading at the first line.

**"Marked in the buffer, in the file, and on the tape."** (CLAUDE.md rule 6.) The file is plain text,
and rule 1's honesty depends on it staying plain. There is a resolution that costs nothing and is
better than either horn: **the resident's paragraphs in the file begin with their lane tag, `[SKEPTIC] `,
which is byte-identical to the train ≡ serve line format.** The .txt becomes a valid lane-headed stream;
reopening it restores the lanes; a human's paragraphs are bare. Rule 6 holds in the file with no
sidecar and no markup. §5.7.

**"The twin race, one paired sample per flip."** BLUEPRINT §3 describes a twin that differs only in
its *trigger*. `FUSOR_DECISIONS_2026-08-12` D1 defines the estate's twin as differing in **four**
treatments (decode-on-delta, persistent KV, mid-decode abort, boundary-grain judgment). These are two
different instruments. nib should ship both, named: the trigger-only twin **T1** lives in the product
toggle and isolates initiative; the four-treatment twin **T4** is a *replay* of the tape, offline, and
isolates residency. §5.5. Do not let the product toggle be described as the twin race; it is half of it.

**"Floor control is the genuinely unsolved part."** Half true, and the prior-art survey (§7) shows
where it splits: in *speech* it is being solved and benchmarked now (Full-Duplex-Bench v1.5, HumDial
2026, with a named "responsive vs floor-holding" trade-off nib chose by fiat); in *text with an AI
participant* it is open — but CSCW's standing answer since Ellis & Gibbs 1989 is "no floor: transform
and make visible", and nib re-introduces a floor without arguing against that. The argument exists
and should be written: the floor is about the *human's attention*, not about correctness — OT
guarantees convergence, it does not stop a machine from writing into the sentence you are forming.

**"The act no turn-based system can perform."** As written this is false: any streaming UI can abort
and delete tokens. The defensible form is narrower and stronger: *no turn-based system can un-say in
response to evidence that arrived while it was speaking, because during a turn no such evidence
exists.* Say it that way, everywhere.

**"In 13 microseconds … while the twin missed the same moment by 65.7 seconds."** (BLUEPRINT §6,
README.) The lift map traced both numbers, and they are two experiments, two binaries, two clocks,
and not the same moment. The 13 µs is the wall time of one `llama_memory_seq_rm` in the M0-4 toy
(`m0_demo.cpp:177-181`), whose own comment says the kill *instant* is illustrated, not derived; the
honest end-to-end figures in the estate are milliseconds ("1 ms to silence"). The 65.7 s is
`runs/cabv2_twin_console.txt:114`: an *"earliest possible"* gap to the twin's next scheduled wake,
in **sim time under a virtual clock**, n = 1, between two separate runs of the cabin deck — the twin
never reacted at all, and the number is partly a property of the deck's authoring. Keep the shape of
the claim (a forming sentence killed by the world; a turn-based twin structurally unable to see the
moment) and drop both borrowed numbers until nib measures its own keystroke → withdrawn latency
(which includes a Win32 repaint) and its own twin gap from its own tape (§5.5). Report `seq_rm`
separately if at all, labelled as the branch-drop cost. BLUEPRINT §7's "1.208×" also needs its
bimodal caveat (`SESSION_HANDOFF.md:90`).

---

## §3 · QC — the repo as it stands

*§3.0 carries the QC subagent's re-run of the oracles; §3.1 the defects this hand found by reading
every line; §3.2 the subagent's additional findings; §3.3 doc-vs-doc drift. Severity: CRITICAL breaks a
law · HIGH loses data or breaks the next stage · MEDIUM wrong but bounded · LOW/NIT.*

### 3.0 The oracles, re-run this session [M 2026-09-04, QC subagent, scratch copy of the tree]

| what | result |
|---|---|
| `build.bat` | exit 0, **zero warnings** under `/W4 /WX /permissive- /std:c++20 /MT` |
| `nib.exe --selftest` | **120 passed, 0 failed**; no llama DLL touched, no GPU |
| `python tools/drive.py` ×3 | **22 passed, 0 failed** each; transcripts byte-identical (md5 `54d46514…`) |
| `nib.exe --ingest README.md --counts` | 3350 bytes → 81 percepts, longest 102, in 3350 == out 3350 (reproduces the devlog exactly) |
| `dumpbin /dependents` | KERNEL32, USER32, GDI32, COMDLG32 hard; llama, ggml, ggml-base delay-loaded; **no network DLL** in either list |
| exe | 439,296 bytes, same size as the operator's 14:53 build (hashes differ only by timestamp and PDB path) |

Four further probes through the `WM_APP+1` / `NIB_LOG` seam, no global input, all reproduced:

```
probe 1  type "ab\né\n", Ctrl+Home, Right, Down, Backspace, save
         disk: b'ab\n\xa9\n'   invalid UTF-8   and Ctrl+R says: replay 1 (byte-exact)
probe 2  GetProcessDpiAwareness(nib) = PROCESS_DPI_UNAWARE;  GetDpiForWindow = 96
probe 3  post WM_CHAR D83D, DE00 (one emoji as Windows delivers it)
         disk: EF BF BD EF BF BD   expected F0 9F 98 80
probe 4  type 26 chars, one undo: document empty; percepts=1 typed_in=27 typed_out=27 dropped=0
         the undo was never perceived, and the conservation identity still reads green
```

Two CLI observations: `nib --check "Z:5>1=2+1=2$X"` (a changeset `--selftest` itself proves is not
canonical) prints the usage banner and **exits 0**; `nib --unpack` likewise. Neither verb is
dispatched.

### 3.1 Findings by inspection [I]

**F1 · CRITICAL · The Act I network law holds for the exe and fails for the process.**
`resident.cpp:168` calls `ggml_backend_load_all_from_path(cfg_.llama_dir)`. That function loads every
known backend name from the directory — confirmed in the source on this disk,
`C:\IT\b9627-src\llama.cpp-b9627\ggml\src\ggml-backend-reg.cpp:559-586`, which calls
`ggml_backend_load_best("rpc", …)` in its list and then loads *any* DLL named in the
`GGML_BACKEND_PATH` environment variable — and `C:/llama.cpp` contains `ggml-rpc.dll` (136 KB), which
carries the `ws2_32.dll` import string (binary grep this session). So at `--resident` time a socket
library enters the process, and an environment variable can inject an arbitrary backend. fusord does
the same thing, but fusord never claimed the law. The `dumpbin /dependents` gate in `build.bat:33-39` inspects `nib.exe`'s
import table and cannot see a `LoadLibrary` that happens at `--resident` time. **Fix:** load the two
backends nib needs by explicit path (`ggml_backend_load("C:/llama.cpp/ggml-cuda.dll")` and the
best-matching `ggml-cpu-*.dll`; the API exists at `ggml-backend.h:253`), never the directory; and add
a *runtime* gate: after the backends load, enumerate the process's modules and refuse to start if
`ws2_32`, `winhttp`, `wininet`, `urlmon`, `dnsapi` or `ggml-rpc` is present. Print the module count on
the receipt. Then `0 bytes egress` is structurally true of the process, not the file. *Verify on the
next GPU run with a module listing; until then the claim on the status line is unproven.*

**F2 · HIGH · Undo and redo bypass ingest, so the resident's world diverges from the document.**
`edit.cpp:223-245` is the only funnel that feeds `PadSource`; `edit.cpp:653-667` (Ctrl+Z / Ctrl+Y) and
`edit.cpp:689-698` (the command seam) call `Doc::undo` / `Doc::redo` directly. Text removed or restored
by undo is never a percept. Rule 7 says deletions are percepts and the world is never edited; an undo
that removes a paragraph is invisible to the mind, which will then judge and, at Stage 2, *write
against* a document it has not perceived. **Fix:** route undo/redo through the same percept path —
derive `removed`/`typed` from the inverse changeset's delete and insert ops (the ops name the exact
bytes), or diff before/after. Also fix the author: `doc.cpp:115,138` stamp undone revs with author
`"undo"`/`"redo"` — in Act II a rev's author is a hand, not a verb. Add a `kind` field to `Rev`.

**F3 · HIGH · The self-echo filter guards the wrong lane names.** `edit.cpp:576` registers one seat,
`"watcher"`. The seats are `SPEAKER`, `SKEPTIC`, `SENTINEL` (`resident.cpp:38-45`), and fusord
additionally filters `"fusor"` (`fusord.cpp:706-707`). The moment Stage 2 commits a line on lane
`SKEPTIC`, it re-enters the compiler as world and the nucleus deliberates about interrupting itself —
the exact failure `source.h` warns about. **Fix:** register `seats()` by name, plus the resident's
self-commit lane, in one place (a `Hand` table, §5.8), and add a selftest that the window's filter set
equals the resident's seat set.

**F4 · HIGH · Ticks are not byte-identical to fusord, and the percept's kind is lost at the ring.**
SPEC 5.1.9 and `ingest.cpp:78` claim the tick is "byte-identical to fusord.cpp:712". fusord decodes
`"\n[tick +Ns]"` **raw onto the trunk with no lane prefix** (`fusord.cpp:712-715`). nib emits a Delta on
lane `bo` whose payload is `[tick +Ns]`, and `Resident::feed` prepends `"\n[bo] "` to every Delta
(`resident.cpp:338`), so the trunk sees `\n[bo] [tick +45s]`. Not the same bytes; v11 never saw a tick
inside a speaker's line. The root cause is structural: `Delta` carries `wall_ms, lane, len, payload`
and nothing else, so `Percept::kind` (`w`/`d`/`t`) is discarded in `PadSource::ship`
(`ingest.cpp:193-204`) and the resident cannot tell a tick or a deletion from typed text except by
sniffing the payload. **Fix, two parts.** (a) Now, in nib: a reserved lane sentinel (empty lane) means
*raw line*: `feed` decodes the payload after a bare `\n`, so the tick is byte-identical. (b) Properly,
in auricle: `Delta` has six bytes of padding (8+16+2+496 = 522 → 528); a `uint8_t kind` after `len`
fits in it with `sizeof(Delta)` unchanged (the payload's offset moves by one byte; nothing persists a
raw `Delta`) and `FileTailSource` writing 0. One line in `source.h`, and the compiler's kind survives
the seam. The spec's complaint about the padding is the location of the
missing field.

**F5 · HIGH · The resident drops words silently when the window fills.** `resident.cpp:313` and
`:339` `return` without a count when `npast_ + n >= n_ctx − 512`. That is a silently dropped percept
inside the loop — rule 7, SPEC 5.1.4 — with no counter and no status-line word. Stage 1b "does not
molt yet" is fine; dropping quietly is not. **Fix:** count `dropped_words_`, surface it, and *refuse
further ingest loudly* (or molt) rather than skip. Note the size of the problem: at n_ctx 8192 a
30 KB document is the whole window, and `docs/devlog.md` is 27 KB. The fold is Stage 2 critical path,
not a later nicety (§5.6).

**F6 · HIGH · Characters outside the BMP are mangled on input.** `edit.cpp:606-614` converts one
`wchar_t` per `WM_CHAR`. Windows delivers an astral character (any emoji, many CJK extension
characters) as two messages, a high and a low surrogate; each alone converts to U+FFFD. Typing 😀
inserts two replacement characters and the file is wrong. **Fix:** hold a high surrogate until its
low arrives, then convert the pair. The driver can test it by posting the two surrogate `WM_CHAR`s.
Related, LOW: `paint` and `col_of` assume one UTF-16 unit = one cell (`edit.cpp:514,534`); wide CJK
glyphs and combining marks will drift the caret. Document as a Stage 0 limit, or measure cells with
`GetTextExtentPoint32W`.

**F7 · MEDIUM · A long deletion's tail chunks read as typed text.** `Compiler::removed`
(`ingest.cpp:146-161`) prefixes the marker once and hands the whole body to `emit`, which chunks at
495 bytes; only the first chunk carries `(removed) `. A pasted-then-deleted page becomes one
`(removed) …` percept followed by several bare `[bo] …` percepts the trunk reads as newly typed. The
selftest at `selftest.cpp:764` strips the marker from every `'d'` percept and so cannot see it; its
removals are ≤ 20 bytes. **Fix:** put the marker on every chunk (and subtract it per chunk in the
arithmetic), or chunk deletions inside `removed`. Add a selftest with a 2,000-byte removal.

**F8 · MEDIUM · The 'f' final judge fires on every Delta, and a pad's Deltas are not lines.**
`Resident::feed` ends with `judge("f", …)` (`resident.cpp:357`), lifted from `fusord.cpp:746`, where a
Delta is a *complete line* from a bridge. In nib a Delta is whatever the compiler flushed: a closed
thought, 160 chars, or **500 ms of quiet mid-sentence**. A person who pauses half a second
mid-sentence costs three probes (~240 ms) on a fragment. This is the over-segmentation SPEC 5.1.2 was
corrected to avoid, reappearing at the quiet-flush grain. It is also a train ≡ serve concern of the
distributional kind (`OPEN_PROBLEMS` #4): the bytes match, the boundary population does not. **Fix:**
carry `kind` across the seam (F4) with a fourth value, *open fragment*, and only `f`-judge closed
percepts; fragments accumulate in `clause_` and the boundary set decides. Then **measure** boundaries
per minute on real typing at several T (§6.2). The `--resident` CLI feeds a synthetic 40 ms clock into
the compiler while the resident's 1500 ms flush law reads the wall clock; its 18 boundaries are on a
cadence no human produces.

**F9 · MEDIUM · `Doc::apply` records a whole-document inverse.** `doc.cpp:89-100`. Every resident
emission (Stage 2 goes through `apply`) will append an inverse the size of the document. A 100 KB
document with 300 emissions is a 30 MB log in RAM and a 30 MB tape on disk. Etherpad has `inverse`;
the port needs it before Stage 2 (the Etherpad report, §2.4 and §6.2, gives the algorithm and the
authorship consequence).

**F10 · MEDIUM · Identity is scattered across three vocabularies.** The document author is `"me"`
(`edit.cpp:231`); the ingest lane is `"bo"` (`edit.cpp:241-242`, hard-coded); the self-echo seat is
`"watcher"`; undo's author is `"undo"`. The father test (D7) needs a second person with their own lane
and colour, Act II needs peers, and the tape needs one identity per hand. **Fix:** one `Hand`
{id, lane, colour, is_resident} table, configured, used by Doc, Compiler, PadSource and the view. The
lane string stays train ≡ serve (`[bo]` is what v11 saw); the *mapping* is configuration.

**F11 · MEDIUM · Judgments are not anchored to the document.** `Judgment` (`resident.h:31-38`) carries
the clause text, not the revision it was judged at or the byte span it occupied. Floor control needs
both (§5.2): the dependency of an emission on the clause it answers is a span at a revision, and a
later keystroke either intersects it or does not. Add `rev` and `[a,b)` now, while the struct is young.

**F12 · LOW · `llama_backend_init()` is never called.** fusord calls it (`fusord.cpp:232`); nib does not.
Harmless in current llama.cpp (it initialises timing and NUMA), but it is a divergence from a lift that
claims to be verbatim; call it and `llama_backend_free()` on stop.

**F13 · LOW · Mixed line endings are silently unified.** `load_into` sets `crlf` if *any* CRLF exists
and saves every line as CRLF; a lone CR becomes LF. SPEC 4.3.2 promises no silent conversion. Bounded;
document it, or preserve per-line.

**F14 · LOW · The lane-change tick ordering.** `Compiler::typed` and `removed` call `push_pending`
before `maybe_tick` (`ingest.cpp:138-139,150-151`); the flush stamps `last_percept_ms_` with `now`, so a
silence that preceded a lane change with pending text is lost. Rare in the window (the 120 ms timer
flushes pending after T), real in the CLI. Swap the order.

**F15 · NIT · `sentence_cut`'s comment lies.** `ingest.cpp:31` says `"e.g. "` must not split a thought;
the code splits after any terminator followed by whitespace, so it does. That matches fusord's
token-final boundary set, which is the point — fix the comment, not the code. Also: a terminator
followed by a closing quote or bracket (`…no." Then`) does not close, so the thought closes late.

**F16 · NIT · Stale strings.** `nib.cpp:227` prints version `0.1.0`; the repo is 0.6.0. README says
"Status: blueprint (0.0.0). Nothing is built." `resident.h:60` and `resident.cpp:171,186` say the CPU
fallback is ~75× slower; the devlog, SPEC 6.0.1 and the commit say 47×. One number, dated.

*Found by the lift-map subagent, confirmed against the code:*

**F17 · HIGH · Idle ticks fire a full three-seat probe round.** The compiler emits the tick as an
ordinary percept (`ingest.cpp:77-95`); `nib.cpp:175` feeds every percept identically with no branch
on `kind`; `Resident::feed` ends with an unconditional `judge("f")` (`resident.cpp:357`). fusord
decodes a tick, reads the frontier, writes the row, and **never judges** (`fusord.cpp:709-719`) —
doctrine at `BLACKBOX_SPEC_v0.1_QC-001.md:7`, *anti-turn exemption 4: ticks never trigger a probe
round.* Harmless in 1b; in Stage 2 it means **silence itself can trigger an emission**, which is a
clock waking the mind — Fake 1 reborn inside the loop. Same root as F4: the kind must survive the
seam, and `feed` must branch on it.

**F18 · HIGH (doc, load-bearing for Stage 2) · SPEC 5.1.6 states half of the self-echo law.** The
gate half (never feed own speech back as *world to judge*) is what `PadSource::is_seat` does. The
trunk half — the emission **is committed to the trunk on the seat's lane** (`fusord.cpp:555-561`) so
the mind remembers having spoken — is absent from the spec, and without it the SKEPTIC re-fires the
same catch ten times (measured 2026-08-12) and say-it-once is *structurally unlearnable*
(`MERIDIAN_SIGHT_SPEC:142`, "L-GATE has no self exception"; the master one-pager's "no
self-exception" correction). Amend 5.1.6 to state both halves; §5.3 builds both. nib is a *room*,
not `--pure`; say so on the tape.

**F19 · LOW · `fusord.cpp:26-27` overstates that `soak --brief` reads its ledger unchanged** (three
silent differences, per the lift map). Not nib's bug, but nib's ledger claims should not inherit it.

### 3.2 The QC subagent's additional findings (companion: `docs/review/QC_NIB_REPO.md`, 1,118 lines)

**C-1 · CRITICAL · Vertical movement and mouse hit-testing treat byte offsets as columns; a
Down-arrow then a Backspace destroys a non-ASCII character.** `LineIndex::offset_of`
(`doc.cpp:179-184`) and `col_of` (`edit.cpp:198-201`) are byte arithmetic; the painter measures in
UTF-16 units (`edit.cpp:514-515, 534-535`). Down from column 1 onto `é` lands the caret *inside* the
sequence; Backspace's walk-back stops at the lead byte and removes it alone; the file on disk is
invalid UTF-8; and **`Ctrl+R` reports byte-exact**, because the log faithfully recorded the corrupting
splice — Stage 0's headline falsifier is structurally blind to this class. The same arithmetic drives
click placement and drag selection, and a split selection copied then pasted substitutes U+FFFD. The
random generators use `a`–`z` and `\n` only, so 120 checks cannot see it. **Fix:** a display-column
type distinct from the byte offset (walk UTF-8, count wide glyphs as two cells), routed through
`move_vertical`, `offset_at_point`, `col_of` and the painter; belt and braces, `Doc::splice` snaps
`start` and `start+ndel` to sequence boundaries and asserts well-formed text; and the 1,000-edit
property test re-run over a multi-byte alphabet with `utf8_valid(doc.text())` after every step.

**H-1 · HIGH · `--check`, `--unpack`, `--ops` are advertised in the usage, HANDOFF §4.5 and SPEC 3.2
and dispatched nowhere** (`nib.cpp:203-228`). `do_unpack`/`do_ops` are dead code; there is no
`check_rep` verb at all; `/W4 /WX` did not notice.

**H-3 (extends F5) · HIGH · At the context wall `feed` still appends to `clause_` and probes three
seats on a clause the trunk never saw** — judgments recorded against stale logits, the "wrong answer
that looks right" the CPU-fallback lesson names. Also `n_ctx − 512` is `int` arithmetic, so
`--ctx 256` makes the bound negative, nothing is ever ingested, and the run prints `0 words` with no
error. Clamp `--ctx`; refuse rather than return.

**H-4 · HIGH · `Doc::apply` leaves the existing undo groups in place** (`doc.cpp:89-100` closes the
open group but does not touch `undo_`). A foreign changeset that replaces text with text of the same
length — the most likely Stage 2 emission — passes the length guard, and the person's next Ctrl+Z
applies inverses at offsets that no longer mean what they meant: silent corruption. `Doc::apply` has
**zero test coverage** and is the function Stage 2 depends on from its first character. **Fix:** clear
both stacks on `apply` (honest: a foreign change ends what you could take back) or transform pending
inverses through the incoming changeset with `follow`; say which in SPEC 2.3.5; test it.

**H-5 · HIGH · A failed undo or redo pops the group first and leaves the document half-changed**
(`doc.cpp:102-143`): no rollback, group gone, nothing on the redo stack. Validate the whole group
against a scratch copy, pop only on success.

**M-1 · MEDIUM (product-visible) · The process is `PROCESS_DPI_UNAWARE`.** No manifest, no
`SetProcessDpiAwarenessContext`, no `/MANIFEST`; `GetDpiForWindow` returns 96; `WM_DPICHANGED`
(`edit.cpp:592-599`) can never be delivered. SPEC 4.1.2 **[BUILT]** is false as built, and on this
225 % box the window is bitmap-scaled. One call at startup, or a manifest.

**Also from the QC**, with fixes in the companion: `Doc::set` assigns `text_` directly, bypassing
`push` and `check_rep` (SPEC 2.1.2's "only mutators" claim is ~); no `WM_QUERYENDSESSION`, so
shutdown or logoff loses work despite the close prompt; `dec()`'s return value is discarded at all
three call sites; `--resident` is non-reproducible by construction (M-11) and can judge from stale
logits (M-12); `build.bat` checks `source.h` but not the transitive `core/ring.h`; **nothing asserts
the `/DELAYLOAD` flags** — drop one and the build prints OK while `--selftest` stops running on a
model-less machine (the one refusal in this build that is not mechanically enforced; three lines of
`findstr` or an `--about` that runs the battery with an empty DLL directory fixes it); SPEC 3.1.1's
"byte-identical to Etherpad" MUST is **false for any non-ASCII edit** (`é` → nib `Z:0>2+2$`, Etherpad
`Z:0>1+1$`) and every vector is ASCII so nothing can see it — §4b's decision closes it; SPEC 9.4's
egress counter is not built and is unmarked, so either build it or mark it [SPECIFIED].

**Port fidelity: excellent.** Function by function against `Changeset.ts`, `Op.ts`, the three
assemblers, `ChangesetUtils.ts` and `Builder.ts`: no semantic divergence in the canonical-form
machinery, including faithful reproduction of Etherpad's own missing `=` bound check in `checkRep`.
And the QC diffed `resident.cpp:19-68` against `fusord.cpp:103-156` line by line — identical; the
probe genuinely forks the KV (`seq_cp TRUNK→DECIDE`) as fusord does. "The `serve_hash` pin is
genuine mechanical proof, and it is the strongest thing in the repository."

### 3.3 Doc-vs-doc drift [I]

| where | says | truth |
|---|---|---|
| README | 0.0.0, nothing is built | 0.6.0, Stages 0a–1b |
| SPEC header | Rev 0.4 · 2026-09-03 | contains 2026-09-04 corrections; every commit is 09-04 |
| SPEC 11.1 | `[BUILT: 71 checks]` | 120 |
| HANDOFF header | written 2026-09-03, Stages 0a–0d | covers 1a/1b of 09-04 |
| HANDOFF §4.3 vs §5 | 120 + 22 checks vs "run both oracles: 77 / 17" | 120 / 22 |
| ROADMAP rows 0a–0d | ✔ 2026-09-03 | 09-04 by git |
| devlog headings | 2026-09-03 (evening/night/late) | 09-04 by git; the session's start date carried through compaction |
| SPEC 6.0.1 / devlog vs resident.h/.cpp | 47× vs ~75× | one measured number: 556 s vs 11.8 s = 47× |
| SPEC 5.1.9 / ingest.cpp:78 | tick "byte-identical to fusord.cpp:712" | false as implemented (F4) |
| SPEC 5.1.1 / ingest.h | `source.h` used "unmodified" | true, and the reason F4(b) must be an auricle change |
| BLUEPRINT §2 | document model = op log with character identities | superseded by ASSEMBLY (Easysync); BLUEPRINT should carry a supersession note at §2 and §7, as ASSEMBLY's header already claims |
| CLAUDE.md rule 2 | `0 bytes egress` structurally true | of the exe; not of the process (F1); and the status line carries no egress counter at all (SPEC 9.4 unbuilt) |
| CLAUDE.md rule 2 | four network DLLs named | the build and SPEC 9.1 check five (`dnsapi`); the rule text is behind its own gate |
| nib.cpp usage | "Stage 1b: the resident computes hold/emit and records it" | fine; version string stale; `--check`/`--unpack`/`--ops` advertised, undispatched (H-1) |
| SPEC 11.4 | `[BUILT]` — 17 checks | 22 |
| SPEC 2.2.2 | one keystroke moves one character, never one byte | false for Up/Down and mouse (C-1) |
| SPEC 4.1.2 | `[BUILT]` per-monitor DPI aware | false: `PROCESS_DPI_UNAWARE` (M-1) |
| SPEC 3.1.1 | a nib and an Etherpad changeset for the same edit are byte-identical | false for any non-ASCII edit; decided in §4b |
| SPEC 2.1.2 | `splice` and `apply` are the only mutators | `Doc::set` assigns the text directly |
| SPEC 5.1.9.1 | the lifted loop must run with `--idle-tick-s 0` | no such knob exists; restate as a prohibition (what was actually done) |
| ASSEMBLY §4 | `kPayloadMax = 496`, a Delta is 512 bytes, chunk at that boundary | both numbers corrected on 09-04 (528; 495); ASSEMBLY is reading item 4 and still hands a new session the wrong ones |
| BLUEPRINT §4 | "a word boundary" is a percept trigger | corrected in SPEC 5.1.2 the same day; BLUEPRINT governs intent and still says it |
| HANDOFF §4.5 | `--check`, `--unpack`, `--ops` documented as commands | undispatched |

---

## §4 · The estate around nib — what nib inherits, what it contradicts, what it settles

**Inherits, and the code shows it:** decode-on-delta; ticks-as-world; the byte-frozen seed with the
hash as a run that fails; the three-seat dial-0 probe; the flush law (boundary ∨ 24 tok ∨ 1500 ms); the
gate rule under backlog; the back-pressure corollary (count, never drop); self-echo at the source; the
insistence that hazards be unreachable by construction. The lift is proved by `serve_hash()` equalling
fusord's pin, which is the right kind of proof.

**Contradicts, deliberately, and should say so in CLAUDE.md:** Addendum F §2.1 — *"the mind never runs
inside any host process's frame budget."* nib runs the model in the editor's process (rule 1: one exe).
The law's substance is about scheduling, not address space: userland is scheduled, never scheduling. nib
satisfies that if and only if the resident owns its own thread and clock, the editor never calls the
model, and the two meet only at the SPSC ring. Write that as rule 11. The price of one process is that a
CUDA driver fault takes the editor with it; the mitigations already exist (forming text never persists;
atomic save; a tape durable as it goes) and should be named beside the rule.

**Settles, for the estate:** `OPEN_PROBLEMS` #7 — *a resident that cannot resume is not resident* — is
solved in a pad for free, because the tape of percepts is a deterministic fold and prefill is fast:
restart = replay the tape into a fresh trunk (§5.6). `OPEN_PROBLEMS` #9 — *is silence perceived?* — is
testable in nib this week: run `--resident` on a stream with and without `[tick +Ns]` lines and compare
the SPEAKER margin on the line after the gap. `OPEN_PROBLEMS` #8 — *zero labels* — is answered by the
pad's ordinary editing (§5.4): deleting the resident's block is a dismissal, and it is also a percept.
`THE-NEEDLE`'s "the product is the record of silence": nib's tape *is* that record for writing, and it
is the first venue where the lane contract meets a human's hands with no bridge in between.

**The distributional caveat the whole thing rides on.** v11 was tuned on `[dana] …` work-chat lines;
the seats' mandates are a watch-room's (address, contradiction, risk). A pad of prose is a new lane. The
bytes are identical; the boundary population and the content distribution are not. Stage 1b shows the
margins are *sane* on prose (a false claim moves the SKEPTIC twelve logits). It does not show the fire
*rate* is livable at pad grain. That is the first thing Stage 1c must measure, and it is why the label
valve must exist before the retune trigger (D3) can be evaluated.

Two expectations the estate has already measured, to be written down before Stage 2 so they cannot
be discovered as surprises: **SENTINEL will dominate** (90 % of fires from the vaguest mandate across
five regimes — soak 1,630 / 178 / 9, `UNPROMPTED_AMENDMENTS:24`; mandate width buys volume, not
conviction), and fusord's post-manners surfaced rate was **31.2/hr against a 6/hr budget**
(lift map §6.8). That gap is where Stage 5's falsifier is decided, the vise says a runtime dial will
not close it, and the honest Act I move is to print which seat spoke on every `emit` row and let the
week decide whether SENTINEL earns its mandate on a writing surface. And one instrument law for the
gutter and the forming text (§5.3–5.4): **margins are meaningful only near contention** — in the deep
tail they measure phrasing, not judgment (surface variance 5.94× meaning), so brightness must
saturate below zero rather than imply that −11 and −4 are an ordering.

---

## §4b · What the Etherpad read settles (companion: `docs/review/ETHERPAD_DEEP_READ.md`)

**The differential harness exists and runs with nothing installed.** Node 24's native type
stripping plus 74 lines of stdlib `module.registerHooks` load the real `Changeset.ts` from
`C:\etherpad-develop` untouched; Etherpad's own generators, made reproducible by swapping
`Math.random` for a seeded PRNG around them, produce cases; a checker compares byte for byte.
Measured this session [M]: `node gen.mjs 7 > cases.jsonl && node check.mjs cases.jsonl` →
**66/66 matched**, hash-stable per seed, and a one-byte corruption is caught (65/66, exit 1). The
harness is copied into `C:\nib\tools\etherpad_harness\` and re-ran there: 66/66. Stage 0f is done in
prototype; what remains is `nib --diff` emitting the same JSONL from a C++ port of Etherpad's
`randomTestChangeset` generator (SPEC 3.3.4 becomes a pre-commit gate, not a wish).

**Authorship is one ordinary attribute.** Key `author`, value `a.` + 16 chars; the pool numbers
attributes in insertion order and **emits them sorted by key** — nib will not be byte-identical
without that sort. Attribute conflicts under `follow` converge on the lexically earlier value, so
choosing id prefixes (`a.h…` for hands, `a.r…` for seats) turns a policy into a structural fact.
Minimum implementation, ~600 lines in dependency order: `AttributePool` → attribute-string codec →
`AttributeMap` → `compose_attributes` + the zipper → `atext` and `apply_to_attribution` → wire every
insert with `[["author", id]]` → **`inverse`**.

**Derive inverses; never persist them.** The timeslider's one structural lesson: Etherpad stores
only forward changesets and computes `inverse(cs, lines, alines, pool)` for both undo and time
travel. nib persists an inverse per revision (`doc.h:23`), which is *why* its inverses are
whole-document splices — and a whole-document delete-and-reinsert **re-authors the entire document
to whoever pressed Ctrl+Z** the moment authorship exists (report §6.2). F9 is therefore a
correctness bug in waiting, not a size note. Measured on the real library: undo of a delete
re-inserts with the *original* author's attributes; undo of an insert emits a bare `-`.

**Keep bytes.** `chars` counts UTF-8 bytes; Etherpad counts UTF-16 code units; `|lines` cannot
diverge. Wire interop with a real Etherpad server is *impossible* on non-ASCII, not degraded, and
Act I does not need it; code points buy neither interop nor simplicity and are rejected. SPEC §14.1
closes with that decision and a named future adapter (byte ↔ code-unit offsets over the current
text, re-`pack` at the wire).

**The forming region has exactly one precedent, and it is the right shape.** Etherpad has no ghost
text and no suggestion layer; the only uncommitted text is the IME composition guard: *while
composing, build no changeset and send none.* That is §5.3 — the overlay produces no changeset until
commit, which makes rule 3 structural rather than disciplinary.

**Divergences to fix or record** (all with line numbers in the report): `make_splice` clamps a
negative index where Etherpad throws, and both comments claim the opposite of the truth
(`changeset.cpp:335-338`, `changeset.h:115`); `deserialize_ops` silently eats a partial `*N`/`|N`
before `$` (`changeset.cpp:82-98`); a stray `\n` in the ops region is skipped by Etherpad and refused
by nib — keep nib's, record it so the harness's "both refuse" rule does not flag it;
`MergingAssembler::clear()` is correct where Etherpad's leaks a stale count (`"=4=1"`); `LineIndex`
counts one more line than Etherpad's line model for a `\n`-terminated document, which matters the day
`mutate_attribution_lines` is ported; and nib has **no trailing-newline invariant**, which an
Etherpad-shaped host requires — decide it before the serializer is written.

**The Act II protocol is written out** (report §2.8): host per document with `atext, pool, head,
revs`; `USER_CHANGES {baseRev, changeset, apool}` → `ACCEPT_COMMIT` / `NEW_CHANGES`; the client's
two-slot state machine with its asymmetric `follow` flags (`true` for local pending, `false` for the
remote change); and the trap that retransmission detection compares the post-`moveOpsToNewPool`
form. nib's floor rule sits *on top of* the serializer, not inside it, and does not excuse a wrong
`follow`: two humans still collide.

---

## §5 · The design at the edge

### 5.1 One mechanism, three uses: the floor is a dependency test in op-space

The fabric kills a forming branch when a committed rev's `contra` set intersects the branch's `dep`
set — a set test on rev ids, no payload parsed (`fabric.h:commit_one`, the contradicts∩dep). syncytium
proves it fires emergently between three seats. **Nobody has applied it to the human's keystrokes, and
the op log makes that free.**

Every emission the resident composes was conditioned on a specific piece of the world: the clause(s)
it judged, which occupy a byte span `[a,b)` of the document at revision `R`. That is its **dep**. Every
human changeset after `R` either intersects the span (after transforming it through the intervening
changesets — trivial arithmetic for a single serializer, and Etherpad's `follow` in Act II) or does
not. **A human edit inside the dep span is a contradicting rev, by construction.** No semantics, no
model, microseconds. From that one rule:

- **Refuse before composing** (SPEC 6.3.2). The floor window is the dep test applied *before* the
  branch opens: if any human op touched the target block within `floor_ms`, the emission is refused,
  the tape records `refused {seat, margin, reason: floor}`, and the seat keeps its want (it may re-fire
  at the next boundary under the existing re-arm rules). The 2 s window is the pad's
  transition-relevance place: two seconds without a keystroke in the block is the human yielding the
  floor, which is the same cue conversation analysts measured in speech (§6.3).
- **Pause or kill while forming** (Stage 3, the structural un-say). The forming branch declares its
  dep. A human op that lands *inside the dep span* kills it: `seq_rm`, the characters withdrawn, the
  tape gets `abort {formed, aired, why: dep-touched, rev}`. A human op that lands *elsewhere* does not
  touch the branch — the person kept writing, the resident keeps forming — but the new percept is
  ingested on the trunk and judged at the next boundary, and if the speaking seat's margin has
  collapsed, that is the **semantic un-say** (`abort {why: margin, m_before, m_after}`). Two paths,
  both mechanical, both on the tape with a reason a reader can check.
- **Never write inside a human's paragraph** (SPEC 6.3.1) is the same test with the target set to the
  block itself: the resident's block declares dep on the block it follows; a human keystroke inside
  that block is a contra. The rule the spec states as manners is now a serializer property.

Two things fall out that the spec did not have. First, the un-say has a *precise* trigger the demo can
show: "your next keystroke contradicted it" means literally *you edited the sentence it was answering*.
Second, the human learns the mechanism without being told: write on and the machine holds its words;
go back and change what it was replying to and it takes them back mid-word.

**Negotiated, with mediated as the floor.** The interruption literature (McFarlane 2002, thirty-six
participants) compared four coordination methods — immediate, negotiated, mediated, scheduled — and
negotiated won. nib as specified is *mediated*: a serializer decides on the human's behalf with a
constant the human never sees. Immediate is correctly forbidden; scheduled is correctly absent (it is
the poll). The upgrade is to make the resident *announce* through the gutter (§5.4) and let the
human dispose: keep typing (defer), pause (yield), enter the block (accept), delete (reject). That is
FUSOR's own propose/dispose idiom, so it costs nothing conceptually. The 2 s constant stays as the
hard floor; it is also, independently, the standard burst threshold in keystroke-logging research
(2,000 ms, Wengelin 2006), which is worth a devlog line — same number, two derivations.

**Count cues; do not test one.** Duncan (1972) showed turn-yielding cues are *additive*: the
listener's attempt probability rises with the number displayed at once, and one attempt-suppressing
cue zeroes it. The pad's cues are countable from the op log: terminal punctuation at a word end, the
pause bucket, a paragraph break, the caret leaving the paragraph, focus loss. Continued typing inside
the window is the suppressing cue. And pauses are not one thing: within-word, between-word,
end-of-sentence and end-of-paragraph pauses differ by an order of magnitude in resumption cost
(Iqbal & Bailey), so the floor window should be a *breakpoint hierarchy* — longer or more intrusive
emissions require coarser breakpoints — rather than one constant. Both are Stage 2 configuration
that the tape can then tune (§6.4's takeover curve).

**The estate's doctrine binds the trigger, and it converged on this mechanism independently.** The
lift-map subagent, reading the auricle tree without this document, concluded: *"nib's trigger must be
`contradicts ∩ dep` over block ids — the same set as the floor rule, two moments."* Two laws it
carries: `FUSOR_HARNESS_SPEC.md:123` **forbids surprisal as a gate** (measured inverted on 2026-08-12;
"recalibration is dead as a fix — do not retry it"), so the nerve is logged and disclosed and never
kills; and *only commits kill; forming evidence may only pause* (`fabric.h:203-212` soft-pause vs
`274-285` hard-kill). In a pad every keystroke is a commit — a changeset — so the human side has no
"forming" object and the pause has nothing to attach to, except IME composition, which Etherpad
already treats as "build no changeset yet" (§4b). The semantic re-probe (a seat's margin collapsing
at the next boundary) is not surprisal and is permitted; it is the same judgment that started the
sentence, asked again. And the estate's rule that **barge-in is a percept, not a cancel** fixes the
order of operations: the killing keystroke enters the trunk *first*, then the branch dies, so a
re-form is informed rather than retried.

### 5.2 What a judgment must carry, and what the tape must record

`Judgment` gains `rev` (the document revision at the boundary) and `span` (the byte range of the
clause at that revision). The compiler already knows the span — it is the splice it just compiled —
so `Percept` carries it too, and the resident copies it through. An emission then declares
`dep = {rev, span}` at `open_branch`, and every human changeset is tested against it on the way in.
This is the cheapest change in this document and it unlocks everything in §5.1.

### 5.3 The forming region is a fourth plane of the buffer and is not in the log

Structural non-persistence, per rule 3: the view holds `Forming {seat, anchor_rev, anchor_offset,
text, branch_id, formed_at}` beside the `Doc`, and the `Doc` never sees it. It is rendered in the
seat's colour at the dim tier (the HUD doctrine: certainty is brightness, hue is role, `hud.h`), anchored
to a committed offset that is transformed forward through every human changeset that lands before it.
Save writes `Doc::text()`; a crash loses the overlay; `Ctrl+R` replays the log and the overlay is not
in it. On **commit** the region becomes one `Doc::apply` with the seat as author — one revision, its
own undo group (the author clause of 2.3.4 already guarantees it), a `[SEAT] ` tag at the head of the
block (§5.7) — and the tape gets `emit {seat, margin, rev, formed, aired, gen_ms}`. The resident then
commits its own line to the trunk on its seat's lane (`fusord.cpp:555-561`; without it the SKEPTIC
re-fires ten times), and the self-echo filter keeps that line from re-entering as world. On **abort**
the overlay is cleared and the tape gets `abort` with what was formed, what reached the air, and why.
In a pad "reached the air" is "was rendered before the kill", which is all of it unless rendering lags;
define `aired` as the rendered prefix and record both.

**The brightness of the forming text is the speaking seat's live margin.** That is a real internal
state rendered, not a tell; it is permitted by rule 5 and it is the pad's only legitimate "hesitation".

### 5.4 The gutter is the continuer, and the label valve is ordinary editing

Conversation analysis has a class of act that never takes the floor: the continuer (*mm-hm*). The
estate forbids fake ones. A real one exists: **the margins.** Render, beside each block, a mark whose
brightness is the highest seat margin at the last boundary in that block. No words, no animation that
is not a state change, and the human learns to read it the way they read a colleague's face. It also
makes floor negotiation two-sided: seeing the SKEPTIC's mark brighten, a writer can *yield* by pausing —
which is exactly the floor window — or keep the floor by typing. This is Stage 1c (§8) and it is the
first moment the operator writes with something present, before a single word is emitted.

Labels accrue from editing, not from a dialog: deleting the resident's block within N minutes is a
**dismiss** (and it is also a `(removed)` percept on the human's lane, so the mind perceives its own
words being thrown out — the no-self-exception law, delivered by the pad for free); editing it is
**edited**; leaving it is a weak **accept**; an explicit key is a strong accept. The tape records the
label with the emission's id. This is `OPEN_PROBLEMS` #8 answered without a review pass, and it is the
v12 corpus (D3) accruing from day one.

**The continuer is a third emission class, and it gets a law.** Below `emit` and above `hold`: a
rendered state that by construction can never become a document character. Rule 5 permits it because
it *is* an internal event (a held, margin-positive judgment). It is also the resolution of a
critique the interruption literature will make: negotiated interruption needs an announcement, and a
resident that may only be silent or fully speaking has no way to announce. Cursor's Tab model gives
the arithmetic for what the announcement buys — an explicit *no-suggestion* action with reward
+0.75 accept / −0.25 reject / 0 silent, so showing is positive-EV exactly when p > 0.25 (Horvitz's
1999 expected-utility rule, made trainable) — and its result was 21 % fewer suggestions with a 28 %
higher accept rate. nib has no accept key, and does not need one: **survival of the resident's
characters at one hour, by character identity, is p**, measured exactly, and the gate's margin
threshold can be checked against it on the tape.

### 5.5 The two switches, made exact

**AI on/off.** Off means the resident thread stops, the trunk is freed, **the model is unloaded and
the card is returned**. Not muted, not paused: no context held is only true if VRAM is released. On
costs the load (11.6 s [M]) plus a re-fold of the tape into a fresh trunk (§5.6), which is what makes
off honest and on cheap to trust. Both transitions are tape rows with the model hash.

**RESIDENT / TURN-BASED — two twins, named.** In TURN-BASED the human presses a key to be answered;
the seat, seed and sampler are identical and only the trigger differs — the **T1** twin. But the
boundary judgments keep running *in shadow* (ledger only, never surfaced; `fusord --pure`'s stance, and
hop-0's `shadow` position), so every TURN-BASED period yields, at every boundary, what the resident
would have done beside what the twin did on the key, and the human's reaction to the twin's line is
the label. That is the paired record BLUEPRINT §3 wanted, at boundary grain rather than per flip. The
address key exists in both modes (in RESIDENT it is simply an address — the SPEAKER's mandate).

The **T4** twin — D1's four treatments: no decode-on-delta, no persistent KV, no mid-decode abort, no
boundary-grain judgment, re-prompted from the transcript on a policy — is not a mode. It is a
**replay**: `nib --twin <tape>` re-drives the recorded stream through a turn-based policy offline, on
the same weights, and prints paired emissions with their latency deltas. The 65.7 s number becomes a
number on writing, reproducible from any tape, with no live experiment. D1's parity clause becomes
enforceable: the replay shares every non-residency scaffold in identical code (repeat suppression, the
28-token cap, coarsening), and a table of which scaffold fired in which arm prints with every run.
Enumerate the scaffolds now (`OPEN_PROBLEMS` #2): repeat suppression, generation cap, floor window,
coarsening, the 6-token minimum, the dep test. The dep test is residency; the rest are common ground.

The real twin to lift is `blackbox.cpp:1243-1354` (a full re-prefill from position 0 per wake, the
same seed, sampler and cap; `starship.cpp`'s twin is a canned string). Three disciplines from it:
both arms receive the tick (`blackbox.cpp:1599-1603` — the one site that once quietly handicapped
the control arm); per-arm config honesty (a dial the turn arm never used is `null` on its row, never
a number); and mid-composition world that the turn arm was blind to is *counted* (`twin_blind_ct`) —
the product claim in one integer. And one caveat that goes on the tape and in every write-up:
**nib's pairs are observational, not matched-input.** blackbox forks byte-identical decks to both
arms; in nib the human types different things in the two modes, and the mode changes what they
type. A within-subject, unblinded, n = 1 design is the weakness; the toggle is still the right
instrument, and the replay twin over the recorded stream is the matched-input half.

### 5.6 The document is its own memory: resume and the fold

A pad has what fusord never had: the world it perceived is on disk, in order, as an append-only tape
that is a deterministic fold. Therefore **restart is a replay**: seed the trunk, then re-decode the
tape's percepts at prefill speed (thousands of tokens per second on this card [BUDGET]); the resident
wakes with the day it was resident for. `OPEN_PROBLEMS` #7 closes. Optionally, on close, persist the
trunk with `llama_state_seq_save_file` (`llama.h:859`) for an instant resume; measure the file size
first (q8_0 KV at 8k is a few hundred MB [BUDGET]).

The **molt** is lifted verbatim from `fusord.cpp:599-650` (scribe seq, the `[memory]` rung, the
`[recent, verbatim]` tail, the tick that names the outage) and it is Stage 2 critical path (F5). At
n_ctx 8192, keep fusord's ratio rather than its numbers: the watermark 24576/65536 = 0.375 becomes
**≈ 3072**, the rung shrinks from 600 to **≤ 256 tokens** and the tail from 600 to **≤ 200 words**,
and the reseed size (seed ≈ 430 tokens + rung + tail) is *measured* before it is trusted. The reseed
re-decodes `SEED_SYS + SEED_EXAMPLES + SEED_OPEN` verbatim (`fusord.cpp:629`) — the one runtime
re-assertion of train ≡ serve — force-judges the open clause first (reason `m`), keeps the resident's
own lines in the tail so it does not forget it spoke across the fold, emits the outage tick, and puts
the full rung on the tape. In a pad the human is typing *during* the fold; percepts queue on the ring
and are ingested after, so the molt's cost is a **latency spike, not a gap** — measure it as one. Two
pad-native experiments ride beside it, both **[BET]** and marked as departures from the lift: (a) the
document's current text as the raw material of the memory rung, since it is the ground truth of what
stands; (b) post-molt recall probes (the estate's F-PERSIST form) on planted facts, molt-from-scribe
vs molt-from-document. The reseed *format* stays byte-identical; only the rung's content is in play.

n_ctx is a measured decision, not a default: 8192 is the whole window for a 30 KB file. Measure the
q8_0 KV cost at 16k on this card with llama-server co-resident before Stage 2, and put the number in
the ROADMAP with its date.

### 5.7 The file is a valid lane stream

Resident paragraphs begin with `[SEAT] `; human paragraphs are bare. The .txt is then byte-for-byte a
lane-headed stream in the tune's own format, rule 6 holds in the file, and opening a file restores
lanes by parsing the prefix at open time only (a human typing a literal `[SKEPTIC] ` later is a spoof
the self-echo filter will simply drop — accept it, and note it). Opening a document *is* the resident
reading it: the open-move splice is compiled as `[doc-author]` percepts, or, better, when a tape exists
beside the file, the tape is replayed instead, because the tape has deletions and order and the file
does not.

### 5.8 One `Hand` table

`{id, lane, colour, kind: human|seat}`. The document's author strings, the compiler's lanes, the
self-echo set, the view's colours and the tape's author fields all read from it. `bo` stays `bo`
because v11 saw `[bo]`; a second person is a second row; the seats are rows with `kind: seat`. The
father test (D7) becomes a configuration change.

### 5.9 Interleaved ingest and generation: the blind window closes

fusord's generation is a blind window bounded by construction (28 tokens, `fusord.cpp:484-503`). A
pad can do better, because syncytium already showed the batch layout: one `llama_decode` can carry
trunk tokens on `seq 0` and the forming branch's pending token on `seq 6` in the same forward pass
(`syncytium.cpp:328-343`, the 1.208× law). So the resident keeps *perceiving while it speaks* — new
percepts land on the trunk, the dep test runs per keystroke, and the semantic re-probe runs at the next
boundary — and the forming branch is killed or allowed to finish on evidence rather than on a token
cap. The forming branch itself does not see the new tokens (it is a fork at 0 MiB), which is correct:
the sentence was conditioned on the world at fork time, and deciding whether that world still stands
is the serializer's job, not the branch's.

A third un-say mode follows and should be built only after the first two are measured — **the pivot**
[SPEC]: on a contra, instead of killing, re-fork the branch from the *new* trunk, re-decode the
already-formed prefix (~20 tokens, one batch), and continue; the sentence changes course from the
point of divergence, and the tail that was withdrawn is on the tape as an abort of a sub-span. It is
cheap and it is the kind of act a colleague performs. Rule 5 governs it absolutely: the pivot must be a
real re-decode from a real contra, never a flourish.

### 5.10 Pacing, stated so it cannot be faked

Emission renders at the sampler's real cadence; no throttle exists that is not tied to a measured
internal state. Two real states may slow it: the card is shared with ingest in the same batch (real),
and a thin margin causes a re-probe at every new percept (real, ~80 ms each [M: 74–87 ms per probe]).
Neither is decoration. At ~30 tok/s [BUDGET, unmeasured on nib] a twenty-word line forms in about a
second, which is visible and honest.

### 5.11 The latency budget, as a table to be measured

| leg | what | today |
|---|---|---|
| keystroke → repaint | the editor alone | [BUDGET] unmeasured; must not move when the resident is on (Stage 1c falsifier) |
| keystroke → percept | compiler: boundary ∨ N ∨ T | T = 500 ms provisional [OPEN 14.3] |
| percept → judgment | three probes at a boundary | 222–260 ms [M 2026-09-04, shared card] |
| judgment → first character | cue prefill (~50 tok) + first sample | [BUDGET] |
| character cadence | the sampler | [BUDGET] ~30 tok/s |
| contra → withdrawal (stop latency) | dep test + `seq_rm` + repaint | [BUDGET] end to end, unmeasured; the `seq_rm` alone is microseconds (`m0_demo.cpp:177-181`, the branch-drop cost only, never the headline) |
| model load / resume | load + tape re-fold | 11.6 s [M] + [BUDGET] |

Levinson's turn-transition band in speech is ~200 ms; nib's judgment-to-first-character sits in the
same band on a free card and a few times it on a shared one. That is the physical reason the surface
will read as a person, and the reason the co-tenancy floor (Addendum B: probes 0.6–24.6 s on a
desktop-loaded card) must print on the status line rather than hide.

### 5.12 Act II and III, briefly, because Act I carries the thesis

Act II: a host serializes (ASSEMBLY §3); peers' changesets arrive at Etherpad's own ~7-char/500 ms
grain, which is already percept grain, on their own lanes; the dep test generalises to N humans
unchanged; the tape's claim weakens to *what this mind perceived* and the product must say so; the
network gate narrows to no-HTTP, no-DNS, subnet-only by construction, and the runtime module gate from
F1 is what makes that checkable. The Etherpad report (§2.8 on disk) gives the client/server protocol
to copy. Act III: three seats co-decode on one trunk at 1.208× — the seat abstraction is already in
`resident.cpp` with three seats in it; syncytium is the worked example.

---

## §6 · Falsifiers and measurement — Stage by stage, with the metric family nib should adopt

### 6.1 The stages, re-cut

The current order goes 2 → 3 → 4. Emission is the most dangerous thing in the project, and the safe
default while it is being built is the switch that turns it off. So:

- **1c · the wire** [SPEC, days]. The resident on its own thread inside the window; judgments back
  over a second SPSC ring; the gutter (§5.4); the AI switch with the model unloaded on off; the tape
  v0 (§6.4) recording changesets, judgments, switches, model hash, mandate; resume by re-fold.
  *Falsifier:* keystroke-to-repaint moves measurably with the resident on (the world dilated for the
  mind); a judgment's clause is not byte-identical to the compiled percept; AI-off leaves VRAM held.
- **2 · emission in blocks, with the structural floor** [SPEC]. §5.1–5.4, the fold, the label valve,
  `Doc::apply` with a real inverse. *Falsifier:* an emission lands in a block a human touched inside
  the window; forming text survives a save or crash; an emission without a `dep` on the tape.
- **3 · un-saying, both paths, interleaved** [SPEC]. §5.1, §5.9. *Falsifier:* a kill without a recorded
  contra or margin flip; residue in the buffer; a saved file containing an un-said word.
- **4 · the two switches, the shadow twin, and `--twin`** [SPEC]. §5.5. *Falsifier:* the arms differ in
  seat, seed or sampler; a scaffold present in one arm and absent from the other.
- **5 · the week** — and **5b · the father test** (D7): same binary, another person's machine and
  writing, not told what it catches. *Falsifier:* "did you leave it on" answered no; or the fire rate
  lands in a different band for the second person, which is the fit-to-Bo result.
- **0f · the differential harness** against the real `Changeset.ts` — running today from
  `tools/etherpad_harness` with nothing installed (§4b); what remains is `nib --diff` and the
  pre-commit wiring, before `compose`/`follow`, i.e. before Act II.

### 6.2 What to measure first, next session, on a free card (with permission)

1. **Boundaries per minute vs T** on a real typing tape, N ∈ {80, 160, 320}, T ∈ {300, 500, 1000,
   1500} ms — the missing number behind SPEC 14.3 and F8. Zero model calls for the first pass
   (`--ingest`); one `--resident` pass for the probe cost.
2. **Do deletions move the margins?** A scripted stream with `(removed)` lines vs the same stream
   without; SKEPTIC/SENTINEL margins on the following boundary. Twenty minutes; decides the marker's
   wording (SPEC 5.1.10, OPEN).
3. **Are ticks perceived?** `OPEN_PROBLEMS` #9, same protocol, ticks on vs off.
4. **KV cost at 16k q8_0** with llama-server co-resident; the number goes in the ROADMAP.
5. **Module listing after backend load** (F1).

### 6.3 What the fields nib borrows from would demand (companion: `docs/review/PRIOR_ART_FLOOR_CONTROL.md` §9–§10)

Four fields each own a piece of this problem and each will ask a specific question first:

- **Full-duplex speech** will ask for **un-say precision**. The 13 µs kill is a latency for a decision
  of unmeasured accuracy; a fast wrong retraction is worse than a slow right one. The field's name
  for the latency is **stop latency** (Full-Duplex-Bench v1.5; HumDial 2026), its scenario is
  *negation/dissatisfaction*, and EchoChain benchmarks the semantic half — whether the abandoned plan
  is replaced by a correct one. Adopt the vocabulary and the four-way coding (Respond / Resume /
  Uncertain / Unknown); never print 13 µs without precision beside it.
- **CSCW** will ask why there is a floor at all, and will predict, from territoriality research
  (writers defend paragraphs against *improvements*; writers copy text to private files to escape
  being watched mid-formation), that the Stage 5 falsifier fires for reasons unrelated to model
  quality. It will also point out that the CHI '26 one-week N=30 deployment of agents in shared
  documents found teams pushed agent output into comments and did not treat agents as members. That
  study is the methodological template for Stage 5 and the prior nib must beat.
- **Co-writing UX** will ask whether the emissions were any good (acceptance, not persistence, drives
  perceived productivity), whether felt ownership dropped (a daily one-item scale is cheap), and
  about latent persuasion (Jakesch et al. 2023): an owner-set mandate with no visible instruction,
  permanent presence and accreted context is the *aggravated* case. nib's honesty laws cover
  provenance, not influence; say so as a standing limitation.
- **Conversation analysis** will say the thought boundary is detection after the fact, not
  projection — a TRP is *projected* before completion — and that un-saying is self-repair, which has
  a positional grammar and a preference structure; a retraction with no repair format will be read
  as an action with a meaning nobody designed. Two cheap answers: score projected closure (the
  boundary mass already does this, at 0.97/0.02) rather than detect punctuation; and give the
  withdrawal a minimal, honest shape (the seat's gutter mark dims as the words go).

### 6.4 The tape, and the metric family for Stage 5

**The family format is settled, SPEC 8.1.1 is correct, and it is achievable today** (lift map §5,
verified on disk). The normative source is `C:\REGISTRAR\core\tape.py`; glance, caseclock and fray
are ports of it. Every row is **exactly six keys**, written canonically so the on-disk order is
`at, body, digest, kind, prev, seq`: `seq` from 0 and contiguous; `kind` free-form; `at` an int64
(nib: milliseconds since the session's `t0`, which goes in the header); `body` an object; `prev` the
previous digest, genesis 64 ASCII zeros; `digest = BLAKE2b-256(prev_hex ‖ 0x00 ‖
canonical({at, body, kind, seq}))`, lowercase hex, canonical JSON meaning
`sort_keys, separators=(",",":"), ensure_ascii=False`. One unchained header line first
(`{"case_id":"nib:<slug>","tool":"nib","version":…,"t0":…}`), content unchecked but required. No `ts`,
no `v`, no `tool` on rows. `glance --verify` is generic — proven by running it on caseclock's
REGISTRAR-generated tape with zero glance row kinds: INTACT — and it checks six-key presence, seq
contiguity, the prev link and the digest, nothing else. **Cheapest correct path: port
`C:\fray\src\tape.{h,cpp}` verbatim** (eight mechanical hunks off glance's), take caseclock's BLAKE2b
RFC vectors (including the 127/128/129-byte cases, where transcriptions break) into `--selftest`, and
run `glance --verify` on nib's own tape as a selftest line. `fabric.h`'s 128-bit non-crypto stub
must not ship on anything nib calls hash-chained; what auricle contributes is the *semantics*
(author stamped from the lane, `op_id` idempotency, typed refusals, a corruption hook for the
falsifier).

The tape lives beside the document, append-only; a failed tape write is fatal (no placeholder where
a record belongs); and it doubles as an **UNPROMPTED tape** (`ts_ms, lane, id, text` per delta is a
projection of the `changeset` and `tick` rows) so every nib session is benchmark-format evidence by
construction — D9's corpus, accruing from ordinary writing. The row kinds, all in `body`:

`session` {model_path, model_desc, **model_sha256** (hash the GGUF before it loads), serve_hash,
n_ctx, kv, devices, have_gpu, sampler {chain:[min_p,temp,dist], min_p 0.05, temp 0.7, seed 11},
gen_cap, floor_ms, flush_ms, clause_tok_cap, bscore_gate, molt_wm, arm, egress_bytes 0, module_gate}
— the row that answers SPEC 8.1.3 · `mandate` {seat, name, mandate} ×3, verbatim · `coefficient`
{name, value, was, source} for every dial that ever moves · `switch` {which, from, to, by, gates} ·
`changeset` {author, rev, base_rev, cs, blocks, ins, del} · `percept` {id, lane, kind, span, ms} ·
`judgment` {i, rev, span, reason, bscore, probe_ms, lat_ms, clause, margins{SPEAKER,SKEPTIC,SENTINEL},
arm, **coarse**} · `hold` {i, seat, margin, reason, suppressed_by ∈ {null, floor, repeat, resolved,
dampened, empty_gen}, last_margin} — fusord has no such row; nib should · `refused` {seat, m, why} ·
`emit` {i, seat, margin, gen_ms, toks, stop ∈ {eog, newline, cap, sentence}, say, block, after_block,
dep, rev} · `abort` {i, seat, formed, reached_air, mid_word, live, reason ∈ {contradicts_dep,
margin, late_percept, operator}, trigger_rev, dep, ms_to_withdraw, toks} · `label` {emit_id, kind} ·
`molt` {before, rung_toks, after, dur_ms, rung, tail_words} · `tick` {gap_s} · `end` {fusord's
aggregate + ring_dropped, echoes, truncated_lanes, coarse_frac, sampler_consulted}.

Three invariants for `--selftest`, each from a measured estate bug: **events = emit + hold, by
addition, never by conflation** (the 12×-high read); **emits(starts) = commits + dampened +
empty_gen + aborts, per seat**; and **the narrative is derived from the fields, never written beside
them** (the beat assertion: a stop implies an abort row and a re-form row exist; no stop implies
neither).

Stage 5 reports, per the UNPROMPTED law, **curves not scalars**, each with its denominator:

| metric | definition | source |
|---|---|---|
| **leave-on rate** | fraction of writing minutes with AI on, per day | `switch` rows — the falsifier itself, as a number |
| `fires/hr` and **holds per fire** | emissions per hour of writing; boundaries judged per emission | `b`, `emit` |
| **refusals/hr** | floor refusals — how often it wanted to speak and the human held the floor | `refused` |
| **un-say rate** | aborts per emission, split by reason (dep-touched vs margin) | `abort`, `emit` |
| **dismiss / edit / accept** | the label distribution, per seat — the first precision the estate has ever had | `label` |
| `staleness` | keystroke-to-judgment latency p50/p95 | `b.lat_ms` |
| **coarse fraction** | per D4: report above 10 %, invalid above 40 %, per lane | `b.reason == c` |
| `catch@window` | when the operator retro-marks a moment ("it should have said something here"), was there an emission in `[p, c]`? — the only mark set that needs a sitting | marks sidecar |
| **toggle flips** | flips per hour and direction, and the shadow-vs-twin pairs | `switch`, `b` in shadow |
| **vigilance decrement** | usefulness by hour of a long session — the human-complement axis UNPROMPTED §14 names and nobody has run | `label` by session hour |
| **stop latency** (the field's name) | `t(abort) − t(the keystroke that triggered it)`, p50/p95 | `abort`, `cs` |
| **un-say precision** | for each abort, did the human's next 60 s in fact contradict the killed sentence? Blind post-hoc coding by someone other than the operator, killed text withheld until judged | `abort` + following `cs`; a sitting |
| **miss rate** | resident blocks the human deleted within 60 s — emissions that should have been un-said and were not | `emit` + `cs` deletions on resident character ids |
| **takeover-rate curve** | fraction of human pauses ≥ θ at which the resident emitted, swept θ ∈ {0.5, 1, 2, 4, 8} s — the single most diagnostic curve | `cs` gaps, `emit` |
| **timing divergence** | Jensen–Shannon divergence of emission-onset vs the human's own gap distribution in the same document (Full-Duplex-Bench's method) | `cs`, `emit` |
| **survival at 10 min / 1 h / session end** | fraction of resident-authored characters still present, by identity — nib's acceptance rate, exact where Copilot's persistence measures are approximate | authorship in the log |
| **own-vs-resident deletion rate** | the territoriality probe: deletion rate of human-authored vs resident-authored characters, age-normalised | authorship in the log |
| **felt ownership** | a daily one-item scale over the week — the cheapest test of whether the honesty laws do the work they claim | a sidecar, by hand |

Two standing limitations to print with every table, not discover from a critic: the observer effect
(`OPEN_PROBLEMS` #11 — the writer reads the emissions), and self-reference (the operator will write
about nib in nib; port fusord's `self_ref` detector and report the rate, D6).

---

## §7 · Novelty — what is unprecedented, what is convergent, what must be cited

*The survey (`docs/review/PRIOR_ART_FLOOR_CONTROL.md`, ~90 citations with a verification ledger)
searched for nib's conjunction and found no match. The three closest, and how they differ:*

- **Lehmann, Shauchenka & Buschek, CHI '26** (arXiv:2509.11826): agents in shared documents, one
  week, N=30 — invoked by explicit tasks, output in *comments*, no perception of keystrokes, no
  boundary, no retraction, cloud model. The Stage 5 template, and the prior to beat.
- **Ink & Switch Patchwork**: a bot as a named collaborator with timeline attribution — nib's
  authorship claim, already prototyped — but it writes *on a branch* and the human merges: floor
  control solved by never taking the floor. nib is explicitly betting against that design; say so.
- **Cotypist / Typeahead / Cursor Tab**: local weights, inline, sub-100 ms, always running — nib's
  deployment profile — but caret-anchored ghost text with no block, no identity, no tape, and no way
  to retract because nothing was committed. Cursor alone has a principled *don't-suggest* action,
  and even that fires on the keystroke clock.

**Genuinely unprecedented, as of this survey:** (1) visible un-saying of persistent, already-rendered
text with the retraction on an append-only hash-chained tape — speech systems stop, serving stacks
abort, nothing withdraws characters a human has read and proves it; (2) deletions as first-class
percepts — keystroke logging and CoAuthor *record* them, nothing *feeds* them to a model as world,
and the byte-conservation falsifier is a rigour level no co-writing system approaches; (3) a
build-time linker gate as a shipped, checkable privacy guarantee (local-only is common; falsifiable
local-only is not); (4) the RESIDENT/TURN-BASED toggle as an instrumented twin race inside the
product with seat, seed and sampler pinned by hash. And from this document: (5) the keystroke as a
contradicting rev in op-space (§5.1) — the fabric's kill, applied to a human hand.

**Convergent, and stronger for the citation:** evidence-triggered judgment on the ingest pass
(Levinson & Torreira's 200 ms puzzle; Moshi; SyncLLM; "Think while Listening"); silence as a
generated symbol (SyncLLM's clock-synchronous intervals — `[tick +Ns]` is the same idea from another
lineage); a forming/committed seam (Moshi's Inner Monologue: a text plan running ahead of the audio,
which is also the template for checking contradiction against a cheap plan ahead of the visible
characters); KV forking as the abort primitive (SGLang RadixAttention, vLLM prefix caching);
territory = the paragraph (Larsen-Ledet & Korsgaard 2019, measured); per-character authorship colour
(Etherpad 2008); the expected-utility emit threshold (Horvitz 1999 → Cursor Tab-RL); the 2 s window
(the keystroke-logging burst threshold).

**The one-sentence positioning the survey recommends:** *nib is not the first system in which a
model writes beside a human in real time — Patchwork and the CHI '26 work got there, and Moshi solved
the always-on half in speech. nib is the first system in which the model can take words back off the
page and prove it did.*

**Must cite** (non-negotiable in a README or a paper): Sacks/Schegloff/Jefferson 1974 · Duncan 1972 ·
Yngve 1970 · Schegloff 1982 · Levinson & Torreira 2015 · Moshi 2024 · SyncLLM 2024 · LSLM ·
Freeze-Omni · Full-Duplex-Bench + v1.5 · HumDial 2026 · EchoChain 2026 · Ellis & Gibbs 1989 ·
Dourish & Bellotti 1992 · Dommel & Garcia-Luna-Aceves 1997 · Larsen-Ledet & Korsgaard 2019 · Wang,
Tan & Lu 2017 · Lehmann et al. 2026 · Etherpad 2008 · CoAuthor 2022 · Ziegler et al. 2022 · Cursor
Tab-RL 2025 · Draxler et al. 2024 · Jakesch et al. 2023 · Horvitz 1999 · McFarlane 2002 · Iqbal &
Bailey · SGLang 2024 · StreamingLLM 2024 · Patchwork. The full list with URLs is in the companion.

---

## §8 · Remediation and implementation — ordered, with the falsifier for each

**P0a · the two criticals a real user hits in the first hour** (hours; no GPU): **C-1** a display
column distinct from the byte offset, splice snapping to sequence boundaries, and the multi-byte
property test with `utf8_valid` after every step · **C-2 / F6** surrogate pairs, with a driver check
that posts `D83D DE00` and asserts `F0 9F 98 80` on disk · **M-1** DPI awareness at startup (one
call or a manifest) so `WM_DPICHANGED` can be delivered on this 225 % box.

**P0b · hygiene and the latent Stage-2 breakers** (hours; no GPU):
F1 explicit backend loads + runtime module gate · F2 / H-2 undo, redo and open → percepts,
`Rev.kind` · **H-4 / H-5** `Doc::apply` clears or transforms the undo stacks, undo/redo validate
before popping, and `Doc::apply` gets tests · **H-1** dispatch `--check`/`--unpack`/`--ops` or
delete them · F3 the seat set in the self-echo filter + a selftest that it equals `seats()` · F4(a)
the raw-line lane sentinel for ticks · F5 / H-3 loud drop counter, refuse-not-return, no judging on
undecoded clauses, `--ctx` clamped · F7 marker on every deletion chunk + a 2,000-byte removal test ·
**F17 `feed` branches on kind; ticks decode and return without judging** · **F18 SPEC 5.1.6 states
both halves of self-echo** · the `/DELAYLOAD` assertion in `build.bat` · `WM_QUERYENDSESSION` ·
`dec()` return checked · F16 and §3.3 every stale number and date, by git · BLUEPRINT §6 drops the
13 µs / 65.7 s pairing and §7 carries the 1.208× caveat · BLUEPRINT §2/§4/§7 and ASSEMBLY §4
supersession notes · SPEC 9.4 built or marked [SPECIFIED] · CLAUDE.md rule 2 names five DLLs and
rule 11 (one process, two clocks) · the two diagnoses (§1) written into README. *Falsifier:*
`--selftest` and `drive.py` green with the new checks; the module gate refuses when `ggml-rpc.dll`
is present; a tick produces zero probes; an undo is a percept.

**P0c · propose upstream to auricle:** `uint8_t kind` in `Delta`'s padding (F4b), `sizeof` unchanged;
`FileTailSource` writes 0. One line; the seam stops lying twice.

**P1 · Stage 1c, the wire** (days): resident thread + judgment ring; `Judgment.rev/span` (F11);
`Percept.span`; the `Hand` table (F10); the gutter; the AI switch with unload; **the tape as a
verbatim port of `C:\fray\src\tape.{h,cpp}`** with caseclock's RFC vectors and a `glance --verify`
selftest line (§6.4); the `session`/`mandate`/`coefficient` rows including the GGUF's SHA-256;
resume by re-fold; the keystroke-to-repaint measurement with the resident on and off. Then the five
measurements of §6.2, plus the tick A/B (`OPEN_PROBLEMS` #9).

**P2 · Stage 2** (days), lifting by the map's ranges: the speak-cue and sampler (`fusord.cpp:477-480`,
`285-288`: min-p 0.05 → temp 0.7 → dist(11), no top-k/top-p/penalty), the fork/drop around
composition (`475-476`, `504`), the 28-token loop with its four stops (`489-503`), **the self-echo
commit** (`555-561`); the dep test (§5.1) as a serializer rule shaped like `fabric.h:274-285` so Stage
3 reuses it; the forming plane (§5.3); commit via `Doc::apply` with a real inverse (F9); the near-dup
damper from `blackbox.cpp:635-662` (Jaccard ≥ 0.60 over a window of 8 — 25 lines, preferred over
fusord's 130) with resolve-permanence keyed on *a changeset on the resident's block* rather than a
word list; the label valve; `hold` rows with `last_margin`; every scaffold individually switchable
and recorded (the scaffolding-ratio receipt); `coarse_frac` printed per lane beside every rate; the
fold (§5.6); `[SEAT] ` tags in the file (§5.7); the emission cap kept at one sentence until §5.9
exists; pacing on the margin only, saturating in the tail.

**P3 · Stage 3**: structural + semantic un-say; the abort record in `cut_audio`'s shape
(`starship.cpp:544-552`) with the honest mid-word/live disclosure; the re-form *after* the stop, never
as the mechanism; the beat assertion lifted into `--selftest`; interleaved ingest/gen (§5.9); the
stop-latency number measured on the real clock through the real window (an `aborts = 0` that was
never reachable is structural, not measured); the demo.

**P4 · Stage 4**: RESIDENT/TURN-BASED with shadow judgments; the address key; `nib --twin` replay;
the scaffold table printed per run; D1 parity as a selftest that the two arms share one code path.

**P5 · Stage 5 and 5b**: the week, on real work (D6); the father test; the tables of §6.4; the
retune trigger evaluated on real labels (D3); the 27B question (D2) if a sixth channel hits the floor.

**Act II prerequisites, any time:** 0f — the harness already runs from `tools/etherpad_harness`
(`node gen.mjs 7 > cases.jsonl && node check.mjs cases.jsonl`); add `nib --diff` with a C++ port of
Etherpad's generator so both sides produce the same corpus from one seed, and make it a pre-commit
gate. Then §4b's ~600 lines in order (`AttributePool` → codec → `AttributeMap` → zipper → `atext` →
authored inserts → `inverse`), then `compose` and `follow` under the harness, the trailing-newline
decision, and the runtime gate narrowed rather than removed. P0 also carries the small port fixes
from §4b: the `make_splice` comments, the partial-token acceptance in `deserialize_ops`, a length cap
in `parse_num`, and a written note that the stray-`\n` refusal is a deliberate divergence.

**The consistency fence** (from NOTICE law 11): a selftest that pairs every quantitative claim
across README, SPEC, ROADMAP, HANDOFF and the devlog — check counts, version strings, dates, the
47× figure — and prints disagreements. §3.3 is what it would have printed today.

---

## §9 · What not to do — the refusals, sharpened by the read

- No pacing that is not a measured state. If a 20-word line forms in a second, let it.
- No "thinking" mark. The gutter renders a margin that exists; that is the only permitted signal.
- No emission inside a human block in Act I, and no exception for "obviously helpful" completions:
  the moment the resident writes into your sentence it stops being a colleague and becomes
  autocomplete, and the twin race is void.
- No retune before labels exist and no labels by dialog: the editing *is* the label.
- No claim of `0 bytes egress` until the process module gate passes at runtime.
- No Stage 5 on estate work; no manufactured session; report the self-reference rate.
- No twin that inherits fewer scaffolds than the resident (D1). The replay shares the code.
- No number without its date, and no date that is not git's.
- No 13 µs without un-say precision beside it. A latency for a decision of unmeasured accuracy is a
  demo number.
- No floor rule without the argument against the no-floor position: the floor exists for the
  human's attention, not for convergence, and the document must say so or CSCW reviewers will say it
  first.

---

## §10 · The line

The pad is the first surface on which un-saying is an ordinary event, and the op log is the first
place where "your next keystroke contradicted it" is a set test rather than a judgment. Everything
above is arrangement of parts that exist — the loop from fusord, the kill from the fabric, the format
from Etherpad, the metrics from UNPROMPTED — around one new fact: a writing surface already knows how
to render a character appearing and being withdrawn, and already records both. Build the wire, turn
the switch off by default, and let the first week decide.

---

*Companion reports on disk: `docs/review/QC_NIB_REPO.md` (repo QC, oracles re-run) ·
`docs/review/LIFT_MAP_AURICLE.md` (2,400 lines: every line range to lift for Stages 2–4, the tape
format verified, twenty-two doctrine laws with sources, sixteen discrepancies) ·
`docs/review/ETHERPAD_DEEP_READ.md` (authorship, compose/follow, the host protocol, units, ten
divergences) · `docs/review/PRIOR_ART_FLOOR_CONTROL.md` (~90 citations with a verification ledger).
The differential harness is in `tools/etherpad_harness/`. Nothing was committed; the working tree is
the operator's to inspect first.*
