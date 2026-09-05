# nib — BACKLOG

*Open items, each with the date it was raised and where it came from. An item leaves this file when
it lands (named with its version in the ROADMAP) or when it is deliberately not done (ROADMAP,
"Deliberately not doing"). Nothing here is a promise about order except the first section.*

## Found 2026-09-05 (evening) by reading every source at 0.10.0

Every falsifier that was fired at held; these are one level below where the falsifiers look, which
is the shape the review of 09-04 named. Ranked. The remediation of this evening takes them in
this order, a commit each; the rest wait their turn.

**Landed in 0.10.1:** the span memory (one entry per boundary, 64 deep, a struct the selftest
fires at; a want whose boundary has fallen out is refused with `span-unknown`, never placed after
line 1) and the want's own boundary on every row it produces, with the forming plane anchored to
the same span. Until then the wire remembered a span once per SEAT in a 16-slot ring — five
boundaries of memory — and the emission, the abort, the suppression and the forming plane all
carried the *newest* boundary rather than the want's, so the emit row's dep span was the newest
clause and not the one the line was about, and the `span-edited` refusal tested the wrong bytes.

**Landed in 0.10.2 — own speech through the document** (SPEC 5.1.6 and 6.3.6 as amended,
6.2.11.3.1). Three findings, one design. The fold did not know who wrote what: `fold_log` ignored
`Rev.author` and `fold_tape` never read the row's `author`, so a seed or twin fold over a document
holding `[SKEPTIC] …` blocks fed the trunk `[bo] [SKEPTIC] …`, the human saying the seat's line,
and a restore did the same for the emissions committed at stop. The trunk heard lines the editor
refused: `speak_wants` queued every allowed line for the trunk before the second floor gate had
ruled, so a `refused {why: floor}` line was on the trunk and in the manners' memory while the
document never received it. And the lines composed at stop never reached the trunk before the
checkpoint. Now the resident's line joins the trunk only when the document has it, carried back
through the pad on the seat's lane as an own-speech percept (kind `s`) that is decoded raw and
never judged; both folds attribute by author and replay a seat's block the same way, prefix and
newlines off; the percept carries the block's revision, so a checkpoint's cursor is never before
a line the trunk holds and a restore cannot replay one twice; the CLI feeds its own lines back
itself; the one line the thread still commits on its own is an abort's aired prefix, flushed
before every checkpoint. The compiler pushes the pending clause before a seat's line, which also
closes the "block inserted above the caret" case for any `floor_ms`/`quiet_ms`. Ten checks in
`--selftest` with no model; the driver asserts an own-speech row on the tape and no percept on
the hand's lane beginning with a seat's tag, live or folded.
**Landed in 0.10.3 — the manners survive the switch, the refusal, the flush clock.** The
checkpoint's sidecar carries what each seat last said, the clause it answered, its age in
milliseconds and in boundaries, and whether the world settled it (`m<seat>.<field>` lines); a
restored resident gets its ladder back, a twin rebuilds its own from the fold's own-speech
percepts. The ladder is a pure check now (`manners_allows`), fired at in `--selftest` on an
imported memory with no model. The refusal SPEC 6.2.11.3 promises is decided by the editor before
the model loads: a checkpoint whose bound row is in none of the tapes makes the resident the
twin, where through 0.10.2 the fold ran with an error on its row. And the resident's 1500 ms
flush clock starts at the percept (SPEC 6.2.13): through 0.10.2 it ran from the last judgment,
and since a pad's percept arrives whole and is decoded in one burst, the `t` path fired on the
first six tokens of every sentence typed after any pause longer than a second and a half (the
un-say window's log: `t` on "Actually the staging database is mysql", `b` on "5", `b` on the
rest — three boundaries for one sentence) and never on a stalled clause. A `t` now means the
decode of a percept itself stalled.

- **A clause whose judgment includes a deletion percept can carry a span whose end precedes its
  start** (`step` takes `span_b` from a deletion's empty span). Edge; a deletion closes its own
  clause in every measured run.
- Doc drift found and corrected the same evening: SPEC §6's header still said emission and
  un-saying were structurally absent; SPEC 11.1 said 201 checks; SPEC 8.1.2 said there was no emit
  path yet; the handoff counted its own commit out.

## Landed in 0.11.0 (Stage 4a, 2026-09-05)

The mode switch RESIDENT / TURN-BASED as the wire's floor policy and nothing else (`Ctrl+Shift+T`,
a `switch` row, the status line, `arm` on the session row); the key (`Ctrl+Enter`, an `ask` row,
`trigger: k` on every row it produces; in RESIDENT a yield); the emit switch, live (`Ctrl+Shift+E`,
the sampler constructed and freed on the thread — the item "`emit` has no key" leaves this file);
`trigger` on every `emit`, `refused` and `abort` row; judgments in shadow during TURN-BASED. What
is still open of Stage 4 is the replay twin, `nib --twin` (SPEC 6.1.6), and the scaffold table.

## Landed in 0.8.0 (Stage 1d, 2026-09-05)

Word wrap with its toggle · the checkpoint beside the document with its sidecar, the restore and
the twin · the resume tick · the hash cache · free VRAM on every judgment row and the status line ·
torn-row recovery · a crash that names itself. The ROADMAP's Stage 1d entry has the numbers. What
follows is what is still open.

## Still open from Stage 3 (2026-09-05)

- **The seats perseverate, and only the manners stop them.** In a long session a seat keeps
  re-proposing the line it already said, at every later boundary; one driver run composed fourteen
  sentences, said six and refused eight, and the eight were the same two lines. The harness is
  doing its job; the disposition is not. Say-it-once is a fine-tune target (the estate's position
  since 2026-08-12), and nib now has its own measurement of the rate to tune against.
