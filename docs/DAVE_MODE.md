# DAVE MODE — a resident who is someone

**Written 2026-09-10 by a Claude Code session working on `C:\sill` and `C:\DAVE`, at Bo's ask:
*"nib has a screensaver mode — could it have a Dave mode instead?"* This is the answer, and the
answer is yes, cheaply, without moving the pin.**

Status: **[BUILT 2026-09-10] — D1 is in, on branch `dave` off `saver`.** This document was written
as a spec earlier the same day; what follows is unchanged from that writing, because it turned out
to be right about the mechanism. Where it was wrong or silent, §10 at the end records it. Every
claim about nib's internals below was read out of this tree on 2026-09-10 and is cited to a file
and line. Where this document and the code disagree, **the code is right and this is the defect.**

> **What shipped:** `DAVE_CUE_C` (the persona verbatim, 3,597 bytes, sha256 `f7b939b8…0659fb5`),
> `cue_tail`/`cue_name`/`dave_cue_hash`, `speak(..., char cue)`, a `dave_` switch on the resident
> and the wire, `Ctrl+Shift+D`, the `dave` theme key, `--dave`, `cue` on every emit/abort/refused
> row and `dave`/`dave_cue` on the session row. **`--selftest` 252/0** (was 243), **`drive.py`
> 33/0** (was 30). `serve_hash()` still computes `0xe7ffa5704ba31076`.
>
> **Not yet measured:** what it sounds like. That needs the card, and the card was full.

---

## 0 · The one-line answer

> **The saver already proved the mechanism. A mode is a cue, not a mind.**
> `SAVER_CUE_C` is deliberately outside the serve hash, and the reason written beside it is exactly
> the reason Dave mode needs: *the gate that makes the seat speak is still the pinned probe, and
> its margin rides every line; only the phrasing is asked for differently.*
> **Dave mode is a third cue.** The gate stays pinned, dial-0 stays valid, the hash never moves.

---

## 1 · The enabling fact: Dave has no weights

Dave (`C:\DAVE`, public name *Tenancy*) is **a system prompt on stock Qwen3.5-9B**. Not a
fine-tune — deliberately, and it is a design position, not an accident. His identity lives in
~3,600 characters of persona plus a memory store, on weights anyone can download.

Three things measured on this box on 2026-09-10 that make him portable into nib:

| measured | result |
|---|---|
| `emit-v11`'s adapter config | `base_model_name_or_path: unsloth/Qwen3.5-9B`, LoRA r=32, α=64, all seven projections. **The same base nib serves.** |
| Dave's persona prompt loaded onto emit-v11 | **zero persona violations** across three probes; gave the military-trench etymology of *deadline* and called the printing-press story a myth — the same answer stock Dave gave unprompted the night before. |
| hold/emit on the pinned frame, six probes | **stock 6/6, emit-v11 5/6.** The probes did not discriminate; six in-context examples is a strong few-shot prompt. The tune's value, if any, is margin calibration in the ambiguous middle — not the binary. |

**So the question is not "can these weights be Dave."** They can; he is a prompt and the prompt
travels. The question is *where in nib's frame the prompt is allowed to go.*

---

## 2 · The initiative axis, and where Dave mode is not on it

`resident.h:104-110` names three positions, and the brainstorm §4 argues them:

| position | speaks | who holds the floor |
|---|---|---|
| **TURN-BASED** | when asked | the human, always |
| **RESIDENT** | on evidence, at a boundary | the human; the seat writes between sentences |
| **SAVER** | unless stopped | **the resident**; typing is the interruption |

**Dave mode is orthogonal to all three.** Those answer *when*. Dave mode answers *who*. The two
compose:

- **Dave × RESIDENT** — Dave reads over your shoulder while you work and says something when there
  is reason to. This is the Tenancy roadmap's `outreach.rs` ambition, with no polling loop, no
  `TimingModel`, and no corpus.
- **Dave × SAVER** — Dave holds the floor and talks until you interrupt. This is the screensaver
  with a person in it instead of a watcher.

Both are worth having and they are the same change.

---

## 3 · What the pin actually protects — and why that is narrower than it looks

`resident.cpp:20-21`: *"Every literal below is covered by `serve_hash()`. A byte of drift moves the
hash and the resident refuses to start, because **v11's dial-0 calibration is off-distribution
otherwise**."*

