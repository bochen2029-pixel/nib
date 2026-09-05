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

- **The fold does not know who wrote what.** `fold_log` ignores `Rev.author` and `fold_tape` never
  reads the row's `author`; both replay every revision on the hand's lane. A seed or twin fold over
  a document that already holds `[SKEPTIC] …` blocks feeds the trunk `[bo] [SKEPTIC] …` — the human
  saying the seat's line — and a restore hits it too, because the rows after the checkpoint include
  the emissions committed at stop. A serve-format drift the tune never saw, and an authorship error
  in what the mind perceives. **Design (decided this evening, below): own speech reaches the trunk
  through the document, as a percept of its own kind, exactly once, live or folded.**
- **Emissions composed at stop never reach the trunk before the checkpoint** (`Wire::run`'s stop
  path composes the remaining wants after `finish()` has flushed own speech, then checkpoints), and,
  worse, **the trunk hears a line the editor refused**: `speak_wants` queues every allowed line for
  the trunk before the editor's second floor gate has ruled, so a `refused {why: floor}` line — two
  in the very first driver run — is on the trunk and in the manners' memory while the document
  never received it. The mind believes it said something nobody saw. Both are the same defect as
  the one above, and the same design closes all three: the resident's line joins the trunk only
  when the document has it, carried back through the pad on the seat's lane as an own-speech
  percept that is decoded raw and never judged; the fold replays seat-authored revisions the same
  way; a checkpoint's cursor then advances past the block's revision because the percept carries
  it, so a restore can never replay a line the trunk already holds. What the thread still commits
  on its own is the aired prefix of an abort, which is never a document revision, flushed before
  every checkpoint.
- **A block inserted above the caret is not seen by the compiler's pending span**, so the next
  keystroke reads as a jump and closes the clause early. Cannot happen while `floor_ms` (2000)
  exceeds `quiet_ms` (500), because the pending clause was flushed by the quiet before any seat may
  compose; the own-speech percept above pushes the pending clause first anyway, which closes it for
  any configuration.
- **SPEC 6.2.11.3 promises a refusal when the checkpoint's tape row is in no tape;** `fold_on_ready`
  degrades to a diff-only fold with an `err` field on the `fold` row instead. `decide_restore`
  should walk the tapes before the model loads and refuse into the twin, as the clause says.
- **The first sentence after a pause is judged at a timeout boundary at its sixth token** (found
  in the un-say window's log during 0.10.1's battery: `judgment 3 SKEPTIC 5.24 t "Actually the
  staging database is mysql"`, then `b` on "5", then `b` on the rest — three boundaries and nine
  probes for one sentence). The resident's 1500 ms flush clock runs from the last judgment; in a
  pad a percept arrives whole and is decoded in one burst, so the clock has always already expired
  when a percept begins after a gap, and the `t` path fires on the first six tokens of every
  sentence after every pause and never on a stalled clause. The compiler's `quiet_ms` is the stall
  timeout at the right grain. Start the clock at the percept; measure the CLI's margins run before
  and after (boundaries and probes move, margins do not).
- **A clause whose judgment includes a deletion percept can carry a span whose end precedes its
  start** (`step` takes `span_b` from a deletion's empty span). Edge; a deletion closes its own
  clause in every measured run.
- Doc drift found and corrected the same evening: SPEC §6's header still said emission and
  un-saying were structurally absent; SPEC 11.1 said 201 checks; SPEC 8.1.2 said there was no emit
  path yet; the handoff counted its own commit out.

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

- **The manners do not survive the switch, though the trunk does.** A restored resident has its own
  lines on its trunk (the self-echo commit) but an empty suppression ladder, so a seat can repeat
  itself once after the AI switch is flipped. Seen in the driver's own tape: the SKEPTIC said its
  Pacific line in two lives, in two phrasings. The fix is `last_say`, `last_clause` and the
  resolved flags in the checkpoint's sidecar, and a wire accessor to read them out of the resident.
  Cheap, and it should land before the week of real work.
- **`emit` has no key.** It is a theme setting; the AI switch has `Ctrl+Shift+A` and wrap has
  `Alt+Z`. Stage 4 gives it one, because a toggle that is flipped during real work is a paired
  sample and belongs on the status line and the tape like the others.
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

## For the estate, not for nib (read in K5 on 2026-09-05; the kernel is in flux, so noted, not filed)

- K5 loads backends with `ggml_backend_load_all_from_path` (fusord.cpp:1395), which pulls
  `ggml-rpc.dll` and with it ws2_32 into its process; its header says the kernel never opens a
  socket. nib's `load_backends` and `module_gate` in `src/resident.cpp` close that by name.
- Two chain formats exist now: K5's `prev`/`h` rows over the raw body, and the family's six-key
  REGISTRAR rows that `glance --verify` reads. Neither verifier reads the other's tape.
- K5's smoke drivers (`converge/smoke/drive.py`) crashed on a missing header row in T5 and T6, and
  T3 timed out at boot; none of the three completed as a test on 2026-09-05.