- **The re-form after an abort does not exist.** A killed sentence is not retried; the seat simply
  keeps its want for the next boundary. K5's `pivot` — re-fork from the new trunk, re-decode the
  formed prefix, continue from the point of divergence — is the interesting version and belongs
  after the simple kill has been lived with (the review's §5.9).
- **`settled_by_world` has never fired in a measured run,** only `margin_flipped`. The acceptance
  detector's phrase list is small and a human's real acceptance is usually paraphrased. Worth
  sweeping the tapes offline before widening it, because a false acceptance silences a seat.

## Still open from Stage 2 (2026-09-05)

- **The floor is global, not per block.** The wire refuses to compose while the hand has moved
  anywhere in the document, where the spec says "a block that has received a human keystroke".
  Stricter than required and therefore safe, but it means writing in one paragraph silences a seat
  that wanted to speak about another. The commit-time gate already tests the span precisely; the
  compose-time gate should use the paragraph once Stage 3's dep test exists.

## Still open from Stage 1d

- **The driver's windows sit on the operator's screen** (no-activate, but shown; the screenshot of
  2026-09-05 is one of them). Place them off-screen rather than hide them: a hidden window gets no
  `WM_PAINT`, and the latency instrument measures keystroke to painted.
- **The fold at prefill speed**, for the first switch-on over a document and for the twin. The
  checkpoint means the fold is normally only the world since it, so this is no longer on the hot
  path, but a first fold over a long document still costs 39 s for 4.2 KB at word grain.
- **The `.prev` generation is written but never read.** `Resident::checkpoint` keeps it and
  `Resident::start` does not fall back to it when the current one fails to load (K5 does). One
  more `try_load` and a `boot_reason` of `restored_prev`; needs a fault-injection test to be worth
  anything, which is K5's T6.
- **The trunk is not bounded.** Nothing prunes `<doc>.trunk.bin` (58.8 MB a document) or the tape.
  A week of real work will say what that costs; Stage 5 is where it matters.

## Later

- **A string-bearing frame instead of auricle's fixed Delta** (SPEC 5.1.1, 5.1.7). K5 abandoned the
  496-byte payload and the 15-character lane; nib's chunking, lane check and lockstep meta ring
  exist only to work around them. At Stage 2, when the frame gains a `grain`.
- **The T sweep** (SPEC 14.3). Needs a real typing tape; the synthetic cadence of `--ingest` and
  `--resident` never pauses, so T never fires.
- **The `Hand` table** (review §5.8): one table for authors, lanes, colours and the self-echo set.
  When the seats write, Stage 2.
- **A lane-contract v0.1 spool beside the document**, K5's intake format
  (`t_mono_ns \t lane \t grain \t text` under a `#lane-contract` header), projected from the
  tape's percept rows, so an external kernel can watch a pad. Act II.
- **`nib --verify` reading K5's chain** (`h = blake2b(prev_hex ‖ body)`) beside the family's, so
  one verifier walks every tape on this machine.
- **The consistency fence** (review §8): a selftest that pairs every count, version and date across
  README, SPEC, ROADMAP, HANDOFF and the devlog.
- **Three brainstorms of 2026-09-05, filed in `docs/BRAINSTORMS_2026-09-05.md`:** voice as a
  second lane (the null is two typed lanes into one trunk, a day's work); the two-gear escalation
  (composition, not judgment; dispatched at the want and deadlined by the floor; the dialler
  outside the exe; the exact payload on the tape; behind rule 9's flag and badge); vision as a floor
  sensor and not a content lane (detector events that ride like ticks and never trigger; frames
  never on the tape; window focus as the zero-hardware null). Their order across all three: fix
  say-it-once first, then focus-as-a-lane, then two typed lanes, then gear 2, then speech, then
  vision — and none of it before Stage 4 and the week of real work.

## On the checklist by the operator's word (2026-09-05, evening)

- **The screen saver — the resident holds the floor** (`docs/BRAINSTORMS_2026-09-05.md` §4). An
  Easter-egg mode in which the resident keeps talking on its own line about whatever is in the
  pad, and the human's typing elsewhere is the interruption: the seat ignores it (the re-probe
  holds), pauses to watch (the re-probe flips and the sentence dies), or keeps going and changes
  the subject on its next sentence (composed off a trunk that now holds the human's words). Nearly
  all of it exists — the Stage 3 seam, the forming plane, own speech through the document. Missing:
  a want that renews itself after the seat's own sentence, and the per-block floor (Stage 2's open
  item). The standing instruction arrives as world (`[bo] Watcher, talk to me about anything until
  I interrupt.`), never as a prompt, because the mandates are inside the serve hash. It is a
  switch on the tape, not a timer. It is also an instrument: the monologue horizon — sentences said
  before the manners refuse everything — is a number the next tune is scored on. The null is the
  CLI, `--resident --emit --saver`, an evening; the window version is the third position of
  Stage 4's switch. Not before Stage 4.

## For the estate, not for nib (read in K5 on 2026-09-05; the kernel is in flux, so noted, not filed)

- K5 loads backends with `ggml_backend_load_all_from_path` (fusord.cpp:1395), which pulls
  `ggml-rpc.dll` and with it ws2_32 into its process; its header says the kernel never opens a
  socket. nib's `load_backends` and `module_gate` in `src/resident.cpp` close that by name.
- Two chain formats exist now: K5's `prev`/`h` rows over the raw body, and the family's six-key
  REGISTRAR rows that `glance --verify` reads. Neither verifier reads the other's tape.
- K5's smoke drivers (`converge/smoke/drive.py`) crashed on a missing header row in T5 and T6, and
  T3 timed out at boot; none of the three completed as a test on 2026-09-05.