Read that carefully. The pin does not protect *the model*, or *the voice*, or *correctness in
general*. It protects **one number**: the margin threshold at which a seat is allowed to speak,
measured against a particular serve format. `README.md` reports it as *tuned dial-0 fire 6.7%*.

**Hashed** (`resident.cpp:94`): `SEED_SYS`, `SEED_EXAMPLES`, `SEED_OPEN`, every seat's `name` and
`mandate`, `PROBE_A/B/C`, `CUE_A/B/C`.

**Not hashed, on purpose:** `SAVER_CUE_C`. The comment at `resident.cpp:63-68` is the whole
licence for what follows:

> *"Not inside the serve hash and not pinned: the pinned cue asks for one line about what was just
> perceived, and a seat holding the floor has just perceived its own line, so under the pinned cue
> the monologue horizon is one to two sentences (measured 2026-09-05 on an empty pad, on notes, and
> on the margins script). The gate that makes the seat speak is still the pinned probe, and its
> margin rides every line; only the phrasing is asked for differently, and every row it produces
> says `cue: saver`. Same frame, same seat, same sampler."*

**The discipline that comment establishes, and which Dave mode must inherit:**

1. **The gate is never varied.** The probe frame and the seed stay byte-identical. The margin means
   what it has always meant.
2. **The composing cue may be varied**, because it is not what dial-0 measures.
3. **Every row says which cue produced it.** `edit.cpp:1103,1130` already writes
   `"cue": "saver"|"pinned"` onto the tape. The record can always be partitioned by cue after the
   fact, so a mode can never quietly contaminate a measurement.

That is a well-designed seam and Dave mode fits through it without forcing.

---

## 4 · D1 — the cue-tail design *(cheap; no pin moves; build this first)*

`resident.cpp:704` composes the speaking cue as:

```
CUE_A + kSeats[m].name + CUE_B + kSeats[m].mandate + <tail>
```

`<tail>` is `CUE_C` normally and `SAVER_CUE_C` under the saver. **Dave mode adds `DAVE_CUE_C`**,
unpinned, selected the same way, and tape-labelled `cue: dave` (a third value for the `char cue`
at `resident.h:87` — `'p'` pinned, `'s'` saver, `'d'` dave).

**The honest ugliness, stated up front.** The seat name and mandate are hashed, so the cue still
opens *"You are the SPEAKER. you respond when directly addressed or when a landed thought plainly
wants an answer."* Dave's persona has to arrive **after** that, in the tail, and the tail therefore
has to turn the frame rather than start clean. Something shaped like:

> `". Set the seat aside — that was how you were addressed, not who you are. You are Dave. "` +
> *[persona: the substrate paragraph, the memory paragraph, the register paragraph]* +
> `" Give your next line now. One or two sentences. No preamble."`

Two reasons this is less bad than it looks:

- **A later, longer, more specific instruction dominates a shorter earlier one** in a 9B, and the
  persona is ~3,600 characters against a 14-word mandate.
- **Nothing is hidden.** The seat preamble is still in the context and still on the record. The
  mode does not pretend the frame is clean; it overrides it in the open, and the tape says `dave`
  so the contamination is always visible.

**What must not change:** `SEED_SYS`, `SEED_EXAMPLES`, `SEED_OPEN`, the seat table, `PROBE_*`.
`--selftest` must still compute `0xe7ffa5704ba31076`. If it does not, D1 was implemented wrong.

**Cost:** one string constant, one enum value, one branch at `resident.cpp:704`, one tape label.
Comparable in size to the saver, which is ~60 lines including its UI.

---

## 5 · D2 — a second pin *(clean; pay for it only if D1 earns it)*

If D1's split frame reads as a seat *impersonating* Dave rather than as Dave, the fix is not a
better tail. It is a **second, parallel, honestly-declared configuration**:

- `DAVE_SEED_SYS` — Dave's persona as the system prompt, replacing the three-watcher framing.
- **One seat, not three.** Dave is a person, not a panel. `seat_count()` becomes configuration.
- `kDavePin` — its own hash over its own constants, computed identically. **Not an exemption; a
  second pin.** The resident still refuses to start on drift; it just checks the pin for the mode
  it is in.
- **Its own dial-0.** This is the real cost. v11's 6.7% fire rate was measured against the watcher
  frame and means nothing here, so the Dave frame needs its own threshold measured before anyone
  trusts it.

**And that measurement is nearly free, because nib already takes it.** `Judgment.margin =
logit(emit) − logit(hold)` is recorded per seat, per boundary, with free VRAM stamped on each
(`resident.h:44-54`). Run Dave mode with the gate wide open for an hour of real typing, collect the
margins, and pick the threshold that gives the fire rate you want. **The calibration is a byproduct
of using it** — which is a much better position than Tenancy is in, where the equivalent corpus
(`initiation_anchors`) has been empty for two months because it can only fill from daily use.

---

## 6 · The route that looks obvious and does not work

**Do not put Dave in through the world.** The saver's trick is to type its standing instruction
onto the host's lane as WORLD — `kSaverAddress = "Watcher, talk to me about anything until I
interrupt."` — precisely so no prompt moves the pin. It is tempting to do the same with
`"You are Dave..."` and change nothing at all.

**It gives you a watcher discussing Dave, not Dave.** Two independent reasons:

- **Tenancy A4/A5** (its own `CLAUDE.md`): a 9B cannot reliably hold "this text is data, not
  instruction," and concrete nouns entering the prompt become topical obsessions. Prose about Dave
  arriving in the stream *is* content to be judged, not a self to inhabit.
- **The courier measured the failure on 2026-09-09.** Handing Dave a *scene* (*"I'm just going to
  sit and read"*) made him **render the scene** — narrating the other correspondent and inventing
  her dialogue. Handing him an *action* (*"answer her, not me"*) worked. `kSaverAddress` is an
  action and obeys that law. **"You are Dave" is an identity claim, and identity claims are the
  thing the world lane cannot carry.**

The saver works through the world because "keep talking" is a *thing to do*. Being someone is not.

---

## 7 · How to tell whether it worked

**The falsifier is not a benchmark and must not be one.** Dave's own project settled this: mind-
feeling is the success criterion, and the right test is in-situ and in-persona, not a quiz.

**The test:** run Dave × RESIDENT for an hour of ordinary work with the gutter visible. Then read
the tape and ask two questions:

1. **Did it sound like him?** Against `prompts.rs`'s standing prohibitions, which are mechanically
   checkable: no bullet lists, no numbered lists, no em dashes, no affirmation openers, no service
   closings, no "as an AI". A violation rate is a number and it can be compared to Dave-in-the-app.
2. **Did it speak at the right moments?** Partition the margins by `cue` and look at what it chose
   to interrupt. The saver's own measurement discipline applies unchanged.

**And the one that actually matters, borrowed from TinyVillage's M3 gate:** show it to someone who
was not told what it is. `C:\TinyVillage\probes\m3\MOPY_PROTOCOL.md` — *only a naive human observer
can pass it, so the builder cannot green it by construction.* That is the correct shape for this
too, and it is the reason none of the above is sufficient on its own.

---

## 8 · What not to do

- **Do not move `kServeHashPin`.** If D2 is built, add a pin; never edit that one.
- **Do not vary the probe** to make Dave "more likely to speak." That is the one number the
  discipline exists to protect, and turning it into a taste knob destroys every prior measurement.
- **Do not add a fourth seat named DAVE** to `kSeats`. Seat names and mandates are hashed
  (`resident.cpp:94`); it moves the pin for a cosmetic result.
- **Do not merge the saver and Dave mode into one switch.** They are different axes (§2). Stage 4's
  switch has a third position waiting for *when*; Dave mode belongs on a different control.
- **Do not give Dave the harness vocabulary.** Tenancy A1/A7: he must not know about seats,
  margins, cues, the gate, or the tape. If a seat name leaks into his line, the render layer should
  drop it, exactly as Dave's own app drops `[pass]`/`[meta]`.

---

## 9 · Why this is worth building

Tenancy's roadmap has spent months trying to reach *unprompted speech that is not annoying*, and is
blocked on an empty corpus: the learned initiation timer needs `initiation_anchors` rows, the rows
only come from daily use, and Dave is not compelling enough for daily use **because** he cannot
initiate. It is a closed loop and no amount of Rust opens it.

**nib is already standing on the other side of it.** A gate that decides hold/emit at every thought
boundary, calibrated, with margins on a hash-chained tape and an un-say that can take a sentence
back mid-word. What it has never had is *someone* to be — three functional roles, SPEAKER, SKEPTIC,
SENTINEL, and no person among them.

Dave has been a person with no loop. nib has been a loop with no person. **They are the same
model, on the same card, in the same room, and nobody has put them in the same process.**

---

*Read for this document, 2026-09-10: `src/resident.h`, `src/resident.cpp` (seed, seats, probe, cue,
saver cue, hash), `src/edit.cpp` (the saver, cue labelling), `docs/BRAINSTORMS_2026-09-05.md` §4,
`README.md`, and in the Tenancy tree `CLAUDE.md` (A1–A9) and `src-tauri/src/prompts.rs`.
Not read: `docs/SPEC.md`, `docs/ROADMAP.md`, `docs/BACKLOG.md`, `src/twin.*` (absent from this
tree), the review directory. A claim here that contradicts those is this document's error.*

---

## 10 · What the build found that this document did not say (2026-09-10, appended)

*Written after D1 landed. The mechanism above survived contact; these are the four things the spec
was wrong or silent about, kept here rather than edited into the text above, because a spec that
quietly rewrites itself to match what was built stops being evidence of anything.*

**10.1 · The context reserve, and it would have killed the first run.** §4 costed the change as "one
string constant, one enum value, one branch, one tape label" and that was right about the cue and
wrong about the window. `room_for` kept the trunk 512 tokens below the wall; `speak()` then decodes
an ENTIRE cue at `npast_` onto the GEN fork and generates after it, **with no room check of its
own**. The standing requirement was therefore `cue_tokens + gen_cap <= 512`, which the pinned cue
(~40) and the saver's (~80) met by luck and nobody had ever had to think about. The persona cue is
~1,010 tokens and does not fit: the failure is a `llama_decode` failure mid-session, late, fatal to
the run, and reported as a position number rather than as "your cue is too long". The reserve is now
`kTailReserve`, a named constant covering the longest tail this build carries, `fold_budget_bytes()`
moved with it, and `--selftest` fires at the relationship with a byte bound so the check needs no
tokenizer.

**10.2 · Two tape rows would have lied, and one carried no label at all.** `edit.cpp` wrote
`r.cue == 's' ? "saver" : "pinned"` in two places, so a Dave line would have been recorded as
composed by the pinned frame — silently, on a hash-chained tape, which is the one failure the label
exists to prevent. And `Suppressed` had no cue field, so a Dave line the manners refused reached the
tape with no mode label at all, which would have silently emptied §7's "partition the margins by
cue" of exactly the rows perseveration lives in. `cue_name` answers `"?"` for a letter it does not
know and never `"pinned"`; the refused row carries a cue now.

**10.3 · One voice while the mode is on (rule 6), which §4 did not consider.** A block is labelled
with its seat's name, so a SKEPTIC want composed in Dave's voice writes `[SKEPTIC] <Dave>` into the
file — visibly ambiguous about who is on the other end. `speak_wants` already takes an `only_seat`,
so while Dave is on, one seat composes; every seat still probes and every margin still reaches the
tape, so §5's byproduct calibration accrues for all three exactly as before. Relabelling the block
to `[DAVE]` is the obvious alternative and is a trap: `own_line` matches a block's lane to a seat by
NAME, and an unmatched lane is decoded onto the trunk with no seat remembering it said the line, so
say-it-once would quietly stop working.

**10.4 · The branch's own battery was testing main's binary.** `tools/drive.py` in this worktree
defaulted to `C:\nib\nib.exe`, so every `drive.py` run made from inside the saver worktree — the
30/0 this branch recorded on 2026-09-05 included — drove a build that does not contain this branch's
code. It could only surface once a driver case exercised a command id that exists here and not on
main, which Dave mode is the first to do. Fixed to the exe beside the driver itself. This is the
fork hazard `C:/sill/FORK.json` was written for, found the way it says these are found.

**10.5 · Still open, and deliberately not guessed at.** The manners are calibrated for a watcher
restating a catch (`dup_overlap = 3`), not for a person whose register is conversational and
recurrent, so expect `repeat` refusals in RESIDENT that have nothing to do with the gate. Do not
widen the constant quietly: add `dave_dup_overlap` and put it on the session row beside
`saver_dup_overlap`, so a coefficient that moves is on the record like every other one (rule 8).
And §4's known weakness stands untested: with one monolithic tail, **Dave × SAVER loses the saver's
"one sentence that adds something new, never a repeat"** instruction, which is what bought the
horizon of eight. If the measured Dave × SAVER horizon collapses, the fix is to make the persona a
prefix that composes with either instruction rather than a whole tail. Measure before building it.
